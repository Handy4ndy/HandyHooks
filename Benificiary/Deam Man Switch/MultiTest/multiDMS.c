//**************************************************************
// Multi Beneficiary Contract - HandyHooks Example
// Author: @Handy_4ndy
//
// Description:
//   Forwards incoming payments to up to 3 configured beneficiary accounts,
//   using configurable split percentages. Always leaves at least 1% in the
//   hook account to fund future executions.
//
// Features:
//   - Forwards percentage of incoming payments to beneficiary accounts
//   - Supports up to 3 beneficiary accounts
//   - Configurable split percentages (max total 100%)
//   - Always leaves at least 1% in the hook account
//   - Admin-only configuration via invoke
//
// Admin Commands (Hook owner only):
//   'BA1' (20 bytes): Set Beneficiary Account 1
//   'BP1' (4 bytes):  Set Beneficiary Percentage 1 (1-99, as uint32)
//   'BA2' (20 bytes): Set Beneficiary Account 2
//   'BP2' (4 bytes):  Set Beneficiary Percentage 2 (1-99, as uint32)
//   'BA3' (20 bytes): Set Beneficiary Account 3
//   'BP3' (4 bytes):  Set Beneficiary Percentage 3 (1-99, as uint32)
//
// Usage:
//   - To set any account or percentage, send an Invoke transaction with the parameter(s).
//   - Only the hook owner can change beneficiary configuration.
//
// Storage Structure:
//   - Beneficiary accounts: "BA1", "BA2", "BA3" (20 bytes each)
//   - Percentages:          "BP1", "BP2", "BP3" (4 bytes each, uint32)
//
//**************************************************************

#include "hookapi.h"

#define DONE(x) accept(SBUF("MBC:: Success :: " x), __LINE__)
#define WARN(x) rollback(SBUF("MBC:: Misconfigured :: " x), __LINE__)
#define NOPE(x) rollback(SBUF("MBC:: Error :: " x), __LINE__)

// State keys for beneficiary accounts and percentages
#define BA1_KEY 0x4241310000000000ULL // "BA1"
#define BP1_KEY 0x4250310000000000ULL // "BP1"
#define BA2_KEY 0x4241320000000000ULL // "BA2"
#define BP2_KEY 0x4250320000000000ULL // "BP2"
#define BA3_KEY 0x4241330000000000ULL // "BA3"
#define BP3_KEY 0x4250330000000000ULL // "BP3"

