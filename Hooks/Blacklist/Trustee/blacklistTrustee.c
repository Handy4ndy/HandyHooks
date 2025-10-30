//**************************************************************
// Blacklist Trustee Hook - Xahau HandyHook Collection
// Author: @handy_4ndy
//
// Description:
//   A client hook that queries a blacklist provider hook to enforce
//   blacklist restrictions. This hook can be installed on any account
//   to leverage centralized blacklist management from a provider hook.
//
// Features:
//   - Toggle blacklist enforcement on/off via invoke
//   - Configure blacklist provider account via invoke
//   - Query foreign state from blacklist provider hook
//   - Service fee collection for transaction processing
//   - Configurable via admin invoke transactions
//
// Admin Commands (Hook owner only):
//   'BLACKLIST' (1 byte): Toggle blacklist enforcement (0=off, 1=on)
//   'PROVIDER_ACC' (20 bytes): Set blacklist provider account ID
//
// Integration:
//   Queries blacklist provider using state_foreign() to check if
//   transaction accounts are blacklisted before allowing transactions.
//
// Storage Structure:
//   - Blacklist flag: Stored with key "BLKLST" (0=off, 1=on)
//   - Provider account: Stored with key "PROVIDER" (20 bytes)
//
// Service Fee:
//   - A service fee of 0.05 XAH is charged per processed transaction
//**************************************************************

#include "hookapi.h"

#define DONE(x) accept(SBUF("Blacklist Trustee :: Success :: " x), __LINE__)
#define NOPE(x) rollback(SBUF("Blacklist Trustee :: Error :: " x), __LINE__)
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

// provider service fee account
static uint8_t service_fee_acc[20] = {0xA6U, 0xFEU, 0xA2U, 0x10U, 0xA7U, 0x4AU, 0xFFU, 0xA8U, 0x77U, 0xA8U, 0xB5U, 0x53U, 0x8AU, 0xFAU, 0xC4U, 0x89U, 0x61U, 0xF6U, 0xFCU, 0x86U};
#define SERVICE_FEE_DROPS 50000

