//**************************************************************
// Global Blacklist Provider Hook - Xahau HandyHook Collection
// Author: @Handy_4ndy
//
// Description:
//   A global blacklist management hook with severity and jurisdiction details.
//   Provides centralized storage and querying for enhanced enforcement.
//
// Features:
//   - Add/remove accounts with severity and jurisdiction
//   - Query blacklist status with details
//   - Enforce on payments based on severity/jurisdiction
//
// Admin Commands (Hook owner only):
//   'ADD_BLACKLIST' (20 bytes): Add account ID to blacklist
//     - Optional: 'SEVERITY' (1 byte), 'JURISDICTION' (up to 20 bytes)
//   'REMOVE_BLACKLIST' (20 bytes): Remove account
//   'CHECK_BLACKLIST' (20 bytes): Query status
//
// Storage:
//   - Namespace: Account ID (20) + zeros (12) = 32 bytes
//   - Key: "BLACKLISTED" (32 bytes)
//   - Value: 22 bytes [flag(1), severity(1), jurisdiction(20)]

#include "hookapi.h"

// Struct for blacklist data (22 bytes)
typedef struct
{
    uint8_t flag;          // 0x01 = blacklisted
    uint8_t severity;      // 1-5
    char jurisdiction[21]; // Up to 20 bytes + null
} BlacklistData;

// Use standard macros from hookapi.h (no redefinition needed)

