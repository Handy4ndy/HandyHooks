/**
 * HandyHooks - 003 - Accept In.
 * This hook only accepts incoming payment transactions.
 * Install on ttPayment.
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

int64_t hook(uint32_t reserved ) {

    TRACESTR("Accept In: Called.");

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
         NOPE("Accept In: Outgoing payment transaction rejected.");

        } else{
            DONE("Accept In: Incoming payment transaction accepted.");
        }


    _g(1,1);
    return 0;
}