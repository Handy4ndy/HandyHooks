/**
 * HandyHooks - 005 - Accept XAH.
 * This hook only accepts XAH Payments, but allows outgoing transactions.
 * Install on ttPayment.
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

int64_t hook(uint32_t reserved) {
    TRACESTR("Accept XAH: Called.");

    // Get the hook account
     uint8_t hook_acc[20];
     hook_account(SBUF(hook_acc));

     TRACEHEX(hook_acc)
 
     // Get the originating account of the transaction
     uint8_t otxn_acc[20];
     otxn_field(SBUF(otxn_acc), sfAccount);

     TRACEHEX(otxn_acc)
 
     // If the transaction is outgoing from the hook account, accept before checking if it is XAH
     if (BUFFER_EQUAL_20(hook_acc, otxn_acc))
         DONE("Accept XAH: Outgoing payment transaction accepted.");
 
     // Get the transaction amount.
     uint8_t amount[8];
     if (otxn_field(SBUF(amount), sfAmount) == 8)
         DONE("Accept XAH: Incoming XAH payment accepted.");
 
    NOPE("Accept XAH: Non-XAH Payment rejected.");

    _g(1,1);
    return 0;
}