/**
 * HandyHooks - 024 - Reject Memo.
 * This hook accepts outgoing payment transactions and incoming payments without memos.
 * Incoming payments with memos are rejected.
 * Install on ttPayment.
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

int64_t hook(uint32_t reserved) {
    TRACESTR("Reject Memo: Called.");

    // Get the hook account
    uint8_t hook_acc[20];
    if (hook_account(SBUF(hook_acc)) != 20)
        NOPE("Reject Memo: Failed to get hook account.");

    TRACEHEX(hook_acc);

    // Get the originating account of the transaction
    uint8_t otxn_acc[20];
    if (otxn_field(SBUF(otxn_acc), sfAccount) != 20)
        NOPE("Reject Memo: Failed to get transaction account.");

    TRACEHEX(otxn_acc);

    // Accept outgoing transactions
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc))
        DONE("Reject Memo: Outgoing payment transaction accepted.");

    // Check for memos in incoming transaction
    uint8_t memo_buf[1024]; // Buffer for sfMemos (arbitrary large size)
    int64_t memo_len = otxn_field(SBUF(memo_buf), sfMemos);

    // If sfMemos exists (length > 0), reject the transaction
    if (memo_len > 0)
        NOPE("Reject Memo: Incoming payment with memos rejected.");

    // Accept incoming payments without memos
    DONE("Reject Memo: Incoming payment without memos accepted.");

    _g(1,1); // Guard function to prevent reentrancy
}