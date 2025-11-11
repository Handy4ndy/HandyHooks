//**************************************************************
// Multi Beneficiary Threshold Contract (MBTC) - Xahau HandyHook Collection
// Author: @Handy_4ndy
//
// Description:
//   When the inactivity threshold has been exceeded, any incoming transaction (payment or invoke)
//   from anyone other than the hook owner triggers an automatic distribution of the full XAH balance to configured beneficiaries.
//   Timer resets on outgoing transactions from the hook account.
//   Hook owner configures beneficiaries via invoke.
//
// Hook Parameters (set during installation):
//   'THRESHOLD' (4 bytes): Inactivity threshold in seconds (uint32) before auto-trigger.
//
// Hook Owner Commands (via ttINVOKE):
//   'BA1' (20 bytes): Set Beneficiary Account 1
//   'BP1' (4 bytes):  Set Beneficiary Percentage 1 (1-100, as uint32)
//   'BA2' (20 bytes): Set Beneficiary Account 2
//   'BP2' (4 bytes):  Set Beneficiary Percentage 2 (1-100, as uint32)
//   'BA3' (20 bytes): Set Beneficiary Account 3
//   'BP3' (4 bytes):  Set Beneficiary Percentage 3 (1-100, as uint32)
//
//**************************************************************

#include "hookapi.h"

#define DONE(x) accept(SBUF("MBTC:: Success :: " x), __LINE__)
#define WARN(x) rollback(SBUF("MBTC:: Warning :: " x), __LINE__)
#define NOPE(x) rollback(SBUF("MBTC:: Error :: " x), __LINE__)
#define COMP(x) rollback(SBUF("MBTC:: Complete :: " x), __LINE__)
uint8_t msg_buf[67] = "MBTC:: Error :: You must wait 00000000 seconds before triggering.";

#define SET_TIME_MSG(remaining_seconds)                            \
    {                                                              \
        msg_buf[30] = '0' + ((remaining_seconds) / 10000000) % 10; \
        msg_buf[31] = '0' + ((remaining_seconds) / 1000000) % 10;  \
        msg_buf[32] = '0' + ((remaining_seconds) / 100000) % 10;   \
        msg_buf[33] = '0' + ((remaining_seconds) / 10000) % 10;    \
        msg_buf[34] = '0' + ((remaining_seconds) / 1000) % 10;     \
        msg_buf[35] = '0' + ((remaining_seconds) / 100) % 10;      \
        msg_buf[36] = '0' + ((remaining_seconds) / 10) % 10;       \
        msg_buf[37] = '0' + ((remaining_seconds)) % 10;            \
        rollback((uint32_t)msg_buf, sizeof(msg_buf), __LINE__);    \
    }

// State keys for beneficiary accounts and percentages
#define BA1_KEY 0x4241310000000000ULL // "BA1"
#define BP1_KEY 0x4250310000000000ULL // "BP1"
#define BA2_KEY 0x4241320000000000ULL // "BA2"
#define BP2_KEY 0x4250320000000000ULL // "BP2"
#define BA3_KEY 0x4241330000000000ULL // "BA3"
#define BP3_KEY 0x4250330000000000ULL // "BP3"
#define LAST_CHECKIN_KEY 0x4C41535443484543ULL

