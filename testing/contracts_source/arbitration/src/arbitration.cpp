/**
 * Arbitration Contract Implementation. See function bodies for further notes.
 * 
 * @author Roger Taulé, Craig Branscom, Peter Bue, Ed Silva
 * @copyright defined in telos/LICENSE.txt
 */

#include "../include/arbitration.hpp"
#include "randomness_provider.cpp"

#pragma region Config_Actions

void arbitration::init(name initial_admin) {
   
    //authenticate
    require_auth(get_self());

    //open config singleton
    config_singleton configs(get_self(), get_self().value);

    //validate
    check(!configs.exists(), "contract already initialized");
    check(is_account(initial_admin), "initial admin account doesn't exist");

    //initialize
    config initial_conf;
    initial_conf.admin = initial_admin;
		initial_conf.contract_version = "0.1.0";

    //set initial config
    configs.set(initial_conf, get_self());
}

void arbitration::setadmin(name new_admin) {
    //open config singleton, get config
    config_singleton configs(get_self(), get_self().value);
    auto conf = configs.get();

    //authenticate
    require_auth(conf.admin);

    //validate
    check(is_account(new_admin), "new admin account doesn't exist");

    //change admin
    conf.admin = new_admin;

    //set new config
    configs.set(conf, get_self());
}

void arbitration::setversion(string new_version)
{
    //open config singleton, get config
    config_singleton configs(get_self(), get_self().value);
    auto conf = configs.get();

    //authenticate
    check(has_auth(conf.admin) || has_auth(get_self()), "Only admin and SC account can change the version");

    //change contract version
    conf.contract_version = new_version;

    //set new config
    configs.set(conf, get_self());
}

void arbitration::setconfig(uint8_t max_claims_per_case, asset fee_usd)
{
	//open config singleton, get config
    config_singleton configs(get_self(), get_self().value);
    auto conf = configs.get();

	//authenticate
	require_auth(conf.admin);

	//Configuration checks
	check(max_claims_per_case > 0, "Minimum 1 claim");
	check(fee_usd.symbol == USD_SYM, "Fee must be set in USD");
	conf.max_claims_per_case = max_claims_per_case;
	conf.fee_usd = fee_usd;

	//set new config
    configs.set(conf, get_self());
}


#pragma endregion Config_Actions

#pragma region Claimant_Actions
void arbitration::withdraw(name owner)
{	
	//authenticate
	require_auth(owner);

	//open accounts table, get balance for the owner
	accounts_table accounts(get_self(), owner.value);
	const auto &bal = accounts.get(TLOS_SYM.code().raw(), "balance does not exist");

	//Transfer funds from the Smart Contract to the owner
	action(permission_level{get_self(), name("active")}, name("eosio.token"), name("transfer"),
		   make_tuple(get_self(),
					  owner,
					  bal.balance,
					  std::string("Telos Arbitration withdrawal")))
		.send();

	accounts.erase(bal);
}

void arbitration::filecase(name claimant, string claim_link, std::optional<name> respondant, name arbitrator, uint8_t claim_category)
{
	//authenticate
	require_auth(claimant);

	//Check that tha claim_link is a valid IPFS Hash
	validate_ipfs_url(claim_link);

	//If a respondant is added, need to check that it is a valid account
	if(respondant) {
		check(is_account(*respondant), "Respondant must be an account");
	}

	check(is_account(arbitrator), "Arbitrator must be an account");

	//Check that the claim category is valid
	check(claim_category <= claim_category::MISC && claim_category >= claim_category::LOST_KEY_RECOVERY, "Claim category not found");

	//open casefiles table
	casefiles_table casefiles(get_self(), get_self().value);
	uint64_t new_case_id = casefiles.available_primary_key();

	//Create a new case file
	casefiles.emplace(claimant, [&](auto &col) {
		col.case_id = new_case_id;
		col.case_status = static_cast<uint8_t>(case_status::CASE_SETUP);
		col.claimant = claimant;
		col.respondant = *respondant;
		col.arbitrator = arbitrator;
		col.approvals = {};
		col.number_claims = 1;
		col.case_ruling = std::string("");
		col.update_ts = time_point_sec(current_time_point());
	});

	//open claims table
	claims_table claims(get_self(), new_case_id);
	uint64_t new_claim_id = claims.available_primary_key();

	//Create a new claim
	claims.emplace(claimant, [&](auto& col){
		col.claim_id = new_claim_id;
		col.claim_summary = claim_link;
		col.claim_category = claim_category;
		col.status = static_cast<uint8_t>(claim_status::FILED);
	});
}

