//**************************************************************
// BirthdayCardHook - Xahau 2nd Birthday Card Example
// Author: @handy_4ndy
//
// Description:
//   Anyone can add a birthday message on-chain.
//   Only the hook owner can delete messages (by account ID).
//
// Parameters:
//   'MSG' (bytes): The message to add.
//   'DEL' (20 bytes): The account ID whose message should be deleted (owner only).
//
// Usage:
//   - To add a message: Send an Invoke transaction with 'MSG' parameter.
//   - To delete a message: Hook owner sends 'DEL' (20 bytes, account ID).
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

    // Get hook account and transaction account
    uint8_t hook_acct[20];
    hook_account(hook_acct, 20);

    uint8_t otx_acc[20];
    otxn_field(otx_acc, 20, sfAccount);

    // Ensure transaction is an Invoke
    int64_t tt = otxn_type();
    if (tt != 99) {
        NOPE("Error: Transaction must be an Invoke");
    }

    // Get parameters
    uint8_t msg_buf[1024];
    uint8_t msg_key[3] = {'M', 'S', 'G'};
    int64_t msg_len = otxn_param(SBUF(msg_buf), SBUF(msg_key));

    uint8_t del_buf[20];
    uint8_t del_key[3] = {'D', 'E', 'L'};
    int64_t del_len = otxn_param(SBUF(del_buf), SBUF(del_key));

    // Anyone can add a message
    if (msg_len > 0) {
        // Namespace: first 20 bytes = sender, rest zero
        uint8_t ns[32];
        int i = 0;
        for (; i < 20; ++i) ns[i] = otx_acc[i];
        for (; i < 32; ++i) ns[i] = 0;

        uint8_t msg_key_data[32] = "BIRTHDAY_MSG";
        for (int k = 12; k < 32; ++k) msg_key_data[k] = 0;

        if (state_foreign_set(msg_buf, msg_len, msg_key_data, 32, ns, 32, hook_acct, 20) < 0) {
            NOPE("Error: Could not add message to birthday card");
        }

        DONE("Success: Message added to birthday card!");
    }

    // Only owner can delete a message by account ID
    if (del_len == 20) {
        if (!BUFFER_EQUAL_20(hook_acct, otx_acc)) {
            NOPE("Error: Only hook owner can delete messages from birthday card");
        }

        // Namespace: first 20 bytes = account ID, rest zero
        uint8_t ns[32];
        int i = 0;
        for (; i < 20; ++i) ns[i] = del_buf[i];
        for (; i < 32; ++i) ns[i] = 0;

        uint8_t msg_key_data[32] = "BIRTHDAY_MSG";
        for (int k = 12; k < 32; ++k) msg_key_data[k] = 0;

        if (state_foreign_set(0, 0, msg_key_data, 32, ns, 32, hook_acct, 20) < 0) {
            NOPE("Error: Could not delete message from birthday card");
        }

        DONE("Success: Message deleted from birthday card!");
    }

    NOPE("Error: No valid birthday card operation specified");

    GUARD(1);
    return 0;
}
