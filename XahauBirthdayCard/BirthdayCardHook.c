//**************************************************************
// BirthdayCardHook - Xahau 2nd Birthday Card Example
// Author: @handy_4ndy
//
// Description:
//   Anyone can add a birthday message on-chain.
//   Only the hook owner can delete messages.
//
// Parameters:
//   'MSG' (bytes): The message to add. If present, a new message is stored.
//   'DEL' (8 bytes): The message number to delete. Only owner can invoke.
//   'CNT' (8 bytes, state): Counter for the number of messages stored.
//
// Usage:
//   - To add a message: Send an Invoke transaction with 'MSG' parameter.
//   - To delete a message: Send an Invoke transaction with 'DEL' parameter (8-byte message number).
//
//**************************************************************
#include "hookapi.h"
#include <stdint.h>

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

#define GUARD(maxiter) _g(__LINE__, (maxiter)+1)

int64_t hook(uint32_t reserved) {
    GUARD(5);

    TRACESTR("BirthdayCardHook: called");

    uint8_t hook_acct[20];
    hook_account(hook_acct, 20);

    uint8_t otx_acc[20];
    otxn_field(otx_acc, 20, sfAccount);

    int64_t tt = otxn_type();
    if (tt != 99) {
        NOPE("Error: Transaction must be an Invoke");
    }

    uint8_t msg_buf[1024];
    uint8_t msg_key[3] = {'M', 'S', 'G'};
    int64_t msg_len = otxn_param(SBUF(msg_buf), SBUF(msg_key));

    uint8_t del_buf[8];
    uint8_t del_key[3] = {'D', 'E', 'L'};
    int64_t del_len = otxn_param(SBUF(del_buf), SBUF(del_key));

    uint8_t count_buf[8];
    uint8_t count_key[3] = {'C', 'N', 'T'};
    uint64_t count = 0;
    if (state(SBUF(count_buf), SBUF(count_key)) >= 0) {
        count = UINT64_FROM_BUF(count_buf);
    }

    // Anyone can add a message
    if (msg_len > 0) {
        uint8_t msg_num_buf[8];
        UINT64_TO_BUF(msg_num_buf, count + 1);

        if (state_set(msg_buf, msg_len, msg_num_buf, 8) < 0) {
            NOPE("Error: Could not add message to birthday card");
        }

        count++;
        UINT64_TO_BUF(count_buf, count);
        if (state_set(SBUF(count_buf), SBUF(count_key)) < 0) {
            NOPE("Error: Could not update message count on birthday card");
        }

        DONE("Success: Message added to birthday card!");
    }

    // Only owner can delete a message
    if (del_len == 8) {
        if (!BUFFER_EQUAL_20(hook_acct, otx_acc)) {
            NOPE("Error: Only hook owner can delete messages from birthday card");
        }

        if (state_set(0, 0, del_buf, 8) < 0) {
            NOPE("Error: Could not delete message from birthday card");
        }

        if (count > 0) count--;
        UINT64_TO_BUF(count_buf, count);
        if (state_set(SBUF(count_buf), SBUF(count_key)) < 0) {
            NOPE("Error: Could not update message count on birthday card");
        }

        DONE("Success: Message deleted from birthday card!");
    }

    NOPE("Error: No valid birthday card operation specified");

    GUARD(1);
    return 0;
}