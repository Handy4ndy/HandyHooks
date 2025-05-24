/**
 * HandyHooks - 011 - Hard Emit.
 * This hook limits the incoming payment's and sends 5 XAH to the ftxn_acc.
 * Install on ttPayment.
 * Set the exact_amount_value to the desired amount.(16)
 * Set the amountOut to the desired amount.(19)
 * Set the ftxn_acc to the desired account address. (22)
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

// Configure an exact amount to recieve
uint64_t exact_amount_value = 10; // 10 XAH

// Configure the amount to emit (In drops)
uint64_t amountOut = 5000000; // 5 XAH

// Configure Account to emit the payment to
uint8_t ftxn_acc[20] = { 0x08U, 0x47U, 0x8BU, 0x33U, 0x41U, 0xBBU, 0x50U, 0xFCU, 0x05U, 0x73U, 0x68U, 0xF2U, 0x2AU, 0xF7U, 0x8CU, 0x59U, 0xF8U, 0x5EU, 0x41U, 0xC5U };

int64_t hook(uint32_t reserved)
{

    TRACESTR("Hard Emit:: Called.");

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
        DONE("Hard Emit: Outgoing payment transaction accepted");
    }

    // Convert the amount from drops to XAH
    unsigned char amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    double xah_amount = (double)otxn_drops / 1000000.0; // Convert to XAH
    TRACEVAR(xah_amount);

    // Ensure the payment is XAH
    if (amount_len != 8){
        NOPE("Hard Emit: Error: Non-XAH payment rejected.");
    }

    // Check if the payment is equal to the exact amount
    if (xah_amount != exact_amount_value){
        NOPE("Hard Emit: Error: Payment amount doesn't match the exact_amount_value.");
    }

    // Reserve space for emitted transaction
     etxn_reserve(1);

     uint8_t txn[PREPARE_PAYMENT_SIMPLE_SIZE];
     // Prepare the payment transactions
     PREPARE_PAYMENT_SIMPLE(txn, amountOut, ftxn_acc, 0, 0);

     uint8_t emithash[32];

     // Emit the transactions and check if they were successful
     if (emit(SBUF(emithash), SBUF(txn)) != 32) {
         NOPE("Hard Emit: Error: Failed to emit transactions");
     }
 
     DONE("Hard Emit: Payment recieved and forwarded Successfully");

    _g(1, 1);
    return 0;
}
