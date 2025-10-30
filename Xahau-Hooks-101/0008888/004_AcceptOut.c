/**
 * HandyHooks - 004 - Accept Out.
 * This hook only accepts outgoing payment transactions.
 * Install on ttPayment.
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

int64_t hook(uint32_t reserved ) {

    TRACESTR("Accept Out: Called.");s

    // Get the hook account
     uint8_t hook_acc[20];
     hook_account(SBUF(hook_acc));

     TRACEHEX(hook_acc)
 
     // Get the originating account of the transaction
     uint8_t otxn_acc[20];
     otxn_field(SBUF(otxn_acc), sfAccount);

     TRACEHEX(otxn_acc)
 
     // If the transaction is outgoing from the hook account, accept it
     if (BUFFER_EQUAL_20(hook_acc, otxn_acc)){
         DONE("Accept Out: Outgoing payment transaction accepted.");

        } else{
            NOPE("Accept Out: Incoming payment transaction rejected.");
        }


    _g(1,1);
    return 0;
}