int64_t hook(uint32_t reserved)
{
    TRACESTR("Global Blacklist Provider: Called.");

    uint8_t hook_acc[20];
    if (hook_account(SBUF(hook_acc)) != 20)
        NOPE("Failed to get hook account.");

    uint8_t otxn_acc[20];
    if (otxn_field(SBUF(otxn_acc), sfAccount) != 20)
        NOPE("Failed to get transaction account.");

    int64_t tt = otxn_type();

    if (tt == 99)
    { // ttINVOKE
        if (!BUFFER_EQUAL_20(otxn_acc, hook_acc))
            NOPE("Only hook owner can manage blacklist");

        // ADD_BLACKLIST
        uint8_t add_blacklist_param[20];
        uint8_t add_blacklist_key[13] = {'A', 'D', 'D', '_', 'B', 'L', 'A', 'C', 'K', 'L', 'I', 'S', 'T'};
        int8_t is_add = otxn_param(SBUF(add_blacklist_param), (uint32_t)add_blacklist_key, 13);  // Cast to uint32_t
        if (is_add == 20)
        {
            uint8_t severity_param[1];
            uint8_t severity_key[8] = {'S', 'E', 'V', 'E', 'R', 'I', 'T', 'Y'};
            int8_t is_sev = otxn_param(SBUF(severity_param), (uint32_t)severity_key, 8);  // Cast
            uint8_t severity = 1; // Default low
            if (is_sev == 1)
            {
                severity = severity_param[0];
                if (severity < 1 || severity > 5)
                {
                    NOPE("Severity must be 1-5");
                }
            }

            uint8_t jur_param[20];
            uint8_t jur_key[12] = {'J', 'U', 'R', 'I', 'S', 'D', 'I', 'C', 'T', 'I', 'O', 'N'};
            int8_t is_jur = otxn_param(SBUF(jur_param), (uint32_t)jur_key, 12);  // Cast
            char jurisdiction[21] = {0}; // Default empty
            if (is_jur > 0 && is_jur <= 20) {
                for (int i = 0; i < is_jur; ++i) {
                    jurisdiction[i] = jur_param[i];
                }
                // No validation - just accept up to 20 bytes
            }

            BlacklistData data = {0x01, severity, {0}};
            for (int i = 0; i < 21; ++i) {  // Removed GUARD
                data.jurisdiction[i] = jurisdiction[i];
            }

            uint8_t data_buf[22];
            data_buf[0] = data.flag;
            data_buf[1] = data.severity;
            for (int i = 0; i < 20; ++i) {  // Removed GUARD
                data_buf[2 + i] = data.jurisdiction[i];
            }

            // Inline generate_namespace
            uint8_t account_namespace[32];
            for (int i = 0; i < 20; ++i)  // Removed GUARD
                account_namespace[i] = add_blacklist_param[i];
            for (int i = 20; i < 32; ++i)  // Removed GUARD
                account_namespace[i] = 0;

            // Inline generate_blacklist_key
            uint8_t blacklist_key_data[32];
            uint8_t temp[11] = "BLACKLISTED";
            for (int i = 0; i < 32; ++i)  // Removed GUARD
                blacklist_key_data[i] = (i < 11) ? temp[i] : 0;

            if (state_foreign_set(SBUF(data_buf), SBUF(blacklist_key_data),
                                  SBUF(account_namespace), SBUF(hook_acc)) != 22)
                NOPE("Failed to add");
            DONE("Account added");
        }

        // REMOVE_BLACKLIST
        uint8_t rem_blacklist_param[20];
        uint8_t rem_blacklist_key[16] = {'R', 'E', 'M', 'O', 'V', 'E', '_', 'B', 'L', 'A', 'C', 'K', 'L', 'I', 'S', 'T'};
        int8_t is_rem = otxn_param(SBUF(rem_blacklist_param), (uint32_t)rem_blacklist_key, 16);  // Cast
        if (is_rem == 20)
        {
            // Inline generate_namespace
            uint8_t account_namespace[32];
            for (int i = 0; i < 20; ++i)  // Removed GUARD
                account_namespace[i] = rem_blacklist_param[i];
            for (int i = 20; i < 32; ++i)  // Removed GUARD
                account_namespace[i] = 0;

            // Inline generate_blacklist_key
            uint8_t blacklist_key_data[32];
            uint8_t temp[11] = "BLACKLISTED";
            for (int i = 0; i < 32; ++i)  // Removed GUARD
                blacklist_key_data[i] = (i < 11) ? temp[i] : 0;

            if (state_foreign_set(0, 0, SBUF(blacklist_key_data),
                                  SBUF(account_namespace), SBUF(hook_acc)) < 0)
                NOPE("Failed to remove");
            DONE("Account removed");
        }

        // CHECK_BLACKLIST
        uint8_t chk_blacklist_param[20];
        uint8_t chk_blacklist_key[15] = {'C', 'H', 'E', 'C', 'K', '_', 'B', 'L', 'A', 'C', 'K', 'L', 'I', 'S', 'T'};
        int8_t is_chk = otxn_param(SBUF(chk_blacklist_param), (uint32_t)chk_blacklist_key, 15);  // Cast
        if (is_chk == 20)
        {
            // Inline generate_namespace
            uint8_t account_namespace[32];
            for (int i = 0; i < 20; ++i)  // Removed GUARD
                account_namespace[i] = chk_blacklist_param[i];
            for (int i = 20; i < 32; ++i)  // Removed GUARD
                account_namespace[i] = 0;

            // Inline generate_blacklist_key
            uint8_t blacklist_key_data[32];
            uint8_t temp[11] = "BLACKLISTED";
            for (int i = 0; i < 32; ++i)  // Removed GUARD
                blacklist_key_data[i] = (i < 11) ? temp[i] : 0;

            uint8_t blacklist_status[22];  // Changed to 22 bytes
            int64_t res = state_foreign(SBUF(blacklist_status), SBUF(blacklist_key_data),
                                        SBUF(account_namespace), SBUF(hook_acc));
            if (res == 22)
            {
                BlacklistData data;
                data.flag = blacklist_status[0];
                data.severity = blacklist_status[1];
                for (int i = 0; i < 20; ++i) {  // Removed GUARD
                    data.jurisdiction[i] = blacklist_status[2 + i];
                }
                DONE("Account blacklisted");
            }
            else
            {
                DONE("Not blacklisted");
            }
        }

        DONE("No valid params");
    }

    if (tt == ttPAYMENT)
    {
        // Inline generate_namespace
        uint8_t account_namespace[32];
        for (int i = 0; i < 20; ++i)  // Removed GUARD
            account_namespace[i] = otxn_acc[i];
        for (int i = 20; i < 32; ++i)  // Removed GUARD
            account_namespace[i] = 0;

        // Inline generate_blacklist_key
        uint8_t blacklist_key_data[32];
        uint8_t temp[11] = "BLACKLISTED";
        for (int i = 0; i < 32; ++i)  // Removed GUARD
            blacklist_key_data[i] = (i < 11) ? temp[i] : 0;

        uint8_t blacklist_status[22];  // Changed to 22 bytes
        int64_t res = state_foreign(SBUF(blacklist_status), SBUF(blacklist_key_data),
                                    SBUF(account_namespace), SBUF(hook_acc));
        if (res == 22 && blacklist_status[0] == 0x01)
        {
            BlacklistData data;
            data.severity = blacklist_status[1];
            for (int i = 0; i < 20; ++i) {  // Removed GUARD
                data.jurisdiction[i] = blacklist_status[2 + i];
            }
            // Example enforcement: reject if severity > 3 or jurisdiction starts with "US"
            if (data.severity > 3 || (data.jurisdiction[0] == 'U' && data.jurisdiction[1] == 'S'))
            {
                NOPE("Transaction rejected");
            }
        }
        DONE("Payment allowed");
    }

    DONE("Passed through");
    _g(1, 1);
    return 0;
}