int64_t hook(uint32_t reserved)
{
    TRACESTR("MBC :: Multi Beneficiary Contract :: Called");

    // Get hook and origin accounts
    uint8_t hook_acc[20];
    uint8_t otxn_acc[20];
    hook_account(SBUF(hook_acc));
    otxn_field(SBUF(otxn_acc), sfAccount);

    // Load admin account from Hook Parameters (set during installation)
    uint8_t admin_acc[20];
    if (hook_param(SBUF(admin_acc), "ADMIN", 5) != 20)
        WARN("Admin account not set during installation");

    int64_t tt = otxn_type();

    // Handle ttINVOKE for configuration or SEND
    if (tt == ttINVOKE)
    {
        // Hook owner configures beneficiaries
        if (BUFFER_EQUAL_20(hook_acc, otxn_acc))
        {
            // Check for BA1 parameter
            uint8_t ba_param[20];
            if (otxn_param(SBUF(ba_param), "BA1", 3) == 20)
            {
                uint8_t key_buf[8];
                UINT64_TO_BUF(key_buf, BA1_KEY);
                state_set(SBUF(ba_param), SBUF(key_buf));
                DONE("BA1 configured");
            }

            // Check for BP1 parameter
            uint8_t bp_param[4];
            if (otxn_param(SBUF(bp_param), "BP1", 3) == 4)
            {
                uint8_t key_buf[8];
                UINT64_TO_BUF(key_buf, BP1_KEY);
                state_set(SBUF(bp_param), SBUF(key_buf));
                DONE("BP1 configured");
            }

            // Check for BA2 parameter
            if (otxn_param(SBUF(ba_param), "BA2", 3) == 20)
            {
                uint8_t key_buf[8];
                UINT64_TO_BUF(key_buf, BA2_KEY);
                state_set(SBUF(ba_param), SBUF(key_buf));
                DONE("BA2 configured");
            }

            // Check for BP2 parameter
            if (otxn_param(SBUF(bp_param), "BP2", 3) == 4)
            {
                uint8_t key_buf[8];
                UINT64_TO_BUF(key_buf, BP2_KEY);
                state_set(SBUF(bp_param), SBUF(key_buf));
                DONE("BP2 configured");
            }

            // Check for BA3 parameter
            if (otxn_param(SBUF(ba_param), "BA3", 3) == 20)
            {
                uint8_t key_buf[8];
                UINT64_TO_BUF(key_buf, BA3_KEY);
                state_set(SBUF(ba_param), SBUF(key_buf));
                DONE("BA3 configured");
            }

            // Check for BP3 parameter
            if (otxn_param(SBUF(bp_param), "BP3", 3) == 4)
            {
                uint8_t key_buf[8];
                UINT64_TO_BUF(key_buf, BP3_KEY);
                state_set(SBUF(bp_param), SBUF(key_buf));
                DONE("BP3 configured");
            }

            NOPE("No valid configuration parameters");
        }

        // Admin invokes SEND to distribute balance
        if (BUFFER_EQUAL_20(admin_acc, otxn_acc))
        {
            uint8_t send_param[1];
            if (otxn_param(SBUF(send_param), "SEND", 4) == 1)
            {
                // Load beneficiaries and percentages (similar to original savings logic)
                uint8_t beneficiary_accounts[3][20];
                uint32_t beneficiary_percentages[3];
                int configured_accounts = 0;

                // Load BA1/BP1
                uint8_t key_buf[8];
                UINT64_TO_BUF(key_buf, BA1_KEY);
                if (state(SBUF(beneficiary_accounts[0]), SBUF(key_buf)) == 20)
                {
                    UINT64_TO_BUF(key_buf, BP1_KEY);
                    uint8_t bp_data[4];
                    if (state(SBUF(bp_data), SBUF(key_buf)) == 4)
                        beneficiary_percentages[0] = UINT32_FROM_BUF(bp_data);
                    else
                        beneficiary_percentages[0] = 99; // Default to 99% for single account
                    configured_accounts++;
                }

                // Load BA2/BP2
                UINT64_TO_BUF(key_buf, BA2_KEY);
                if (state(SBUF(beneficiary_accounts[1]), SBUF(key_buf)) == 20)
                {
                    // Check for duplicate with BA1
                    if (BUFFER_EQUAL_20(beneficiary_accounts[1], beneficiary_accounts[0]))
                    {
                        TRACEVAR(beneficiary_accounts[0]);
                        TRACEVAR(beneficiary_accounts[1]);
                        WARN("BA2 cannot match BA1");
                    }

                    UINT64_TO_BUF(key_buf, BP2_KEY);
                    uint8_t bp_data[4];
                    if (state(SBUF(bp_data), SBUF(key_buf)) == 4)
                        beneficiary_percentages[1] = UINT32_FROM_BUF(bp_data);
                    else
                        WARN("BP2 missing for multiple accounts");
                    configured_accounts++;
                }

                // Load BA3/BP3
                UINT64_TO_BUF(key_buf, BA3_KEY);
                if (state(SBUF(beneficiary_accounts[2]), SBUF(key_buf)) == 20)
                {
                    // Check for duplicates with BA1 and BA2
                    if (BUFFER_EQUAL_20(beneficiary_accounts[2], beneficiary_accounts[0]) ||
                        BUFFER_EQUAL_20(beneficiary_accounts[2], beneficiary_accounts[1]))
                    {
                        TRACEVAR(beneficiary_accounts[0]);
                        TRACEVAR(beneficiary_accounts[1]);
                        TRACEVAR(beneficiary_accounts[2]);
                        WARN("BA3 cannot match BA1 or BA2");
                    }

                    UINT64_TO_BUF(key_buf, BP3_KEY);
                    uint8_t bp_data[4];
                    if (state(SBUF(bp_data), SBUF(key_buf)) == 4)
                        beneficiary_percentages[2] = UINT32_FROM_BUF(bp_data);
                    else
                        WARN("BP3 missing for multiple accounts");
                    configured_accounts++;
                }

                // If no accounts configured, pass through
                if (configured_accounts == 0)
                    DONE("No beneficiary accounts configured");

                // If only one account, send 99% (leave 1% behind)
                if (configured_accounts == 1)
                    beneficiary_percentages[0] = 99;

                // Cap total percentage at 100%
                uint32_t total_percent = 0;
                for (int i = 0; GUARD(3), i < configured_accounts; i++)
                {
                    TRACEVAR(beneficiary_percentages[i]);
                    total_percent += beneficiary_percentages[i];
                }
                if (total_percent > 100)
                    WARN("Total beneficiary percentage cannot exceed 100%");

                // Get account balance (adapt from SingleDMS.c)
                uint8_t acct_kl[34];
                util_keylet(SBUF(acct_kl), KEYLET_ACCOUNT, SBUF(hook_acc), 0,0,0,0);
                if (slot_set(SBUF(acct_kl), 1) != 1)
                    NOPE("Could not load account keylet");
                if (slot_subfield(1, sfBalance, 1) != 1)
                    NOPE("Could not load sfBalance");
                int64_t balance_xfl = slot_float(1);
                if (balance_xfl <= 0)
                    NOPE("Insufficient balance");

                // Convert XFL balance to drops
                int64_t balance_drops = float_int(balance_xfl, 6, 1); // 6 for drops, 1 for rounding

                // Reserve 0.1 XAH per beneficiary for fees (adapt from SingleDMS.c)
                uint64_t reserve_per_txn = 1000000; // 1 XAH in drops
                uint64_t total_reserve = reserve_per_txn * configured_accounts;

                if (balance_drops <= (int64_t)total_reserve)
                    NOPE("Insufficient balance to send (need reserve for fees)");

                // Calculate total to distribute: 99% of balance minus reserve, to leave at least 1%
                int64_t total_to_distribute = (balance_drops - total_reserve) * 99 / 100;

                // Reserve space for emitted transactions
                etxn_reserve(configured_accounts);

                // Emit payments to beneficiaries
                for (int i = 0; GUARD(3), i < configured_accounts; i++)
                {
                    // Calculate amount for this beneficiary
                    int64_t beneficiary_amount = total_to_distribute * beneficiary_percentages[i] / 100;

                    if (beneficiary_amount > 0)
                    {
                        // Prepare payment transaction
                        uint8_t txn[PREPARE_PAYMENT_SIMPLE_SIZE];
                        PREPARE_PAYMENT_SIMPLE(txn, beneficiary_amount, beneficiary_accounts[i], 0, 0);

                        uint8_t emithash[32];
                        // Emit the transaction
                        if (emit(SBUF(emithash), SBUF(txn)) != 32)
                            NOPE("Failed to emit payment to beneficiary");
                    }
                }

                DONE("Balance distributed to beneficiaries");
            }
            NOPE("Invalid admin invoke");
        }
        NOPE("Unauthorized invoke");
    }

    // Accept outgoing transactions
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc))
        DONE("Outgoing transaction");

    // Remove payment handling logic (no longer triggered by incoming payments)

    // Pass through all other transaction types
    DONE("Transaction passed through");

    _g(1, 1);
    return 0;
}