void arbitration::addclaim(uint64_t case_id, string claim_link, name claimant, uint8_t claim_category)
{
	//authenticate
	require_auth(claimant);

	//Check that tha claim_link is a valid IPFS Hash
	validate_ipfs_url(claim_link);

	//open config singleton, get config
    config_singleton configs(get_self(), get_self().value);
    auto conf = configs.get();

	//open casefile tables and checks that the case exists
	casefiles_table casefiles(get_self(), get_self().value);
	const auto& cf = casefiles.get(case_id, "Case Not Found");

	//Check that the claim category is valid
	check(claim_category <= claim_category::MISC && claim_category >= claim_category::LOST_KEY_RECOVERY, "Claim category not found");
	
	//Only the claimant can add new claims, the number of claims cannot exceed the maximum permitted and case must be in setup
	check(cf.case_status == case_status::CASE_SETUP, "claims cannot be added after CASE_SETUP is complete.");
	check(cf.number_claims < conf.max_claims_per_case, "case file has reached maximum number of claims");
	check(claimant == cf.claimant, "you are not the claimant of this case.");

	//open claims table
	claims_table claims(get_self(), case_id);

	//Check that two different claims doesn't have the same link
	for(auto itr = claims.begin(); itr != claims.end(); ++itr) {
		check(itr->claim_summary != claim_link, "Claim Link already exists in another claim");
	}

	//Creates a new claim
	uint64_t new_claim_id = claims.available_primary_key();
	claims.emplace(claimant, [&](auto& col){
		col.claim_id = new_claim_id;
		col.claim_summary = claim_link;
		col.claim_category = claim_category;
		col.status = static_cast<uint8_t>(claim_status::FILED);
	});
	
	//Update casefile table
	casefiles.modify(cf, same_payer, [&](auto &col) {
		col.number_claims += 1;
		col.update_ts = time_point_sec(current_time_point());
	});
}

void arbitration::updateclaim(uint64_t case_id, uint64_t claim_id, name claimant, string claim_link)
{
	//authenticate
	require_auth(claimant);

	//Check that tha claim_link is a valid IPFS Hash
	validate_ipfs_url(claim_link);

	//open casefile tables and checks that the case exists
	casefiles_table casefiles(get_self(), get_self().value);
	const auto& cf = casefiles.get(case_id, "Case not found");

	//Only the claimant can update the claim. A claim can only be updated in case investigation
	//or case setup
	check(cf.claimant == claimant, "must be the claimant of this case_id");
	check(cf.case_status == case_status::CASE_INVESTIGATION 
		|| cf.case_status == case_status::CASE_SETUP, "case status does NOT allow responses at this time");

	//open claims table and checks that the claim exists
	claims_table claims(get_self(), case_id);
	auto claim_it = claims.find(claim_id);
	check(claim_it != claims.end(), "Claim not found");

	//To update a claim, claim_status cannot be accepted nor declined
	check(claim_it->status == claim_status::FILED || 
		(claim_it->claim_info_needed && claim_it->status == claim_status::RESPONDED), "Claim cannot be updated");

	//Update a claim
	claims.modify(claim_it, get_self(), [&](auto& col){
		col.claim_summary = claim_link;
		col.claim_info_needed = false;
	});	
}