int64_t hook(uint32_t reserved)
{
    TRACESTR("MBTC :: Multi Beneficiary Threshold Contract :: Called");

    // Declare key_buf
    uint8_t key_buf[8];

    // Get the Hook account
    uint8_t hook_acc[20];
    hook_account(SBUF(hook_acc));

    // Get the Originating account of the transaction
    uint8_t otxn_acc[20];
    otxn_field(SBUF(otxn_acc), sfAccount);

    // Load threshold from Hook Parameters
    uint8_t threshold_buf[4];
    uint32_t threshold = 2592000; // Default 30 days in seconds
    if (hook_param(SBUF(threshold_buf), "THRESHOLD", 9) == 4)
        threshold = UINT32_FROM_BUF(threshold_buf);

    int64_t tt = otxn_type();

    // Accept outgoing payments from hook account and reset timer
    if ((tt == ttPAYMENT && BUFFER_EQUAL_20(hook_acc, otxn_acc)) || (tt == ttINVOKE && BUFFER_EQUAL_20(hook_acc, otxn_acc)))
    {
        // Reset the timer on outgoing transaction
        uint32_t current_time = (uint32_t)ledger_last_time();
        uint8_t time_buf[4];
        UINT32_TO_BUF(time_buf, current_time);
        UINT64_TO_BUF(key_buf, LAST_CHECKIN_KEY);
        state_set(SBUF(time_buf), SBUF(key_buf));
        DONE("Outgoing payment from hook account accepted, timer reset");
    }

    // Handle ttINVOKE for configuration (only from hook owner)
    if (tt == ttINVOKE && BUFFER_EQUAL_20(hook_acc, otxn_acc))
    {
        // Check for BA1 and BP1 parameters
        uint8_t ba_param[20];
        uint8_t bp_param[4];
        int has_ba1 = otxn_param(SBUF(ba_param), "BA1", 3) == 20;
        int has_bp1 = otxn_param(SBUF(bp_param), "BP1", 3) == 4;
        if (has_ba1 && has_bp1)
        {
            // Cap at 100%
            uint32_t bp1_value = UINT32_FROM_BUF(bp_param);
            if (bp1_value > 100)
                bp1_value = 100;  
            
            // Update buffer
            UINT32_TO_BUF(bp_param, bp1_value);
            UINT64_TO_BUF(key_buf, BA1_KEY);
            state_set(SBUF(ba_param), SBUF(key_buf));
            UINT64_TO_BUF(key_buf, BP1_KEY);
            state_set(SBUF(bp_param), SBUF(key_buf));
            DONE("BA1 and BP1 configured");
        }
        else if (has_ba1 || has_bp1)
        {
            NOPE("BA1 and BP1 must be submitted together");
        }

        // Check for BA2 and BP2 parameters
        int has_ba2 = otxn_param(SBUF(ba_param), "BA2", 3) == 20;
        int has_bp2 = otxn_param(SBUF(bp_param), "BP2", 3) == 4;
        if (has_ba2 && has_bp2)
        {
            // Check for duplicate with BA1
            uint8_t existing_ba1[20];
            UINT64_TO_BUF(key_buf, BA1_KEY);
            if (state(SBUF(existing_ba1), SBUF(key_buf)) == 20)
            {
                if (BUFFER_EQUAL_20(ba_param, existing_ba1))
                    WARN("BA2 cannot match BA1");
            }
            // Check total with BP1
            uint32_t bp2_value = UINT32_FROM_BUF(bp_param);
            if (bp2_value > 100)
                bp2_value = 100;
            uint32_t total_percent = bp2_value;
            uint8_t existing_bp1[4];
            UINT64_TO_BUF(key_buf, BP1_KEY);
            if (state(SBUF(existing_bp1), SBUF(key_buf)) == 4)
                total_percent += UINT32_FROM_BUF(existing_bp1);
            if (total_percent > 100)
                WARN("Total beneficiary percentage cannot exceed 100%");

            // Update buffer
            UINT32_TO_BUF(bp_param, bp2_value);
            UINT64_TO_BUF(key_buf, BA2_KEY);
            state_set(SBUF(ba_param), SBUF(key_buf));
            UINT64_TO_BUF(key_buf, BP2_KEY);
            state_set(SBUF(bp_param), SBUF(key_buf));
            DONE("BA2 and BP2 configured");
        }
        else if (has_ba2 || has_bp2)
        {
            NOPE("BA2 and BP2 must be submitted together");
        }

        // Check for BA3 and BP3 parameters
        int has_ba3 = otxn_param(SBUF(ba_param), "BA3", 3) == 20;
        int has_bp3 = otxn_param(SBUF(bp_param), "BP3", 3) == 4;
        if (has_ba3 && has_bp3)
        {
            // Check for duplicates with BA1 and BA2
            uint8_t existing_ba1[20];
            uint8_t existing_ba2[20];
            UINT64_TO_BUF(key_buf, BA1_KEY);
            int has_existing_ba1 = (state(SBUF(existing_ba1), SBUF(key_buf)) == 20);
            UINT64_TO_BUF(key_buf, BA2_KEY);
            int has_existing_ba2 = (state(SBUF(existing_ba2), SBUF(key_buf)) == 20);
            if (has_existing_ba1 && BUFFER_EQUAL_20(ba_param, existing_ba1))
                WARN("BA3 cannot match BA1");
            if (has_existing_ba2 && BUFFER_EQUAL_20(ba_param, existing_ba2))
                WARN("BA3 cannot match BA2");

            // Check total with BP1 and BP2
            uint32_t bp3_value = UINT32_FROM_BUF(bp_param);
            if (bp3_value > 100)
                bp3_value = 100;  
            uint32_t total_percent = bp3_value;
            uint8_t existing_bp1[4];
            uint8_t existing_bp2[4];
            UINT64_TO_BUF(key_buf, BP1_KEY);
            if (state(SBUF(existing_bp1), SBUF(key_buf)) == 4)
                total_percent += UINT32_FROM_BUF(existing_bp1);
            UINT64_TO_BUF(key_buf, BP2_KEY);
            if (state(SBUF(existing_bp2), SBUF(key_buf)) == 4)
                total_percent += UINT32_FROM_BUF(existing_bp2);
            if (total_percent > 100)
                WARN("Total beneficiary percentage cannot exceed 100%");

            // Update buffer
            UINT32_TO_BUF(bp_param, bp3_value);
            UINT64_TO_BUF(key_buf, BA3_KEY);
            state_set(SBUF(ba_param), SBUF(key_buf));
            UINT64_TO_BUF(key_buf, BP3_KEY);
            state_set(SBUF(bp_param), SBUF(key_buf));
            DONE("BA3 and BP3 configured");
        }
        else if (has_ba3 || has_bp3)
        {
            NOPE("BA3 and BP3 must be submitted together");
        }

        WARN("No valid configuration parameters, submit BAx and BPx pairs");
    }

    // Trigger balance distribution with an invoke transaction from non-owner, if threshold exceeded
    if (tt == ttINVOKE && !BUFFER_EQUAL_20(hook_acc, otxn_acc))
    {
        // Load last outgoing time
        UINT64_TO_BUF(key_buf, LAST_CHECKIN_KEY);
        uint8_t last_checkin_buf[4];
        uint32_t last_checkin = 0;
        if (state(SBUF(last_checkin_buf), SBUF(key_buf)) == 4)
            last_checkin = UINT32_FROM_BUF(last_checkin_buf);

        // Get current time
        uint32_t current_time = (uint32_t)ledger_last_time();

        // Check if threshold exceeded
        uint32_t time_elapsed = current_time - last_checkin;
        if (time_elapsed < threshold)
        {
            uint32_t remaining = threshold - time_elapsed;
            SET_TIME_MSG(remaining);
        }

        // Load beneficiaries and percentages
        uint8_t beneficiary_accounts[3][20];
        uint32_t beneficiary_percentages[3];
        int configured_accounts = 0;

        // Load BA1/BP1
        UINT64_TO_BUF(key_buf, BA1_KEY);
        if (state(SBUF(beneficiary_accounts[0]), SBUF(key_buf)) == 20)
        {
            UINT64_TO_BUF(key_buf, BP1_KEY);
            uint8_t bp_data[4];
            if (state(SBUF(bp_data), SBUF(key_buf)) == 4)
                beneficiary_percentages[0] = UINT32_FROM_BUF(bp_data);
            else
                beneficiary_percentages[0] = 100;
            configured_accounts++;

                TRACEHEX(beneficiary_accounts[0]);
                TRACEVAR(beneficiary_percentages[0]);
        }

        // Load BA2/BP2
        UINT64_TO_BUF(key_buf, BA2_KEY);
        if (state(SBUF(beneficiary_accounts[1]), SBUF(key_buf)) == 20)
        {
            UINT64_TO_BUF(key_buf, BP2_KEY);
            uint8_t bp_data[4];
            if (state(SBUF(bp_data), SBUF(key_buf)) == 4)
                beneficiary_percentages[1] = UINT32_FROM_BUF(bp_data);
            else
                WARN("BP2 missing for multiple accounts");
            configured_accounts++;

                TRACEHEX(beneficiary_accounts[1]);
                TRACEVAR(beneficiary_percentages[1]);
        }

        // Load BA3/BP3
        UINT64_TO_BUF(key_buf, BA3_KEY);
        if (state(SBUF(beneficiary_accounts[2]), SBUF(key_buf)) == 20)
        {
            UINT64_TO_BUF(key_buf, BP3_KEY);
            uint8_t bp_data[4];
            if (state(SBUF(bp_data), SBUF(key_buf)) == 4)
                beneficiary_percentages[2] = UINT32_FROM_BUF(bp_data);
            }
            else
                WARN("BP3 missing for multiple accounts");
            configured_accounts++;

                TRACEHEX(beneficiary_accounts[2]);
                TRACEVAR(beneficiary_percentages[2]);


        // If no accounts configured, pass through
        if (configured_accounts == 0)
            DONE("No beneficiary accounts configured");

        // If only one account, send 100%
        if (configured_accounts == 1)
            beneficiary_percentages[0] = 100;

        // Get account balance
        uint8_t acct_kl[34];
        util_keylet(SBUF(acct_kl), KEYLET_ACCOUNT, SBUF(hook_acc), 0, 0, 0, 0);
        if (slot_set(SBUF(acct_kl), 1) != 1)
            NOPE("Could not load account keylet");
        if (slot_subfield(1, sfBalance, 1) != 1)
            NOPE("Could not load sfBalance");
        int64_t balance_xfl = slot_float(1);
        if (balance_xfl <= 0)
            NOPE("Insufficient balance");

        // Convert XFL balance to drops
        int64_t balance_drops = float_int(balance_xfl, 6, 1);

        // Reserve 3 XAH per beneficiary for fees
        uint64_t reserve_per_txn = 3000000;
        uint64_t total_reserve = reserve_per_txn * configured_accounts;

        if (balance_drops <= (int64_t)total_reserve)
            COMP("This contract appears to have been completed, Farewell Adventurer <3 ");

        // Calculate total to distribute: 100% of balance minus reserve
        int64_t total_to_distribute = (balance_drops - total_reserve) * 100 / 100;

        // Reserve space for emitted transactions
        etxn_reserve(configured_accounts);

        // Emit payments to beneficiaries
        for (int i = 0; GUARD(3), i < configured_accounts; i++)
        {   
            // Calculate beneficiary amount
            int64_t beneficiary_amount = total_to_distribute * beneficiary_percentages[i] / 100;
            if (beneficiary_amount > 0)
            {
                uint8_t txn[PREPARE_PAYMENT_SIMPLE_SIZE];
                PREPARE_PAYMENT_SIMPLE(txn, beneficiary_amount, beneficiary_accounts[i], 0, 0);
                uint8_t emithash[32];
                if (emit(SBUF(emithash), SBUF(txn)) != 32)
                    NOPE("Failed to emit payment to beneficiary");
            }
        }

        DONE("Balance distributed to beneficiaries (threshold exceeded)");
    }

    // Pass through all other transaction types
    DONE("Transaction passed through");

    _g(1, 1);
    return 0;
}