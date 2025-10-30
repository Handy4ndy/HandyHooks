//**************************************************************
// Blacklist Provider Hook - Xahau HandyHook Collection
// Author: @Handy_4ndy
//
// Description:
//   A dedicated blacklist management hook that provides centralized
//   blacklist storage and querying capabilities. Other hooks can
//   query this hook's blacklist data using state_foreign() calls.
//
// Features:
//   - Add/remove accounts to/from blacklist via invoke transactions
//   - Permanently active blacklist enforcement
//   - Check if specific accounts are blacklisted
//   - Hierarchical namespace storage for unlimited scalability
//   - Query interface for other hooks to check blacklist status
//
// Admin Commands (Hook owner only):
//   'ADD_BLACKLIST' (20 bytes): Add account ID to blacklist
//   'REMOVE_BLACKLIST' (20 bytes): Remove account ID from blacklist
//   'CHECK_BLACKLIST' (20 bytes): Query blacklist status of account
//
// Integration:
//   Other hooks can check blacklist status by calling:
//   state_foreign(status_buf, "BLACKLISTED", account_namespace, blacklist_hook_account)
//
// Storage Structure:
//   - Account blacklist status: Stored in account-specific namespaces
//   - Namespace: First 20 bytes = account ID, remaining 12 bytes = zeros
//   - Key: "BLACKLISTED" (padded with zeros to 32 bytes)
//   - Value: 0x01 = blacklisted, absence = not blacklisted
//
//**************************************************************

#include "hookapi.h"

#define DONE(x) accept(SBUF("Blacklist :: Success :: " x), __LINE__)
#define NOPE(x) rollback(SBUF("Blacklist :: Error :: " x), __LINE__)
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

int64_t hook(uint32_t reserved) {
    TRACESTR("Blacklist Provider: Called.");

    // Get hook account and transaction account
    uint8_t hook_acc[20];
    if (hook_account(SBUF(hook_acc)) != 20)
        NOPE("Failed to get hook account.");

    uint8_t otxn_acc[20];
    if (otxn_field(SBUF(otxn_acc), sfAccount) != 20)
        NOPE("Failed to get transaction account.");

    int64_t tt = otxn_type();

    // Only process ttINVOKE transactions for configuration
    if (tt == 99) {
        // Verify only hook owner can change settings
        if (!BUFFER_EQUAL_20(otxn_acc, hook_acc))
            NOPE("Only hook owner can manage blacklist settings");

        // Add account to blacklist
        uint8_t add_blacklist_param[20];
        uint8_t add_blacklist_key[13] = {'A','D','D','_','B','L','A','C','K','L','I','S','T'};
        int8_t is_add_blacklist = otxn_param(SBUF(add_blacklist_param), add_blacklist_key, 13);
        if (is_add_blacklist == 20) {
            // Generate account-specific namespace from the account ID
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

        // Check blacklist status (query command)
        uint8_t check_blacklist_param[20];
        uint8_t check_blacklist_key[15] = {'C','H','E','C','K','_','B','L','A','C','K','L','I','S','T'};
        int8_t is_check_blacklist = otxn_param(SBUF(check_blacklist_param), check_blacklist_key, 15);
        if (is_check_blacklist == 20) {
            // Generate account-specific namespace from the account ID
            uint8_t account_namespace[32];
            for (int i = 0; GUARD(20), i < 20; ++i)
                account_namespace[i] = check_blacklist_param[i];
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
                DONE("Account is blacklisted");
            else
                DONE("Account is not blacklisted");
        }

        DONE("No valid blacklist parameters provided");
    }

    // For other transaction types, always check blacklist
    if (tt == ttPAYMENT) {
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

        DONE("Payment transaction allowed");
    }

    // Pass through other transaction types
    DONE("Transaction passed through");

    _g(1,1);
    return 0;
}
