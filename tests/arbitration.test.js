const { loadConfig, Blockchain } = require("@klevoya/hydra");

const config = loadConfig("hydra.yml");

describe("arbitration", () => {
  let blockchain = new Blockchain(config);
  let tester = blockchain.createAccount(`arbitration`);

  beforeAll(async () => {
    tester.setContract(blockchain.contractTemplates[`arbitration`]);
    tester.updateAuth(`active`, `owner`, {
      accounts: [
        {
          permission: {
            actor: tester.accountName,
            permission: `eosio.code`
          },
          weight: 1
        }
      ]
    });
  });

  beforeEach(async () => {
    tester.resetTables();
  });

  it("can send the addclaim action", async () => {
    expect.assertions(1);

    await tester.contract.addclaim({
      case_id: "123456",
      claim_link: "string",
      claimant: "user1",
      claim_category: 123
    });

    expect(tester.getTableRowsScoped(`accounts`)[`arbitration`]).toEqual([
      {
        balance: "1.2345 EOS"
      }
    ]);
  });
});
