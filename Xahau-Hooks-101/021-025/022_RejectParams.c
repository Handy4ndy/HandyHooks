/**
 * HandyHooks - 022 - Reject Hook Parameter.
 * This hook accepts outgoing payment transactions and incoming payments without hook parameters.
 * Incoming payments with hook parameters are rejected.
 * Install on ttPayment.
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

int64_t hook(uint32_t reserved) {
    TRACESTR("Reject Hook Param: Called.");

    // Get the hook account
    uint8_t hook_acc[20];
    if (hook_account(SBUF(hook_acc)) != 20)
        NOPE("Reject Hook Param: Failed to get hook account.");

    TRACEHEX(hook_acc);

    // Get the originating account of the transaction
    uint8_t otxn_acc[20];
    if (otxn_field(SBUF(otxn_acc), sfAccount) != 20)
        NOPE("Reject Hook Param: Failed to get transaction account.");

    TRACEHEX(otxn_acc);

    // Accept outgoing transactions
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc))
        DONE("Reject Hook Param: Outgoing payment transaction accepted.");

    // Check for hook parameters in incoming transaction
    uint8_t param_buf[1024]; // Buffer for sfHookParameters (arbitrary large size)
    int64_t param_len = otxn_field(SBUF(param_buf), sfHookParameters);

    // If sfHookParameters exists (length > 0), reject the transaction
    if (param_len > 0)
        NOPE("Reject Hook Param: Incoming payment with hook parameters rejected.");

    // Accept incoming payments without hook parameters
    DONE("Reject Hook Param: Incoming payment without hook parameters accepted.");

    _g(1,1); 

    return 0;
}