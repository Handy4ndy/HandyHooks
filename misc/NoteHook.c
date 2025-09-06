//**************************************************************
// NoteHook - Xahau HandyHook Example
// Author: @handy_4ndy
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

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

// Guard macro to prevent infinite loops
#define GUARD(maxiter) _g(__LINE__, (maxiter)+1)

// Hook entry point
int64_t hook(uint32_t reserved) {
    GUARD(5); // Prevent infinite loops

    // Get hook account
    uint8_t hook_acct[20];
    hook_account(hook_acct, 20);

    // Get origin transaction account
    uint8_t otx_acc[20];
    otxn_field(otx_acc, 20, sfAccount);

    // Verify transaction is from hook owner
    if (!BUFFER_EQUAL_20(hook_acct, otx_acc)) {
        NOPE("Error: Only hook owner can invoke this hook");
    }

    // Check if transaction is an Invoke
    int64_t tt = otxn_type();
    if (tt != 99) {
        NOPE("Error: Transaction must be an Invoke");
    }

    // Get parameters
    uint8_t note_buf[1024]; // Buffer for note data
    uint8_t note_key[3] = {'N', 'O', 'T'}; // Key for note parameter
    int64_t note_len = otxn_param(SBUF(note_buf), SBUF(note_key));

    uint8_t del_buf[8]; // Buffer for delete parameter (note number)
    uint8_t del_key[3] = {'D', 'E', 'L'}; // Key for delete parameter
    int64_t del_len = otxn_param(SBUF(del_buf), SBUF(del_key));

    // Counter for note numbers
    uint8_t count_buf[8];
    uint8_t count_key[3] = {'C', 'N', 'T'};
    uint64_t count = 0;
    if (state(SBUF(count_buf), SBUF(count_key)) >= 0) {
        count = UINT64_FROM_BUF(count_buf);
    }

    // Handle note addition
    if (note_len > 0) {
        // Generate unique key for new note
        uint8_t note_num_buf[8];
        UINT64_TO_BUF(note_num_buf, count + 1);

        // Store the note
        if (state_set(note_buf, note_len, note_num_buf, 8) < 0) {
            NOPE("Error: Could not set note state");
        }

        // Increment and store counter
        count++;
        UINT64_TO_BUF(count_buf, count);
        if (state_set(SBUF(count_buf), SBUF(count_key)) < 0) {
            NOPE("Error: Could not update count state");
        }

        DONE("Success: Note added");
    }

    // Handle note deletion
    if (del_len == 8) {
        // Delete specified note
        if (state_set(0, 0, del_buf, 8) < 0) {
            NOPE("Error: Could not delete note");
        }

        // Decrease and store counter
        count--;
        UINT64_TO_BUF(count_buf, count);
        if (state_set(SBUF(count_buf), SBUF(count_key)) < 0) {
            NOPE("Error: Could not update count state");
        }

        DONE("Success: Note deleted");
    }

    // If no valid operation was performed
    NOPE("Error: No valid operation specified");

    // Final guard
    GUARD(1);
    return 0;
}