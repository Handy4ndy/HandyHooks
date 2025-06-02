/**
 * HandyHooks - 013 - Install Emit.
 * This hook limits the incoming payment and forwards an amount to another acount.
 * Install on ttPayment with the following hook params: AM, AO, A1
 * Convert decimal to 32bit hex (flipped https://hooks.services/tools/decimal-to-hex-to-fliphex).
 * Example: "AM": "0a000000", "AO": "404b4c00".
 * Convert rADDRESS to Account ID (https://hooks.services/tools/raddress-to-accountid).
 * Example: "A1": "05A506F1996C9E045C1CE7EE7DF7E599C19A6D52".
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)


int64_t hook(uint32_t reserved)
{

    TRACESTR("Install Emit:: Called.");

    // ACCOUNT: Hook Account
    uint8_t hook_acc[20];
    hook_account(hook_acc, 20);

    // ACCOUNT: Origin Tx Account
    uint8_t otxn_acc[20];
    otxn_field(otxn_acc, 20, sfAccount);


        uint32_t exactAmount;
        // Get the amount to recieve from the hook param
        if (hook_param(SVAR(exactAmount), "AM", 2) != 4)
            NOPE("Install Emit: Error: Payment exactAmount not set");

            if (exactAmount == 0)
                NOPE("Install Emit: Error: amountOut cannot be zero");

        uint32_t amountOut;
        // Get the amount to send from the hook param (In drops)
        if (hook_param(SVAR(amountOut), "AO", 2) != 4)
            NOPE("Install Emit: Error: Payment amountOut not set");

            if (amountOut == 0)
                NOPE("Install Emit: Error: amountOut cannot be zero");

        uint8_t ftxn_acc[20];
        // Get the first account from the hook param
        if (hook_param(SBUF(ftxn_acc), "A1", 2) != 20)
            NOPE("Install Emit: Error: Account A1 not set");

        
        // Ensure the accounts are unique
        if (BUFFER_EQUAL_20(ftxn_acc, hook_acc))
            NOPE("Install Emit: Error: Matching accounts found. Bailing..");

    
    // To know the type of origin txn
    int64_t tt = otxn_type();

    // If the transaction is outgoing from the hook account, accept it
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc) && tt == ttPAYMENT)
    {
        DONE("Install Emit: Outgoing payment transaction accepted");
    }

    // Convert the amount from drops to XAH
    unsigned char amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    double xah_amount = (double)otxn_drops / 1000000.0; // Convert to XAH
    TRACEVAR(xah_amount);

    // Ensure the payment is XAH
    if (amount_len != 8){
        NOPE("Install Emit: Error: Non-XAH payment rejected.");
    }

    // Check if the payment is equal to the exact amount
    if (xah_amount != exactAmount){
        NOPE("Install Emit: Error: Payment amount doesn't match the exact_amount_value.");
    }

    // Reserve space for the emitted transaction
     etxn_reserve(1);

     // Prepare the first payment transactions
     uint8_t txn[PREPARE_PAYMENT_SIMPLE_SIZE];
     PREPARE_PAYMENT_SIMPLE(txn, amountOut, ftxn_acc, 0, 0);

     uint8_t emithash[32];

     // Emit the transaction and check if it was successful
     if(emit(SBUF(emithash), SBUF(txn)) != 32) {
         NOPE("Install Emit: Error: Failed to emit transactions");
     }
     
    DONE("Install Emit: Payment recieved and forwarded Successfully");

    _g(1, 1);
    return 0;
}