int64_t hook(uint32_t reserved) {
    TRACESTR("Blacklist Trustee: Called.");

    // Get hook account and transaction account
    uint8_t hook_acc[20];
    if (hook_account(SBUF(hook_acc)) != 20)
        NOPE("Failed to get hook account.");

    uint8_t otxn_acc[20];
    if (otxn_field(SBUF(otxn_acc), sfAccount) != 20)
        NOPE("Failed to get transaction account.");

    int64_t tt = otxn_type();

    // State keys for configuration
    uint64_t blacklist_num = 0x00424C4B4C535400; // 'BLKLST' (flag)
    uint8_t blacklist_buf[8];
    UINT64_TO_BUF(blacklist_buf, blacklist_num);

    uint64_t provider_num = 0x50524F5649444552; // 'PROVIDER' 
    uint8_t provider_buf[8];
    UINT64_TO_BUF(provider_buf, provider_num);

    // Process ttINVOKE transactions for configuration
    if (tt == 99) {
        // Verify only hook owner can change settings
        if (!BUFFER_EQUAL_20(otxn_acc, hook_acc))
            NOPE("Only hook owner can change blacklist trustee settings");

        // BLACKLIST on/off toggle
        uint8_t blacklist_param[1];
        uint8_t blacklist_key[9] = {'B','L','A','C','K','L','I','S','T'};
        int8_t is_blacklist = otxn_param(SBUF(blacklist_param), blacklist_key, 9);
        if (is_blacklist > 0) {
            if (blacklist_param[0] > 1)
                NOPE("BLACKLIST must be 0 or 1");
            if (state_set(SBUF(blacklist_param), SBUF(blacklist_buf)) < 0)
                NOPE("Failed to set BLACKLIST state");
            TRACEVAR(blacklist_param[0]);
            DONE("BLACKLIST state toggled successfully");
        }

        // Set blacklist provider account
        uint8_t provider_param[20];
        uint8_t provider_key[12] = {'P','R','O','V','I','D','E','R','_','A','C','C'};
        int8_t is_provider = otxn_param(SBUF(provider_param), provider_key, 12);
        if (is_provider == 20) {
            if (state_set(SBUF(provider_param), SBUF(provider_buf)) != 20)
                NOPE("Failed to set provider account");
            DONE("Blacklist provider account configured successfully");
        }

        DONE("No valid configuration parameters provided");
    }

    // Process payments and check blacklist
    if (tt == ttPAYMENT) {
        
        // Ignore self-transactions (hook account sending to itself)
        if (BUFFER_EQUAL_20(hook_acc, otxn_acc)) {
            TRACESTR("Self-transaction detected - passing through");
            DONE("Self-transaction passed through");
        }
        
        // Load blacklist flag
        uint8_t blacklist_flag;
        int64_t flag_result = state(SBUF(&blacklist_flag), SBUF(blacklist_buf));
        TRACEVAR(flag_result);
        
        if (flag_result < 0)
            blacklist_flag = 0; // Default to disabled

        // Add debugging for blacklist flag
        TRACEVAR(blacklist_flag);
        
        // Only check blacklist if enabled
        if (blacklist_flag) {
            TRACESTR("Blacklist checking enabled - loading provider account");
            
            // Load provider account
            uint8_t provider_acc[20];
            int64_t provider_result = state(SBUF(provider_acc), SBUF(provider_buf));
            TRACEVAR(provider_result);
            
            if (provider_result != 20)
                NOPE("Blacklist provider account not configured - use PROVIDER_ACC parameter");

            // Generate account-specific namespace from the otxn account ID
            uint8_t account_namespace[32];
            for (int i = 0; GUARD(20), i < 20; ++i)
                account_namespace[i] = otxn_acc[i];
            for (int i = 20; GUARD(32), i < 32; ++i)
                account_namespace[i] = 0;
            
            // State key for blacklist data on provider
            uint8_t blacklist_key_data[32] = "BLACKLISTED";
            for (int i = 11; GUARD(32), i < 32; ++i)
                blacklist_key_data[i] = 0;
            
            TRACESTR("Querying blacklist status from provider");
            
            // Query blacklist status from provider hook using state_foreign
            uint8_t blacklist_status[1] = {0};
            int64_t blacklist_result = state_foreign(SBUF(blacklist_status), 
                                                    SBUF(blacklist_key_data), 
                                                    SBUF(account_namespace), 
                                                    SBUF(provider_acc));
            
            TRACEVAR(blacklist_result);
            
            if (blacklist_result == 1) {
                // Successfully retrieved blacklist status
                TRACEVAR(blacklist_status[0]);
                if (blacklist_status[0] == 0x01) {
                    TRACESTR("Account is blacklisted - rejecting transaction");
                    NOPE("Transaction rejected: Account is blacklisted by provider");
                } else {
                    TRACESTR("Account found in provider but not blacklisted - proceeding");
                }
            } else if (blacklist_result == -5) {
                // DOESNT_EXIST - account not in blacklist (not blacklisted)
                TRACESTR("Account not found in blacklist (DOESNT_EXIST) - proceeding");
            } else {
                // Other error - log but proceed (fail-open for now)
                TRACESTR("Blacklist query error - proceeding with caution");
                TRACEVAR(blacklist_result);
            }
        } else {
            TRACESTR("Blacklist checking disabled - proceeding without check");
        }

        // Only emit service fee if we have sufficient balance to avoid emission errors
        // Reserve emission slot for service fee
        etxn_reserve(1);
        
        // Prepare and emit service fee payment
        uint8_t fee_txn[PREPARE_PAYMENT_SIMPLE_SIZE];
        PREPARE_PAYMENT_SIMPLE(fee_txn, SERVICE_FEE_DROPS, service_fee_acc, 0, 0);
        
        uint8_t fee_emithash[32];
        int64_t emit_result = emit(SBUF(fee_emithash), SBUF(fee_txn));
        if (emit_result != 32) {
            TRACEVAR(emit_result);
            TRACESTR("Blacklist Trustee :: Warning :: Failed to emit service fee payment - insufficient balance or other error.");
        } else {
            TRACESTR("Service fee payment emitted successfully");
        }

        DONE("Payment transaction processed successfully");
    }

    // Pass through other transaction types
    DONE("Transaction passed through");

    _g(1,1);
    return 0;
}
