const { loadConfig, Blockchain } = require("@klevoya/hydra");

const config = loadConfig("hydra.yml");

describe("Init Telos Arbitration Smart Contract Tests", () => {
    let blockchain = new Blockchain(config);
    let arbitration = blockchain.createAccount("arbitration");
    let admin = blockchain.createAccount("admin");

    beforeAll(async () => {
        arbitration.setContract(blockchain.contractTemplates[`arbitration`]);
        arbitration.updateAuth(`active`, `owner`, {
        accounts: [
            {
            permission: {
                actor: arbitration.accountName,
                permission: `eosio.code`
            },
            weight: 1
            }
        ]
        });
    });

    beforeEach(async () => {
        arbitration.resetTables();
    });

    it("init the SC", async () => {
        expect.assertions(1);

        await arbitration.contract.init({
            initial_admin: "admin"
        })

        expect(arbitration.getTableRowsScoped("config")["arbitration"][0]).toEqual({
            "contract_version": "0.1.0",
            "admin": "admin",
            "fee_usd": "10.0000 USD",
            "available_funds": "15.0000 TLOS",
            "reserved_funds": "100.0000 TLOS",
            "max_claims_per_case": 21
        });
    });

    it("return an error if contract is already initialized", async () => {
        await arbitration.loadFixtures("config", require("../fixtures/arbitration/config.json"));

        await expect(arbitration.contract.init({
            initial_admin: "admin"
        })).rejects.toThrow("contract already initialized")
    
    });


    it("return an error if admin account doesn't exist", async () => {
        await expect(arbitration.contract.init({
            initial_admin: "falseadmin"
        })).rejects.toThrow("initial admin account doesn't exist")
    });
});