void arbitration::removeclaim(uint64_t case_id, uint64_t claim_id, name claimant)
{	
	//authenticate
	require_auth(claimant);

	//open casefile tables and checks that the case exists
	casefiles_table casefiles(get_self(), get_self().value);
	const auto& cf = casefiles.get(case_id, "Case Not Found");

	//Only the claimant can remove a claim and case must be in setup status to do so
	check(cf.case_status == case_status::CASE_SETUP, "Claims cannot be removed after CASE_SETUP is complete");
	check(claimant == cf.claimant, "you are not the claimant of this case.");
	
	//open claims table and checks that the claim exists
	claims_table claims(get_self(), case_id);
	auto claim_it = claims.find(claim_id);
	check(claim_it != claims.end(), "Claim not found");

	//erase the claim
	claims.erase(claim_it);

	//Update casefile table
	casefiles.modify(cf, same_payer, [&](auto &col) {
		col.update_ts = time_point_sec(current_time_point());
		col.number_claims -= 1;
	});
}

void arbitration::shredcase(uint64_t case_id, name claimant)
{	
	//authenticate
	require_auth(claimant);

	//open casefile tables and checks that the case exists
	casefiles_table casefiles(get_self(), get_self().value);
	auto c_itr = casefiles.find(case_id);
	check(c_itr != casefiles.end(), "Case Not Found");

	//Only the claimant can shred the case and it must be in setup status to do so
	check(claimant == c_itr->claimant, "you are not the claimant of this case.");
	check(c_itr->case_status == case_status::CASE_SETUP, "cases can only be shredded during CASE_SETUP");
	
	//Open the claims table for the case and erase all the claims
	claims_table claims(get_self(), case_id);
	auto claim_it = claims.begin();
	while(claim_it != claims.end()) {
		claim_it = claims.erase(claim_it);
	}

	//erase the case
	casefiles.erase(c_itr);
}

void arbitration::readycase(uint64_t case_id, name claimant)
{	
	//open config singleton, get config
    config_singleton configs(get_self(), get_self().value);
    auto conf = configs.get();

	//authenticate 
	require_auth(claimant);

	//open casefile tables and checks that the case exists
	casefiles_table casefiles(get_self(), get_self().value);
	const auto& cf = casefiles.get(case_id, "Case Not Found");

	//To ready a case, it has to be done by the claimant, the case must have at least a claim and must be in setup status
	check(cf.case_status == case_status::CASE_SETUP, "Cases can only be readied during CASE_SETUP");
	check(claimant == cf.claimant, "you are not the claimant of this case.");
	check(cf.number_claims >= 1, "Cases must have at least one claim");
		
	configs.set(conf, get_self());

	//Update casefile table
	casefiles.modify(cf, get_self(), [&](auto &col) {
		col.case_status = static_cast<uint8_t>(case_status::AWAITING_ARB_ACCEPT);
		col.update_ts = time_point_sec(current_time_point());
	});
}

void arbitration::cancelcase(uint64_t case_id) {

	//open config singleton, get config
	config_singleton configs(get_self(), get_self().value);
	auto conf = configs.get();

	//open casefile tables and checks that the case exists
	casefiles_table casefiles(get_self(), get_self().value);
	const auto& cf = casefiles.get(case_id, "Case not found");

	//A case can only be canceled while it is in awaiting arbs status
	check(cf.case_status == case_status::ARB_ASSIGNED, "Case status must be in ARB_ASSIGNED stage");

	//authenticate
	require_auth(cf.claimant);
		
	//update casefiles status
	casefiles.modify(cf, same_payer, [&](auto& col){
		col.case_status = static_cast<uint8_t>(case_status::CANCELLED);
		col.update_ts = time_point_sec(current_time_point());
	});

	//Substract the fee paid by the claimant from the reserved funds
	conf.reserved_funds -= cf.fee_paid_tlos;
};

#pragma endregion Claimant_Actions

#pragma region Respondant_Actions

