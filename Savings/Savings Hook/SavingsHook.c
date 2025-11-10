//**************************************************************
// Incoming Payment Savings Hook (IPS)- HandyHooks Example
// Author: @Handy_4ndy
//
// Description:
//   Automatically forwards a percentage of incoming payments to up to 3
//   configured savings accounts, using configurable split percentages.
//   Always leaves at least 1% in the hook account to fund future executions.
//
// Features:
//   - Forwards percentage of incoming payments to savings accounts
//   - Supports up to 3 savings accounts
//   - Configurable split percentages (max total 99%)
//   - Always leaves at least 1% in the hook account
//   - Admin-only configuration via invoke
//
// Admin Commands (Hook owner only):
//   'SA1' (20 bytes): Set Savings Account 1
//   'SP1' (4 bytes):  Set Savings Percentage 1 (1-99, as uint32)
//   'SA2' (20 bytes): Set Savings Account 2
//   'SP2' (4 bytes):  Set Savings Percentage 2 (1-99, as uint32)
//   'SA3' (20 bytes): Set Savings Account 3
//   'SP3' (4 bytes):  Set Savings Percentage 3 (1-99, as uint32)
//
// Usage:
//   - To set any account or percentage, send an Invoke transaction with the parameter(s).
//   - Only the hook owner can change savings configuration.
//
// Storage Structure:
//   - Savings accounts: "SA1", "SA2", "SA3" (20 bytes each)
//   - Percentages:      "SP1", "SP2", "SP3" (4 bytes each, uint32)
//
//**************************************************************

#include "hookapi.h"

#define DONE(x) accept(SBUF("IPS:: Success :: " x), __LINE__)
#define WARN(x) rollback(SBUF("IPS:: Misconfigured :: " x), __LINE__)
#define NOPE(x) rollback(SBUF("IPS:: Error :: " x), __LINE__)

// State keys for savings accounts and percentages
#define SA1_KEY 0x5341310000000000ULL // "SA1"
#define SP1_KEY 0x5350310000000000ULL // "SP1"
#define SA2_KEY 0x5341320000000000ULL // "SA2"
#define SP2_KEY 0x5350320000000000ULL // "SP2"
#define SA3_KEY 0x5341330000000000ULL // "SA3"
#define SP3_KEY 0x5350330000000000ULL // "SP3"

