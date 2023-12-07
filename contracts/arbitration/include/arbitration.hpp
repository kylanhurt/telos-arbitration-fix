/**
 * Arbitration Contract Interface
 *
 * @author Roger Taulé, Craig Branscom, Peter Bue, Ed Silva, Douglas Horn
 * @copyright defined in telos/LICENSE.txt
 */

#pragma once
#include <eosio/action.hpp>
#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/permission.hpp>
#include <eosio/singleton.hpp>
#include "eosiosystem-interface.hpp"

using namespace std;
using namespace eosio;
using namespace eosiosystem;

CONTRACT arbitration : public eosio::contract
{

public:
	using contract::contract;
	static constexpr symbol TLOS_SYM = symbol("TLOS", 4);
	static constexpr symbol VOTE_SYM = symbol("VOTE", 4);
	static constexpr symbol USD_SYM = symbol("USD", 4);	

#pragma region Enums

	// TODO: describe each enum in README

	// kylan: adjust
	// enum class case_status : uint8_t
	// {
	// 	CASE_SETUP = 0,
	// 	AWAITING_ARBS = 1,
	// 	ARB_ASSIGNED = 2,
	// 	CASE_INVESTIGATION = 3,
	// 	DECISION = 4,
	// 	ENFORCEMENT = 5,
	// 	RESOLVED = 6,
	// 	DISMISSED = 7,
	// 	CANCELLED = 8,
	// 	MISTRIAL = 9,
	// };

	enum class case_status : uint8_t
	{
		CASE_SETUP = 0,
		AWAITING_ARB_ACCEPT = 1,
		ARB_ASSIGNED = 2,
		CASE_INVESTIGATION = 3,
		DECISION = 4,
		ENFORCEMENT = 5,
		RESOLVED = 6,
		DISMISSED = 7,
		CANCELLED = 8,
		MISTRIAL = 9,
	};	

	friend constexpr bool operator==(const uint8_t &a, const case_status &b)
	{
		return a == static_cast<uint8_t>(b);
	}

	friend constexpr bool operator!=(const uint8_t &a, const case_status &b)
	{
		return a == static_cast<uint8_t>(b);
	}

	friend constexpr bool operator<(const uint8_t &a, const case_status &b)
	{
		return a < static_cast<uint8_t>(b);
	}

	friend constexpr bool operator>(const uint8_t &a, const case_status &b)
	{
		return a > static_cast<uint8_t>(b);
	}

	friend constexpr bool operator>=(const uint8_t &a, const case_status &b)
	{
		return a >= static_cast<uint8_t>(b);
	}

	enum class claim_status : uint8_t
	{
		FILED = 1,
		RESPONDED = 2,
		ACCEPTED = 3,
		DISMISSED = 4,
	};

	friend constexpr bool operator==(const uint8_t &a, const claim_status &b)
	{
		return a == static_cast<uint8_t>(b);
	}

	friend constexpr bool operator!=(const uint8_t &a, const claim_status &b)
	{
		return a == static_cast<uint8_t>(b);
	}

	enum class claim_category : uint8_t
	{
		LOST_KEY_RECOVERY = 1,
		TRX_REVERSAL = 2,
		EMERGENCY_INTER = 3,
		CONTESTED_OWNER = 4,
		UNEXECUTED_RELIEF = 5,
		CONTRACT_BREACH = 6,
		MISUSED_CR_IP = 7,
		A_TORT = 8,
		BP_PENALTY_REVERSAL = 9,
		WRONGFUL_ARB_ACT = 10,
		ACT_EXEC_RELIEF = 11,
		WP_PROJ_FAILURE = 12,
		TBNOA_BREACH = 13,
		MISC = 14
	};

	friend constexpr bool operator>=(const uint8_t &a, const claim_category &b)
	{
		return a >= static_cast<uint8_t>(b);
	}

	friend constexpr bool operator<=(const uint8_t &a, const claim_category &b)
	{
		return a <= static_cast<uint8_t>(b);
	}

#pragma endregion Enums

#pragma region Config_Actions

	// initialize the contract
	// pre: config table not initialized
	// auth: self
	ACTION init(name initial_admin);

	// set new admin
	// pre: new_admin account exists
	// auth: admin
	ACTION setadmin(name new_admin);

	// set contract version
	// auth: admin
	ACTION setversion(string new_version);

	// set configuration parameters
	// auth: admin
	ACTION setconfig(uint8_t max_claims_per_case,
									 asset fee_usd);

#pragma endregion Config_Actions

#pragma region Claimant_Actions

	// Allows the owner to withdraw their funds
	// pre: balance > 0
	// auth: owner
	ACTION withdraw(name owner);

	// Files a new case
	// auth: claimant
	// NOTE: filing a case doesn't require a respondent
	ACTION filecase(name claimant, string claim_link,
									std::optional<name> respondant, name arbitrator, uint8_t claim_category);

	// Adds a claim for an existing case
	// pre: case must be in setup status
	// auth: claimant
	ACTION addclaim(uint64_t case_id, string claim_link, name claimant, uint8_t claim_category);

	// Updates a claim for an existing case
	// pre: case must be in investigation or setup status
	// auth: claimant
	ACTION updateclaim(uint64_t case_id, uint64_t claim_id, name claimant, string claim_link);

	// Remove a claim for an existing case
	// pre: case must be in setup status
	// auth: claimant
	ACTION removeclaim(uint64_t case_id, uint64_t claim_id, name claimant);

	// Remove an existing case
	// pre: case must be in setup status
	// auth: claimant
	ACTION shredcase(uint64_t case_id, name claimant);

	// Set a case as ready to proceed
	// pre: case must be in setup status
	// post: case moves to awaiting arbs stage
	// auth: claimant
	ACTION readycase(uint64_t case_id, name claimant);

	// Cancel a case before accepting any of the arbitators offer
	// pre: case must be in awaiting arbs status
	// auth: claimant
	ACTION cancelcase(uint64_t case_id);

#pragma endregion Claimant_Actions

#pragma region Respondant_Actions
	ACTION acceptarb(name respondant, uint64_t case_id);

	// Allows the respondant to respond to a claim
	// pre: case must be in investigation status
	// auth: respondant
	ACTION respond(uint64_t case_id, uint64_t claim_id, name respondant, string response_link);

#pragma endregion Respondant_Actions

#pragma region Case_Actions

	// Starts the case investigation period
	// pre: case must be in ARB_ASSIGNED status
	// auth: assigned arbitrator
	ACTION startcase(uint64_t case_id, name assigned_arb, uint8_t number_days_respondant, string response_info_required);

	// Ask the respondant and the claimant to provide more information if needed
	// pre: case must be in investigation status
	// auth: assigned arbitrator
	ACTION reviewclaim(uint64_t case_id, uint64_t claim_id, name assigned_arb, bool claim_info_needed,
										 string claim_info_required, bool response_info_needed, string response_info_required,
										 uint8_t number_days_claimant, uint8_t number_days_respondant);

	// Accepts or denies a claim of a particular case
	// pre: case must be in investigation status
	// auth: assigned arbitrator
	ACTION settleclaim(uint64_t case_id, name assigned_arb, uint64_t claim_id, bool accept, string decision_link);

	// After settling all the claims, set a ruling for the whole case
	// pre: case must be in investigation status and all claims settled
	// post: moves the case to decision stage
	// auth: assigned arbitrator
	ACTION setruling(uint64_t case_id, name assigned_arb, string case_ruling);

#pragma endregion Case_Actions

#pragma region BP_Actions

	// Validates that the case and the decision taken by the arbitrator are valid
	// pre: case must be in decision stage
	// post: if not valid, case is considered mistrial. Otherwise, move the case to enforcement stage
	// auth: admin
	ACTION validatecase(uint64_t case_id, bool proceed);

	// Closes a case after the ruling has been enforced
	// pre: case must be in enforcement status
	// post: moves the case to resolved status
	// auth: admin
	ACTION closecase(uint64_t case_id);

	// Forces the recusal of an arbitrator from a case
	// pre: case must not be enforced yet
	// post: Case is considered void and mistrial status is set
	// auth: admin
	// ACTION forcerecusal(uint64_t case_id, string rationale, name arbitrator);

	// Dismiss an arbitrator from all his cases
	// auth: admin
	// ACTION dismissarb(name arbitrator, bool remove_from_cases);

#pragma endregion BP_Actions

#pragma region Arb_Actions
	ACTION arbacceptnom(name arbitrator, uint64_t case_id);


	// Set the different languages the arbitrator will handle cases
	// auth: arbitrator
	// ACTION setlangcodes(name arbitrator, vector<uint16_t> lang_codes);

	// Recuse from a case
	// post: Case is considered void and mistrial status is set
	// auth: arbitrator
	// ACTION recuse(uint64_t case_id, string rationale, name assigned_arb);

#pragma endregion Arb_Actions

#pragma region Test_Actions

#pragma endregion Test_Actions

#pragma region System Structs

	struct permission_level_weight
	{
		permission_level permission;
		uint16_t weight;

		EOSLIB_SERIALIZE(permission_level_weight, (permission)(weight))
	};

	struct key_weight
	{
		eosio::public_key key;
		uint16_t weight;

		EOSLIB_SERIALIZE(key_weight, (key)(weight))
	};

	struct wait_weight
	{
		uint32_t wait_sec;
		uint16_t weight;

		EOSLIB_SERIALIZE(wait_weight, (wait_sec)(weight))
	};

	struct authority
	{
		uint32_t threshold = 0;
		std::vector<key_weight> keys;
		std::vector<permission_level_weight> accounts;
		std::vector<wait_weight> waits;

		EOSLIB_SERIALIZE(authority, (threshold)(keys)(accounts)(waits))
	};

#pragma endregion System Structs

#pragma region Tables and Structs

	// NOTE: Stores all information related to a single claim.
	TABLE claim
	{
		uint64_t claim_id;
		string claim_summary; // NOTE: ipfs link to claim document from claimant
		string decision_link; // NOTE: ipfs link to decision document from arbitrator
		string response_link; // NOTE: ipfs link to response document from respondant (if any)
		time_point_sec claimant_limit_time;
		bool claim_info_needed = false;
		string claim_info_required;
		time_point_sec respondant_limit_time;
		bool response_info_needed = false;
		string response_info_required;
		uint8_t status = static_cast<uint8_t>(claim_status::FILED);
		uint8_t claim_category;

		uint64_t primary_key() const { return claim_id; }
		EOSLIB_SERIALIZE(claim, (claim_id)(claim_summary)(decision_link)(response_link)(claimant_limit_time)(claim_info_needed)(claim_info_required)(respondant_limit_time)(response_info_needed)(response_info_required)(status)(claim_category))
	};
	typedef multi_index<name("claims"), claim> claims_table;

	/**
	 * Case Files for all arbitration cases.
	 * @scope get_self().value
	 * @key case_id
	 */
	TABLE casefile
	{
		uint64_t case_id;
		uint8_t case_status;
		name claimant;
		name respondant;
		name arbitrator;
		vector<name> approvals;
		uint8_t number_claims;
		string case_ruling;
		asset fee_paid_tlos = asset(0, TLOS_SYM);
		time_point_sec update_ts;

		uint64_t primary_key() const { return case_id; }

		uint64_t by_claimant() const { return claimant.value; }
		uint128_t by_uuid() const
		{
			uint128_t claimant_id = static_cast<uint128_t>(claimant.value);
			uint128_t respondant_id = static_cast<uint128_t>(respondant.value);
			return (claimant_id << 64) | respondant_id;
		}

		EOSLIB_SERIALIZE(casefile, (case_id)(case_status)(claimant)(respondant)(arbitrator)(approvals)(number_claims)(case_ruling)(fee_paid_tlos)(update_ts))
	};
	typedef multi_index<name("casefiles"), casefile> casefiles_table;

	/**
	 * Singleton for global config settings.
	 * @scope singleton scope (get_self().value)
	 * @key table name
	 */
	TABLE config
	{
		name admin;
		string contract_version;

		uint8_t max_claims_per_case = 21;
		asset fee_usd = asset(100000, USD_SYM);
		asset available_funds = asset(0, TLOS_SYM);
		asset reserved_funds = asset(0, TLOS_SYM);

		EOSLIB_SERIALIZE(config, (admin)(contract_version)(max_claims_per_case)(fee_usd)(available_funds)(reserved_funds))
	};
	typedef singleton<name("config"), config> config_singleton;

	// scope: account name
	TABLE account
	{
		asset balance;

		uint64_t primary_key() const { return balance.symbol.code().raw(); }
		EOSLIB_SERIALIZE(account, (balance))
	};
	typedef multi_index<name("accounts"), account> accounts_table;

#pragma endregion Tables and Structs

#pragma region Helpers

	void validate_ipfs_url(string ipfs_url);

	void assert_string(string to_check, string error_msg);

	bool all_claims_resolved(uint64_t case_id);

	vector<permission_level_weight> get_arb_permissions();

	void set_permissions(vector<permission_level_weight> & perms);

	vector<claim>::iterator get_claim_at(string claim_hash, vector<claim> & claims);

	void sub_balance(name owner, asset value);

	void add_balance(name owner, asset value, name ram_payer);

	// mixes current transaction with seed and returns a hash
	checksum256 get_rngseed(uint64_t seed);

	string get_rand_ballot_name();

	uint64_t tlosusdprice();

	// void notify_bp_accounts();

#pragma endregion Helpers

#pragma region Notification_handlers

	// [[eosio::on_notify("eosio.token::transfer")]] void transfer_handler(name from, name to, asset quantity, string memo);

#pragma endregion Notification_handlers

#pragma region Test_Actions

	// ACTION skiptovoting();

#pragma endregion Test_Actions
};