void arbitration::acceptarb(name respondant, uint64_t case_id)
{
	//authenticate
	require_auth(respondant);

	//open casefile tables and checks that the case exists
	casefiles_table casefiles(get_self(), get_self().value);
	const auto& cf = casefiles.get(case_id, "Case not found");

	//Only respondant can add a response to a claim, and a response can only be added during case investigation status
	check(cf.respondant != name(0), "case_id does not have a respondant");
	check(cf.respondant == respondant, "must be the respondant of this case_id");
	check(cf.case_status == case_status::CASE_SETUP, "case status does NOT allow respondant approval of arbitrator at this time");	

	casefiles.modify(cf, get_self(), [&](auto &col) {
		col.case_status = static_cast<uint8_t>(case_status::AWAITING_ARB_ACCEPT);
		col.update_ts = time_point_sec(current_time_point());
	});
}

void arbitration::respond(uint64_t case_id, uint64_t claim_id, name respondant, string response_link)
{	
	//authenticate
	require_auth(respondant);

	//Check that tha response_link is a valid IPFS Hash
	validate_ipfs_url(response_link);

	//open casefile tables and checks that the case exists
	casefiles_table casefiles(get_self(), get_self().value);
	const auto& cf = casefiles.get(case_id, "Case not found");

	//Only respondant can add a response to a claim, and a response can only be added during case investigation status
	check(cf.respondant != name(0), "case_id does not have a respondant");
	check(cf.respondant == respondant, "must be the respondant of this case_id");
	check(cf.case_status == case_status::CASE_INVESTIGATION, "case status does NOT allow responses at this time");

	//open claims tables and checks that the claim exists
	claims_table claims(get_self(), case_id);
	auto claim_it = claims.find(claim_id);
	check(claim_it != claims.end(), "Claim not found");

	//To respond a claim, a response information needs to be asked by the arbitrator and the claim cannot be resolved yet
	check(claim_it->status == claim_status::FILED || claim_it->status == claim_status::RESPONDED , "Claim must be in FILED status");
	check(claim_it->response_info_needed, "No response needed");

	//Update claim
	claims.modify(claim_it, get_self(), [&](auto& col){
		col.response_link = response_link;
		col.status = static_cast<uint8_t>(claim_status::RESPONDED);
		col.response_info_needed = false;
	});	
}


#pragma endregion Respondant_Actions

#pragma region Case_Actions

void arbitration::startcase(uint64_t case_id, name assigned_arb, uint8_t number_days_respondant, string response_info_required) {

	//authenticate
	require_auth(assigned_arb);

	//open casefile tables and checks that the case exists
	casefiles_table casefiles(get_self(), get_self().value);
	const auto& cf = casefiles.get(case_id, "Case not found");

	//Check that the arbitrator is assigned to the case
	check(assigned_arb == cf.arbitrator, "Only an assigned arbitrator can start a case");
	
	//Check that the case in arbs assigned status
	check(cf.case_status == case_status::ARB_ASSIGNED, "Case status must be in ARB_ASSIGNED");
	
	//Update casefile
	casefiles.modify(cf, get_self(), [&](auto &col) {
		col.case_status = static_cast<uint8_t>(case_status::CASE_INVESTIGATION);
		col.update_ts = time_point_sec(current_time_point());
	});

	//If there is a respondant, initialize all claims with a response limit time. If no response has 
	//been provided after that time, the arbitrator will be able to set a decision for the claim
	if(cf.respondant != name(0)) {
		claims_table claims(get_self(), case_id);
		for(auto claim_it = claims.begin(); claim_it != claims.end(); ++claim_it) {
			claims.modify(claim_it, get_self(), [&](auto& col){
				col.response_info_needed = true;
				col.response_info_required = response_info_required;
				col.respondant_limit_time = time_point_sec(current_time_point().sec_since_epoch() + number_days_respondant*86400);
			});	
		}
	}
	
	//Update the open cases for the corresponding arbitrator

}

