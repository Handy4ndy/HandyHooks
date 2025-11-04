//**************************************************************
// Admin Hook Locker - Xahau HandyHook Collection
// Author: @Handy_4ndy
//
// Description:
//   Prevents SetHook transactions on the account.
//   Admin can toggle the locker on/off via INVOKE transactions.
//   All other transaction types pass through normally.
//
// Hook Parameters (set during installation):
//   'ADMIN' (20 bytes): Admin account ID with toggle control.
//
// Admin Controls (via ttINVOKE):
//   'LOCK' (1 byte): Any value to toggle lock on/off.
//
// Rejects (when enabled):-
//   - SetHook transactions.
//   - Invokes from none admin accounts.
//
//**************************************************************

#include "hookapi.h"

#define DONE(x) accept(SBUF("AHL:: Success :: " x), __LINE__)
#define WARN(x) rollback(SBUF("AHL:: Warning :: " x), __LINE__)
#define NOPE(x) rollback(SBUF("AHL:: Error :: " x), __LINE__)

#define SETHOOK_LOCK_KEY 0x5345544800000000ULL  // "SETH"

int64_t hook(uint32_t reserved) {

    TRACESTR("AHL :: Admin Hook Locker  :: Called");

    // Get the Hook account
    uint8_t hook_acc[20];
    hook_account(SBUF(hook_acc));

    // Get the Originating account of the transaction
    uint8_t otxn_acc[20];
    otxn_field(SBUF(otxn_acc), sfAccount);

    // Load admin account from Hook Parameters (set during installation)
    uint8_t admin_acc[20];
    if (hook_param(SBUF(admin_acc), "ADMIN", 5) != 20)
        WARN("Admin account not set during installation");

    int64_t tt = otxn_type();

    // Accept non-relevant transaction types
    if (tt != ttINVOKE && tt != ttHOOK_SET)
        DONE("Transaction type not monitored - passing through");

    // Handle ttINVOKE for admin toggle control
    if (tt == ttINVOKE)
    {
        // Only admin can toggle the LOCK
        if (!BUFFER_EQUAL_20(admin_acc, otxn_acc))
            NOPE("Only admin can toggle the HOOK_SET LOCK");

        // Check for LOCK parameter
        uint8_t lock_param[1];
        if (otxn_param(SBUF(lock_param), "LOCK", 4) == 1)
        {
            uint8_t sethook_key[8];
            UINT64_TO_BUF(sethook_key, SETHOOK_LOCK_KEY);

            // Read current lock state
            uint8_t lock_state[1];
            int32_t state_len = state(SBUF(lock_state), SBUF(sethook_key));

            uint8_t new_state[1];
            if (state_len == 1 && lock_state[0] == 1)
            {
                // Lock is enabled, disable it
                new_state[0] = 0;
                state_set(SBUF(new_state), SBUF(sethook_key));
                DONE("HOOK_SET Lock disabled");
            }
            else
            {
                // Lock is disabled or not set, enable it
                new_state[0] = 1;
                state_set(SBUF(new_state), SBUF(sethook_key));
                DONE("HOOK_SET Lock enabled");
            }
        }

        // If LOCK parameter not present, pass through to next hook
        DONE("No LOCK parameter, Skipping..");
    }

    // Check if SetHook locking is enabled
    uint8_t sethook_key[8];
    UINT64_TO_BUF(sethook_key, SETHOOK_LOCK_KEY);
    uint8_t lock_state[1];
    int32_t state_len = state(SBUF(lock_state), SBUF(sethook_key));

    // If lock state not set or set to 0, allow all transactions
    if (state_len != 1 || lock_state[0] == 0)
        DONE("HOOK_SET Lock disabled - transaction allowed");

    // Lock is enabled, check transaction type
    if (tt == ttHOOK_SET)
        NOPE("HOOK_SET transaction locked");

    // All other transaction types are accepted
    DONE("Transaction type allowed");

    _g(1,1); // Guard
    return 0;
}