int64_t hook(uint32_t reserved)
{
    TRACESTR("IPS :: Incoming Payment Savings :: Called");

    // Get hook and origin accounts
    uint8_t hook_acc[20];
    uint8_t otxn_acc[20];
    hook_account(SBUF(hook_acc));
    otxn_field(SBUF(otxn_acc), sfAccount);

    int64_t tt = otxn_type();

    // Handle ttINVOKE for configuration FIRST (before checking outgoing)
    if (tt == ttINVOKE)
    {
        if (!BUFFER_EQUAL_20(hook_acc, otxn_acc))
            NOPE("Only hook owner can configure");

        // Check for SA1 parameter
        uint8_t sa_param[20];
        if (otxn_param(SBUF(sa_param), "SA1", 3) == 20)
        {
            uint8_t key_buf[8];
            UINT64_TO_BUF(key_buf, SA1_KEY);
            state_set(SBUF(sa_param), SBUF(key_buf));
            DONE("SA1 configured");
        }

        // Check for SP1 parameter
        uint8_t sp_param[4];
        if (otxn_param(SBUF(sp_param), "SP1", 3) == 4)
        {
            uint8_t key_buf[8];
            UINT64_TO_BUF(key_buf, SP1_KEY);
            state_set(SBUF(sp_param), SBUF(key_buf));
            DONE("SP1 configured");
        }

        // Check for SA2 parameter
        if (otxn_param(SBUF(sa_param), "SA2", 3) == 20)
        {
            uint8_t key_buf[8];
            UINT64_TO_BUF(key_buf, SA2_KEY);
            state_set(SBUF(sa_param), SBUF(key_buf));
            DONE("SA2 configured");
        }

        // Check for SP2 parameter
        if (otxn_param(SBUF(sp_param), "SP2", 3) == 4)
        {
            uint8_t key_buf[8];
            UINT64_TO_BUF(key_buf, SP2_KEY);
            state_set(SBUF(sp_param), SBUF(key_buf));
            DONE("SP2 configured");
        }

        // Check for SA3 parameter
        if (otxn_param(SBUF(sa_param), "SA3", 3) == 20)
        {
            uint8_t key_buf[8];
            UINT64_TO_BUF(key_buf, SA3_KEY);
            state_set(SBUF(sa_param), SBUF(key_buf));
            DONE("SA3 configured");
        }

        // Check for SP3 parameter
        if (otxn_param(SBUF(sp_param), "SP3", 3) == 4)
        {
            uint8_t key_buf[8];
            UINT64_TO_BUF(key_buf, SP3_KEY);
            state_set(SBUF(sp_param), SBUF(key_buf));
            DONE("SP3 configured");
        }

        NOPE("No valid parameters");
    }

    // Accept outgoing transactions (but NOT ttINVOKE since we handled that above)
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc))
        DONE("Outgoing transaction");

    // Handle ttPayment (type 96)
    if (tt == ttPayment)
    {
        // Get Genesis Mint amount
        uint8_t amount[8];
        if (otxn_field(SBUF(amount), sfAmount) != 8)
            DONE("Non-XAH Payment, Skipping..");

        // Load savings accounts and percentages
        uint8_t savings_accounts[3][20];
        uint32_t savings_percentages[3];
        int configured_accounts = 0;

        // Load SA1/SP1
        uint8_t key_buf[8];
        UINT64_TO_BUF(key_buf, SA1_KEY);
        if (state(SBUF(savings_accounts[0]), SBUF(key_buf)) == 20)
        {
            UINT64_TO_BUF(key_buf, SP1_KEY);
            uint8_t sp_data[4];
            if (state(SBUF(sp_data), SBUF(key_buf)) == 4)
                savings_percentages[0] = UINT32_FROM_BUF(sp_data);
            else
                savings_percentages[0] = 99; // Default to 99% for single account
            configured_accounts++;
        }

        // Load SA2/SP2
        UINT64_TO_BUF(key_buf, SA2_KEY);
        if (state(SBUF(savings_accounts[1]), SBUF(key_buf)) == 20)
        {
            // Check for duplicate with SA1
            if (BUFFER_EQUAL_20(savings_accounts[1], savings_accounts[0]))
            {
                TRACEVAR(savings_accounts[0]);
                TRACEVAR(savings_accounts[1]);
                WARN("SA2 cannot match SA1");
            }

            UINT64_TO_BUF(key_buf, SP2_KEY);
            uint8_t sp_data[4];
            if (state(SBUF(sp_data), SBUF(key_buf)) == 4)
                savings_percentages[1] = UINT32_FROM_BUF(sp_data);
            else
                WARN("SP2 missing for multiple accounts");
            configured_accounts++;
        }

        // Load SA3/SP3
        UINT64_TO_BUF(key_buf, SA3_KEY);
        if (state(SBUF(savings_accounts[2]), SBUF(key_buf)) == 20)
        {
            // Check for duplicates with SA1 and SA2
            if (BUFFER_EQUAL_20(savings_accounts[2], savings_accounts[0]) ||
                BUFFER_EQUAL_20(savings_accounts[2], savings_accounts[1]))
            {
                TRACEVAR(savings_accounts[0]);
                TRACEVAR(savings_accounts[1]);
                TRACEVAR(savings_accounts[2]);
                WARN("SA3 cannot match SA1 or SA2");
            }

            UINT64_TO_BUF(key_buf, SP3_KEY);
            uint8_t sp_data[4];
            if (state(SBUF(sp_data), SBUF(key_buf)) == 4)
                savings_percentages[2] = UINT32_FROM_BUF(sp_data);
            else
                WARN("SP3 missing for multiple accounts");
            configured_accounts++;
        }

        // If no accounts configured, pass through
        if (configured_accounts == 0)
            DONE("No savings accounts configured");

        // If only one account, send 99% (leave 1% behind)
        if (configured_accounts == 1)
            savings_percentages[0] = 99;

        // Cap total percentage at 99%
        uint32_t total_percent = 0;
        for (int i = 0; GUARD(3), i < configured_accounts; i++)
        {
            TRACEVAR(savings_percentages[i]);
            total_percent += savings_percentages[i];
        }
        if (total_percent > 99)
            WARN("Total savings percentage cannot exceed 99%");

        // Convert amount to XFL and calculate savings
        int64_t total_xfl = float_sto_set(SBUF(amount));
        int64_t hundred_xfl = float_set(0, 100);

        // Calculate each savings amount in XFL
        int64_t sum_forwarded_xfl = 0;
        int64_t savings_amounts_xfl[3];
        for (int i = 0; GUARD(3), i < configured_accounts; i++)
        {
            int64_t percent_xfl = float_set(0, savings_percentages[i]);
            int64_t fraction_xfl = float_divide(percent_xfl, hundred_xfl);
            int64_t savings_amount_xfl = float_multiply(total_xfl, fraction_xfl);
            savings_amounts_xfl[i] = savings_amount_xfl;
            sum_forwarded_xfl = float_sum(sum_forwarded_xfl, savings_amount_xfl);
        }

        // Calculate remaining amount (should be at least 1%)
        int64_t remaining_xfl = float_sum(total_xfl, float_negate(sum_forwarded_xfl));
        int64_t one_percent_xfl = float_divide(total_xfl, hundred_xfl);
        if (float_compare(remaining_xfl, one_percent_xfl, COMPARE_LESS) == 1)
            WARN("Remaining amount less than 1% after savings distribution");

        etxn_reserve(configured_accounts);

        for (int i = 0; GUARD(3), i < configured_accounts; i++)
        {
            uint64_t savings_drops = (uint64_t)float_int(savings_amounts_xfl[i], 0, 0);
            if (savings_drops > 0)
            {
                // Emit payment to savings account
                uint8_t txn[PREPARE_PAYMENT_SIMPLE_SIZE];
                PREPARE_PAYMENT_SIMPLE(txn, savings_drops, savings_accounts[i], 0, 0);

                uint8_t emithash[32];
                emit(SBUF(emithash), SBUF(txn));
            }
        }

        DONE("Payment percentage forwarded to savings accounts");
    }

    // Pass through all other transaction types
    DONE("Transaction passed through");

    _g(1, 1);
    return 0;
}