void arbitration::reviewclaim(uint64_t case_id, uint64_t claim_id, name assigned_arb, 
	bool claim_info_needed, string claim_info_required, bool response_info_needed, 
	string response_info_required, uint8_t number_days_claimant, uint8_t number_days_respondant){
	
	//authenticate
	require_auth(assigned_arb);

	//open casefile tables and checks that the case exists
	casefiles_table casefiles(get_self(), get_self().value);
	const auto& cf = casefiles.get(case_id, "Case not found");

	//Check that the arbitrator is assigned to the case
	check(assigned_arb == cf.arbitrator, "Only an assigned arbitrator can review a claim");

	//Check that the case in investigation status
	check(cf.case_status == case_status::CASE_INVESTIGATION, "To review a claim, case should be in investigation status");

	//open claim tables and checks that the claim exists
	claims_table claims(get_self(), case_id);
	auto claim_it = claims.find(claim_id);
	check(claim_it != claims.end(), "Claim not found");

	//Check that the claim has not been settled yet and at least extra information is being asked from either the claimant or the respondant
	check(claim_it->status == claim_status::FILED || claim_it->status != claim_status::RESPONDED, "Claim status needs to be filed or responded to review");
	check(claim_info_needed || response_info_needed, "Need to update at least respondant or claimant");
	check(!claim_info_needed || number_days_claimant > 0, "At least one day has to be given to the claimant");
	check(!response_info_needed || number_days_respondant > 0, "At least one day has to be given to the respondant");

	//Update claim
	claims.modify(claim_it, get_self(), [&](auto& col) {
		if(claim_info_needed) {
			col.claim_info_needed = true;
			col.claim_info_required = claim_info_required;
			col.claimant_limit_time = time_point_sec(current_time_point().sec_since_epoch() + number_days_claimant*86400);
		}

		if(response_info_needed) {
			col.response_info_needed = true;
			col.response_info_required = response_info_required;
			col.respondant_limit_time = time_point_sec(current_time_point().sec_since_epoch() + number_days_respondant*86400);
		}
	});

};


void arbitration::settleclaim(uint64_t case_id, name assigned_arb, uint64_t claim_id, bool accept, string decision_link)
{	
	//authenticate
	require_auth(assigned_arb);

	//Check that the decision is a valid IPFS Hash
	validate_ipfs_url(decision_link);

	//open casefile tables and checks that the case exists
	casefiles_table casefiles(get_self(), get_self().value);
	const auto& cf = casefiles.get(case_id, "Case not found");

	//Check that the case in investigation status
	check(cf.case_status == case_status::CASE_INVESTIGATION, "To settle a claim, case should be in investigation status");

	//Check that the arbitrator is assigned to the case
	check(assigned_arb == cf.arbitrator, "Only an assigned arbitrator can settle a claim");

	//open claim tables and checks that the claim exists
	claims_table claims(get_self(), case_id);
	auto claim_it = claims.find(claim_id);
	check(claim_it != claims.end(), "Claim not found");

	auto now = time_point_sec(current_time_point());

	//If there is a respondant, check if it still has time to provide a response. If so, the claim can not be settled
	if(cf.respondant != name(0)) {
		check((!claim_it->response_info_needed  || (claim_it->response_info_needed && claim_it->respondant_limit_time <= now)), 
			"Respondant still have time to respond");
	}

	//If extra information was asked to the claimant, check if it still has time to provide it. If so, the claim can not be settled
	check((!claim_it->claim_info_needed  || (claim_it->claim_info_needed && claim_it->claimant_limit_time <= now)), 
			"Claimant still have time to respond");
	
	//Update claim
	claims.modify(claim_it, get_self(), [&](auto& col){
		col.decision_link = decision_link;
		if(accept) {
			col.status = static_cast<uint8_t>(claim_status::ACCEPTED);
		} else {
			col.status = static_cast<uint8_t>(claim_status::DISMISSED);
		}
	});	
}

