/**
 * HandyHooks - 010 - Accept Emit.
 * This hook allows outgoing transactions, limits the incoming payment and returns 5 XAH.
 * Install on ttPayment.
 * Set the exact_amount_value to the desired amount.(48)
 * Set the amountOut to the desired amount.(59)
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

int64_t hook(uint32_t reserved)
{

    TRACESTR("Accept Emit:: Called.");

    // ACCOUNT: Hook Account
    uint8_t hook_acc[20];
    hook_account(hook_acc, 20);

    // ACCOUNT: Origin Tx Account
    uint8_t otxn_acc[20];
    otxn_field(otxn_acc, 20, sfAccount);

    // To know the type of origin txn
    int64_t tt = otxn_type();

    // If the transaction is outgoing from the hook account, accept it
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc) && tt == ttPAYMENT)
    {
        DONE("Accept Emit:: Outgoing payment transaction accepted");
    }

    // fetch the sent Amount
    unsigned char amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    double xah_amount = (double)otxn_drops / 1000000.0; // Convert to XAH
    TRACEVAR(xah_amount);

    // Ensure the payment is XAH
    if (amount_len != 8){
        NOPE("Accept Emit: Error: Non-XAH payment rejected.");
    }

    // Configure a minimum amount
    uint64_t exact_amount_value = 10; // 10 XAH

    // Check if the payment is less than the minimum amount
    if (xah_amount != exact_amount_value){
        NOPE("Accept Emit: Error: Payment amount doesn't match the exact_amount_value.");
    }

    // Reserve space for three emitted transactions
     etxn_reserve(1);

     uint64_t amountOut = 50000000; // 5 XAH

     uint8_t txn[PREPARE_PAYMENT_SIMPLE_SIZE];
     // Prepare the payment transactions
     PREPARE_PAYMENT_SIMPLE(txn, amountOut, otxn_acc, 0, 0);

     uint8_t emithash[32];

     // Emit the transactions and check if they were successful
     if (emit(SBUF(emithash), SBUF(txn)) != 32) {
         NOPE("Accept Emit: Error: Failed to emit transactions");
     }
 
     DONE("Accept Emit: Payment recieved and forwarded Successfully");

    _g(1, 1);
    return 0;
}
