/**
 * HandyHooks - 021 - Require Hook Parameter.
 * This hook requires hook parameters with payments.
 * Incoming payments without hook parameters are rejected.
 * Install on ttPayment.
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

int64_t hook(uint32_t reserved) {
    TRACESTR("Require Hook Param: Called.");

    // Get the hook account
    uint8_t hook_acc[20];
    if (hook_account(SBUF(hook_acc)) != 20)
        NOPE("Require Hook Param: Failed to get hook account.");

    TRACEHEX(hook_acc);

    // Get the originating account of the transaction
    uint8_t otxn_acc[20];
    if (otxn_field(SBUF(otxn_acc), sfAccount) != 20)
        NOPE("Require Hook Param: Failed to get transaction account.");

    TRACEHEX(otxn_acc);

    // Accept outgoing transactions
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc))
        DONE("Require Hook Param: Outgoing payment transaction accepted.");

    // Check for hook parameters in incoming transaction
    uint8_t param_buf[2056]; // Buffer for sfHookParameters (arbitrary large size)
    int64_t param_len = otxn_field(SBUF(param_buf), sfHookParameters);

    // If sfHookParameters are not present, reject the transaction
    if (param_len == 0)
        NOPE("Require Hook Param: Incoming payment without hook parameters rejected.");

    // Accept incoming payments without hook parameters
    DONE("Require Hook Param: Incoming payment with hook parameters accepted.");

    _g(1,1); 

    return 0;
}