void arbitration::setruling(uint64_t case_id, name assigned_arb, string case_ruling) {
	//authenticate
	require_auth(assigned_arb);

	//Check that the case ruling is a valid IPFS Hash
	validate_ipfs_url(case_ruling);

	//open casefile tables and checks that the case exists
	casefiles_table casefiles(get_self(), get_self().value);
	const auto& cf = casefiles.get(case_id, "Case not found");

	//Check that the case in investigation status
	check(cf.case_status == case_status::CASE_INVESTIGATION, "Case status must be CASE INVESTIGATION");

	//Check that all the claims has been resolved. If there's any claim left, the ruling can not be set
	check(all_claims_resolved(case_id), "There are claims that has not been resolved");
	
	//Check that the arbitrator is assigned to the case
	check(assigned_arb == cf.arbitrator, "Only an assigned arbitrator can set a ruling");

	//Update casefile
	casefiles.modify(cf, same_payer, [&](auto& col) {
		col.case_ruling = case_ruling;
		col.case_status = static_cast<uint8_t>(case_status::DECISION);
		col.update_ts = time_point_sec(current_time_point());
	});

	// notify_bp_accounts();
}

#pragma region Arb_Actions
void arbitration::arbacceptnom(name arbitrator, uint64_t case_id)
{
	//authenticate
	require_auth(arbitrator);

	//open casefile tables and checks that the case exists
	casefiles_table casefiles(get_self(), get_self().value);
	const auto& cf = casefiles.get(case_id, "Case not found");

	//Only respondant can add a response to a claim, and a response can only be added during case investigation status
	check(cf.arbitrator == arbitrator, "must be the respondant of this case_id");
	check(cf.case_status == case_status::AWAITING_ARB_ACCEPT, "case status does NOT allow arbitrator accepting case at this time");	

	casefiles.modify(cf, get_self(), [&](auto &col) {
		col.case_status = static_cast<uint8_t>(case_status::ARB_ASSIGNED);
		col.update_ts = time_point_sec(current_time_point());
	});
}
#pragma endregion Arb_Actions

#pragma endregion Case_Actions

#pragma region BP_Actions

void arbitration::closecase(uint64_t case_id) {
	//open config table, get config
	config_singleton configs(get_self(), get_self().value);
	auto conf = configs.get();

	//authenticate
	require_auth(conf.admin);

	//open casefile tables and checks that the case exists
	casefiles_table casefiles(get_self(), get_self().value);
	const auto& cf = casefiles.get(case_id, "Case not found");

	//Check that the case is in enforcement status
	check(cf.case_status == case_status::ENFORCEMENT, "Case status must be ENFORCEMENT");

	//Update casefile
	casefiles.modify(cf, same_payer, [&](auto& col) {
		col.case_status = static_cast<uint8_t>(case_status::RESOLVED);
		col.update_ts = time_point_sec(current_time_point());
	});
}

void arbitration::validatecase(uint64_t case_id, bool proceed)
{	
	//open config table, get config
	config_singleton configs(get_self(), get_self().value);
	auto conf = configs.get();

	//authenticate
	require_auth(conf.admin);

	//open casefile tables and checks that the case exists
	casefiles_table casefiles(get_self(), get_self().value);
	const auto& cf = casefiles.get(case_id, "Case not found");

	//Check that the case is in decision status
	check(cf.case_status == case_status::DECISION, "Case must be in DECISION status");

	//BPs can decide either to proceed or dismiss the case if they consider that the arbitrator ruling isn't valid
	if(proceed) {
		//Remove the arbitrator cost and the fee from reserved funds, and add the fee to available funds.
		conf.reserved_funds -= cf.fee_paid_tlos;
		conf.available_funds += cf.fee_paid_tlos;
		configs.set(conf, get_self());

	} else {
		//If the case is considered not valid, return the fee paid and the arbitrator 
		//rate cost to the claimant, since the case is considered mistrial
		auto tlos_returned = cf.fee_paid_tlos;
		add_balance(cf.claimant, tlos_returned, get_self());

		//Remove the telos returned from reserved funds
		conf.reserved_funds -= tlos_returned;
		configs.set(conf, get_self());
	}

	//Update casefile
	casefiles.modify(cf, same_payer, [&](auto &col) {
		if(proceed) {
			col.case_status = static_cast<uint8_t>(case_status::ENFORCEMENT);
		} else {
			col.case_status = static_cast<uint8_t>(case_status::DISMISSED);
		}
		col.update_ts = time_point_sec(current_time_point());
	});
}

