//**************************************************************
// NoteHook (NTH) - Xahau HandyHook Collection
// Author: @Handy_4ndy
//
// Description:
//   This hook allows the owner to add or delete notes on-chain.
//   Only the hook owner can invoke this hook.
//
// Parameters:
//   'NOT' (bytes): The note to add. If present, a new note is stored.
//   'DEL' (8 bytes): The note number to delete. If present, deletes the note with this number.
//   'CNT' (8 bytes, state): Counter for the number of notes stored (managed by the hook).
//
// Usage:
//   - To add a note: Send an Invoke transaction with 'NOT' parameter.
//   - To delete a note: Send an Invoke transaction with 'DEL' parameter (8-byte note number).
//
//**************************************************************
#include "hookapi.h"
#include <stdint.h>

#define DONE(x) accept(SBUF("NTH :: Success :: " x), __LINE__)
#define NOPE(x) rollback(SBUF("NTH :: Error :: " x), __LINE__)


#define GUARD(maxiter) _g(__LINE__, (maxiter)+1)

int64_t hook(uint32_t reserved) {
    GUARD(5);

    TRACESTR("NTH :: NoteHook :: Called");

    uint8_t hook_acct[20];
    hook_account(hook_acct, 20);

    uint8_t otx_acc[20];
    otxn_field(otx_acc, 20, sfAccount);

    if (!BUFFER_EQUAL_20(hook_acct, otx_acc)) {
        NOPE("Error: Only hook owner can invoke this hook");
    }

    int64_t tt = otxn_type();
    if (tt != 99) {
        NOPE("Error: Transaction must be an Invoke");
    }

    uint8_t note_buf[1024];
    uint8_t note_key[3] = {'N', 'O', 'T'}; 
    int64_t note_len = otxn_param(SBUF(note_buf), SBUF(note_key));

    uint8_t del_buf[8]; 
    uint8_t del_key[3] = {'D', 'E', 'L'}; 
    int64_t del_len = otxn_param(SBUF(del_buf), SBUF(del_key));

    uint8_t count_buf[8];
    uint8_t count_key[3] = {'C', 'N', 'T'};
    uint64_t count = 0;
    if (state(SBUF(count_buf), SBUF(count_key)) >= 0) {
        count = UINT64_FROM_BUF(count_buf);
    }

    if (note_len > 0) {
        uint8_t note_num_buf[8];
        UINT64_TO_BUF(note_num_buf, count + 1);

        if (state_set(note_buf, note_len, note_num_buf, 8) < 0) {
            NOPE("Error: Could not set note state");
        }

        count++;
        UINT64_TO_BUF(count_buf, count);
        if (state_set(SBUF(count_buf), SBUF(count_key)) < 0) {
            NOPE("Error: Could not update count state");
        }

        DONE("Success: Note added");
    }

    if (del_len == 8) {

        if (state_set(0, 0, del_buf, 8) < 0) {
            NOPE("Error: Could not delete note");
        }

        count--;
        UINT64_TO_BUF(count_buf, count);
        if (state_set(SBUF(count_buf), SBUF(count_key)) < 0) {
            NOPE("Error: Could not update count state");
        }

        DONE("Success: Note deleted");
    }

    NOPE("Error: No valid operation specified");

    // Final guard
    GUARD(1);
    return 0;
}