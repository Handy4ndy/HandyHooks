//**************************************************************
// SafeGuard Hook (SGH) - Xahau HandyHook Collection
// Author: @Handy_4ndy
//
// Description:
//   Provides min/max/cap/blacklist controls for payments.
//   Allows dynamic configuration via INVOKE transactions.
//   Emits a developer contribution on each processed transaction.
//
// Development Contribution:
//   This hook includes a hardcoded development contribution of **0.05 XAH** that is
//   automatically charged for each successful payment transaction.
//   This contribution is used to fund ongoing development and maintenance of the HandyHooks collection.
//
// Parameters:
//   'MIN'    (1 byte):   Enable/disable minimum amount check (0 or 1).
//   'MINAMT' (8 bytes):  Minimum allowed payment amount (in drops).
//   'MAX'    (1 byte):   Enable/disable maximum amount check (0 or 1).
//   'MAXAMT' (8 bytes):  Maximum allowed payment amount (in drops).
//   'CAP'    (1 byte):   Enable/disable outgoing cap (0 or 1, 80% of balance).
//   'BLACKLIST' (1 byte): Enable/disable blacklist feature (0 or 1).
//   'ADD_BLACKLIST' (20 bytes): Add account to blacklist (by account ID).
//   'REMOVE_BLACKLIST' (20 bytes): Remove account from blacklist (by account ID).
//
// Usage:
//   - To set any flag or value, send an Invoke transaction with the parameter(s).
//   - Multiple parameters can be set in a single transaction.
//   - Only the hook owner can change settings or manage the blacklist.
//
//**************************************************************

#include "hookapi.h"

#define DONE(x) accept(SBUF("SGH :: Success :: " x), __LINE__)
#define NOPE(x) rollback(SBUF("SGH :: Error :: " x), __LINE__)
#define GUARD(maxiter) _g(__LINE__, (maxiter)+1)

#define UINT64_TO_BUF(buf, i) \
{ \
    (buf)[0] = (i >> 56) & 0xFFU; \
    (buf)[1] = (i >> 48) & 0xFFU; \
    (buf)[2] = (i >> 40) & 0xFFU; \
    (buf)[3] = (i >> 32) & 0xFFU; \
    (buf)[4] = (i >> 24) & 0xFFU; \
    (buf)[5] = (i >> 16) & 0xFFU; \
    (buf)[6] = (i >> 8) & 0xFFU; \
    (buf)[7] = i & 0xFFU; \
}

#define UINT64_FROM_BUF(buf) \
    (((uint64_t)(buf)[0] << 56) + ((uint64_t)(buf)[1] << 48) + \
     ((uint64_t)(buf)[2] << 40) + ((uint64_t)(buf)[3] << 32) + \
     ((uint64_t)(buf)[4] << 24) + ((uint64_t)(buf)[5] << 16) + \
     ((uint64_t)(buf)[6] << 8) + (uint64_t)(buf)[7])

#define MIN_AMOUNT_VALUE 1
#define MAX_AMOUNT_VALUE 1000000

static uint8_t dev_contrib_acc[20] = {0xCC, 0x41, 0x96, 0xC1, 0xF2, 0x34, 0xDB, 0xAA, 0x06, 0x13, 0x0F, 0xAA, 0xF5, 0xD2, 0x8C, 0x53, 0x77, 0xA6, 0xFB, 0xCA};
#define DEV_CONTRIB_DROPS 50000