#pragma endregion BP_Actions


#pragma region Helpers

typedef arbitration::claim claim;


void arbitration::validate_ipfs_url(string ipfs_url)
{
	//Check that ipfs_url is a valid link
	check(ipfs_url.find('http') != std::string::npos, 'IPFS URL must include "http"');
}

void arbitration::assert_string(string to_check, string error_msg)
{
	check(to_check.length() > 0 && to_check.length() < 255, error_msg.c_str());
}

bool arbitration::all_claims_resolved(uint64_t case_id) {
	//open claims table for case id
	claims_table claims(get_self(), case_id);

	//check that all the claims has been resolved, either accepted or declined
	for(auto claim_itr = claims.begin(); claim_itr != claims.end(); ++claim_itr) {
		if(claim_itr->status == claim_status::FILED || claim_itr->status == claim_status::RESPONDED) {
			return false;
		}
	}

	return true;
}


void arbitration::set_permissions(vector<permission_level_weight> &perms) {
	//review update auth permissions and weights.
	if (perms.size() > 0)
	{
		sort(perms.begin(), perms.end(), [](const auto &first, const auto &second) 
			{ return first.permission.actor.value < second.permission.actor.value; });

		uint32_t weight = perms.size() > 3 ? (((2 * perms.size()) / uint32_t(3)) + 1) : 1;

		action(permission_level{get_self(), "active"_n}, "eosio"_n, "updateauth"_n,
				std::make_tuple(
					get_self(),
					"major"_n,
					"active"_n,
					authority{
						weight,
						std::vector<key_weight>{},
						perms,
						std::vector<wait_weight>{}}))
			.send();
	}
}


void arbitration::sub_balance(name owner, asset value){
	accounts_table from_acnts(_self, owner.value);

	const auto &from = from_acnts.get(value.symbol.code().raw(), "no balance object found");
	check(from.balance.amount >= value.amount, "overdrawn balance");

	if (from.balance - value == asset(0, value.symbol))
	{
		from_acnts.erase(from);
	}
	else
	{
		from_acnts.modify(from, owner, [&](auto &a) {
			a.balance -= value;
		});
	}
}

void arbitration::add_balance(name owner, asset value, name ram_payer){
	accounts_table to_acnts(_self, owner.value);
	auto to = to_acnts.find(value.symbol.code().raw());
	if (to == to_acnts.end())
	{
		to_acnts.emplace(ram_payer, [&](auto &a) {
			a.balance = value;
		});
	}
	else
	{
		to_acnts.modify(to, same_payer, [&](auto &a) {
			a.balance += value;
		});
	}
}

checksum256 arbitration::get_rngseed(uint64_t seed)
{
  auto trxsize = transaction_size();
  char* trxbuf = (char*) malloc(trxsize);   // no need to free as it's executed once and VM gets destroyed
  uint32_t trxread = read_transaction( trxbuf + 8, trxsize );
  check( trxsize == trxread, "read_transaction failed");
  *((uint64_t*)trxbuf) = seed;
  return sha256(trxbuf, trxsize + 8);
}


//Get Random Number (based in WAX RNG contract example)
inline uint64_t get_rand() {
  auto size = transaction_size();
  char buf[size];

  auto read = read_transaction(buf, size);
  check(size == read, "read_transaction() has failed.");

  checksum256 tx_id = sha256(buf, size); 
  uint64_t signing_value;
  memcpy(&signing_value, tx_id.data(), sizeof(signing_value)); //Converting checksum256 to uint64_t
  return signing_value;
}

string arbitration::get_rand_ballot_name() {
	RandomnessProvider randomness_provider(get_rngseed(get_rand()));

	string ballot_name = "";

	string possibleCharacters = string("12345abcdefghijklmnopqrstuvwxyz");
	for(auto i = 0; i < 12; ++i) {
		uint32_t rand = randomness_provider.get_rand(possibleCharacters.length() - 1); 
		ballot_name += possibleCharacters[rand];
	}

	return ballot_name;
}


#pragma endregion Helpers