int64_t hook(uint32_t reserved) {
    TRACESTR("SGH :: SafeGuard :: Called.");

    uint8_t hook_acc[20];
    hook_account(hook_acc, 20);

    uint8_t otxn_acc[20];
    otxn_field(otxn_acc, 20, sfAccount);

    int64_t tt = otxn_type();

    uint64_t min_num = 0x000000000F4D494E; // 'MIN' (flag)
    uint8_t min_buf[8];
    UINT64_TO_BUF(min_buf, min_num);

    uint64_t min_amt_num = 0x4D494E414D54544; // 'MINAMT' (min amount)
    uint8_t min_amt_buf[8];
    UINT64_TO_BUF(min_amt_buf, min_amt_num);

    uint64_t max_num = 0x000000000F4D4158; // 'MAX' (flag)
    uint8_t max_buf[8];
    UINT64_TO_BUF(max_buf, max_num);

    uint64_t max_amt_num = 0x4D4158414D54544; // 'MAXAMT' (max amount)
    uint8_t max_amt_buf[8];
    UINT64_TO_BUF(max_amt_buf, max_amt_num);

    // CAP flag state key
    uint64_t cap_num = 0x000000000F434150; // 'CAP' (flag)
    uint8_t cap_buf[8];
    UINT64_TO_BUF(cap_buf, cap_num);

    // BLACKLIST flag state key
    uint64_t blacklist_num = 0x00424C4B4C535400; // 'BLKLST' (flag)
    uint8_t blacklist_buf[8];
    UINT64_TO_BUF(blacklist_buf, blacklist_num);

    // Process INVOKE transactions
    if (tt == 99) {
        
        int equal = 0;
        BUFFER_EQUAL(equal, otxn_acc, hook_acc, 20);
        if (!equal)
            NOPE("Only hook owner can change settings");

        // Get and set MIN parameter
        uint8_t min_param[1];
        uint8_t min_key[3] = {'M','I','N'};
        int8_t is_min = otxn_param(SBUF(min_param), SBUF(min_key));

        if (is_min > 0) {
            if (min_param[0] > 1)
                NOPE("MIN must be 0 or 1");
            if (state_set(SBUF(min_param), SBUF(min_buf)) < 0)
                NOPE("Failed to set MIN state");
            TRACEVAR(min_param[0]);
            DONE("MIN state set successfully");
        }

        // Get and set MINAMT parameter
        uint8_t min_amt_param[8];
        uint8_t min_amt_key[6] = {'M','I','N','A','M','T'};
        int8_t is_min_amt = otxn_param(SBUF(min_amt_param), min_amt_key, 6);

        if (is_min_amt == 8) {
            if (state_set(SBUF(min_amt_param), SBUF(min_amt_buf)) < 0)
                NOPE("Failed to set MINAMT state");
            TRACEVAR(UINT64_FROM_BUF(min_amt_param));
            DONE("MINAMT value set successfully");
        }

        // Get and set MAX parameter
        uint8_t max_param[1];
        uint8_t max_key[3] = {'M','A','X'};
        int8_t is_max = otxn_param(SBUF(max_param), SBUF(max_key));
        
        if (is_max > 0) {
            if (max_param[0] > 1)
            NOPE("MAX must be 0 or 1");
            if (state_set(SBUF(max_param), SBUF(max_buf)) < 0)
            NOPE("Failed to set MAX state");
            TRACEVAR(max_param[0]);
            DONE("MAX state set successfully");
        }

        // Get and set MAXAMT parameter
        uint8_t max_amt_param[8];
        uint8_t max_amt_key[6] = {'M','A','X','A','M','T'};
        int8_t is_max_amt = otxn_param(SBUF(max_amt_param), max_amt_key, 6);
        if (is_max_amt == 8) {
            if (state_set(SBUF(max_amt_param), SBUF(max_amt_buf)) < 0)
                NOPE("Failed to set MAXAMT state");
            TRACEVAR(UINT64_FROM_BUF(max_amt_param));
            DONE("MAXAMT value set successfully");
        }

        // CAP on/off switch logic
        uint8_t cap_param[1];
        uint8_t cap_key[3] = {'C','A','P'};
        int8_t is_cap = otxn_param(SBUF(cap_param), SBUF(cap_key));
        if (is_cap > 0) {
            if (cap_param[0] > 1)
                NOPE("CAP must be 0 or 1");
                if (state_set(SBUF(cap_param), SBUF(cap_buf)) < 0)
                    NOPE("Failed to set CAP state");
                TRACEVAR(cap_param[0]);
                DONE("CAP state set successfully");
        }

        // BLACKLIST on/off switch logic
        uint8_t blacklist_param[1];
        uint8_t blacklist_key[9] = {'B','L','A','C','K','L','I','S','T'};
        int8_t is_blacklist = otxn_param(SBUF(blacklist_param), blacklist_key, 9);
        if (is_blacklist > 0) {
            if (blacklist_param[0] > 1)
                NOPE("BLACKLIST must be 0 or 1");
            if (state_set(SBUF(blacklist_param), SBUF(blacklist_buf)) < 0)
                NOPE("Failed to set BLACKLIST state");
            TRACEVAR(blacklist_param[0]);
            DONE("BLACKLIST state set successfully");
        }

        // Add account to blacklist
        uint8_t add_blacklist_param[20];
        uint8_t add_blacklist_key[13] = {'A','D','D','_','B','L','A','C','K','L','I','S','T'};
        int8_t is_add_blacklist = otxn_param(SBUF(add_blacklist_param), add_blacklist_key, 13);
        
        // Generate account-specific namespace from the account ID
        if (is_add_blacklist == 20) {
            uint8_t account_namespace[32];
            for (int i = 0; GUARD(20), i < 20; ++i)
                account_namespace[i] = add_blacklist_param[i];
            for (int i = 20; GUARD(32), i < 32; ++i)
                account_namespace[i] = 0;
            
            // Simple state key for blacklist data
            uint8_t blacklist_key_data[32] = "BLACKLISTED";
            for (int i = 11; GUARD(32), i < 32; ++i)
                blacklist_key_data[i] = 0;
            
            // Set blacklisted flag (1 byte = 0x01)
            uint8_t blacklisted_flag[1] = {0x01};
            if (state_foreign_set(SBUF(blacklisted_flag), SBUF(blacklist_key_data), 
                                  SBUF(account_namespace), SBUF(hook_acc)) != 1)
                NOPE("Failed to add account to blacklist");
            
            DONE("Account added to blacklist successfully");
        }

        // Remove account from blacklist
        uint8_t remove_blacklist_param[20];
        uint8_t remove_blacklist_key[16] = {'R','E','M','O','V','E','_','B','L','A','C','K','L','I','S','T'};
        int8_t is_remove_blacklist = otxn_param(SBUF(remove_blacklist_param), remove_blacklist_key, 16);
        if (is_remove_blacklist == 20) {
            // Generate account-specific namespace from the account ID
            uint8_t account_namespace[32];
            for (int i = 0; GUARD(20), i < 20; ++i)
                account_namespace[i] = remove_blacklist_param[i];
            for (int i = 20; GUARD(32), i < 32; ++i)
                account_namespace[i] = 0;
            
            // Simple state key for blacklist data
            uint8_t blacklist_key_data[32] = "BLACKLISTED";
            for (int i = 11; GUARD(32), i < 32; ++i)
                blacklist_key_data[i] = 0;
            
            // Remove blacklisted flag by setting empty state
            if (state_foreign_set(0, 0, SBUF(blacklist_key_data), 
                                  SBUF(account_namespace), SBUF(hook_acc)) < 0)
                NOPE("Failed to remove account from blacklist");
            
            DONE("Account removed from blacklist successfully");
        }

    DONE("No valid parameters provided for invoke Skipping.");
    }

    // Process PAYMENT transactions
    if (tt == ttPAYMENT) {

        uint8_t min_flag;
        uint8_t max_flag;
        uint8_t cap_flag;
        uint8_t blacklist_flag;

        if (state(SBUF(&min_flag), SBUF(min_buf)) < 0)
            min_flag = 0;
        if (state(SBUF(&max_flag), SBUF(max_buf)) < 0)
            max_flag = 0;
        if (state(SBUF(&cap_flag), SBUF(cap_buf)) < 0)
            cap_flag = 0;
        if (state(SBUF(&blacklist_flag), SBUF(blacklist_buf)) < 0)
            blacklist_flag = 0;

        // Check blacklist if enabled
        if (blacklist_flag) {
            // Generate account-specific namespace from the otxn account ID
            uint8_t account_namespace[32];
            for (int i = 0; GUARD(20), i < 20; ++i)
                account_namespace[i] = otxn_acc[i];
            for (int i = 20; GUARD(32), i < 32; ++i)
                account_namespace[i] = 0;
            
            // Simple state key for blacklist data
            uint8_t blacklist_key_data[32] = "BLACKLISTED";
            for (int i = 11; GUARD(32), i < 32; ++i)
                blacklist_key_data[i] = 0;
            
            // Check if account is blacklisted
            uint8_t blacklist_status[1];
            int64_t blacklist_result = state_foreign(SBUF(blacklist_status), SBUF(blacklist_key_data), 
                                                    SBUF(account_namespace), SBUF(hook_acc));
            
            if (blacklist_result == 1 && blacklist_status[0] == 0x01)
                NOPE("Transaction rejected: Account is blacklisted");
        }

        uint8_t amount_buffer[48];
        int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
        if (amount_len != 8)
            DONE("Non-XAH payment accepted.");
        int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
        double xah_amount = (double)otxn_drops / 1000000.0;

        // Get direction: incoming or outgoing
        int is_outgoing = 0;
        if (BUFFER_EQUAL_20(hook_acc, otxn_acc))
            is_outgoing = 1;

        // Enforce minimum for incoming
        if (!is_outgoing && min_flag) {
            uint8_t min_amt_val[8];
            double min_amt = 1.0;
            if (state(SBUF(min_amt_val), SBUF(min_amt_buf)) == 8)
                min_amt = (double)UINT64_FROM_BUF(min_amt_val) / 1000000.0;
            if (xah_amount < min_amt)
                NOPE("Payment below minimum amount.");
        }

        // Enforce maximum for outgoing
        if (is_outgoing && max_flag) {
            uint8_t max_amt_val[8];
            double max_amt = 1000000.0;
            if (state(SBUF(max_amt_val), SBUF(max_amt_buf)) == 8)
                max_amt = (double)UINT64_FROM_BUF(max_amt_val) / 1000000.0;
            if (xah_amount > max_amt)
                NOPE("Payment above maximum amount.");
        }

        // Debug traces for CAP logic
        TRACEVAR(cap_flag);
        TRACEVAR(is_outgoing);
        
        // Enforce CAP for outgoing: payment cannot be >= 80% of account balance (XFL logic)
        if (is_outgoing && cap_flag) {

            // Get account keylet
            uint8_t acct_kl[34];
            util_keylet(SBUF(acct_kl), KEYLET_ACCOUNT, SBUF(hook_acc), 0,0,0,0);
            // SLOT SET: Slot 1
            if (slot_set(SBUF(acct_kl), 1) != 1)
                DONE("Safeguard Hook: Could not load account keylet");

            // SLOT SUBFIELD: sfBalance
            if (slot_subfield(1, sfBalance, 1) != 1)
                DONE("Safeguard Hook: Could not load account sfBalance");
            int64_t balance_xfl = slot_float(1); // balance in XFL

            // Get outgoing amount in drops and convert to XFL
            uint8_t amt_buf[8];
            otxn_field(SBUF(amt_buf), sfAmount);
            int64_t amt_drops = AMOUNT_TO_DROPS(amt_buf);
            int64_t amt_xfl = float_set(-6, amt_drops);

            // Calculate 80% of balance in XFL
            int64_t cap_limit_xfl = float_mulratio(balance_xfl, 0, 8, 10);
            TRACEVAR(balance_xfl);
            TRACEVAR(amt_xfl);
            TRACEVAR(cap_limit_xfl);
            if (amt_xfl >= cap_limit_xfl)
                NOPE("Payment exceeds 80% of account balance.");
        }

        // Reserve emission slot
        etxn_reserve(1);
        
        // Prepare the payment
        uint8_t dev_contrib_txn[PREPARE_PAYMENT_SIMPLE_SIZE];
        PREPARE_PAYMENT_SIMPLE(dev_contrib_txn, DEV_CONTRIB_DROPS, dev_contrib_acc, 0, 0);

        // Emit the service fee payment
        uint8_t dev_contrib_emithash[32];
        if (emit(SBUF(dev_contrib_emithash), SBUF(dev_contrib_txn)) != 32)
            TRACESTR("Safeguard :: Warning :: Failed to emit developer contribution payment.");

    DONE("Payment accepted and developer contribution emitted.");
    }

    NOPE("Unsupported transaction type, check HookOn Triggers!");

    _g(1,1);
    return 0;
}