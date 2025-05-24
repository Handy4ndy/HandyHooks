/**
 * HandyHooks - 012 - Hard Multi Emit.
 * This hook limits the incoming payment's and forwards 5 XAH to two hardcoded accounts.
 * Install on ttPayment.
 * Set the exact_amount_value to the desired amount.(16)
 * Set the amountOut to the desired amount.(19)
 * Set ftxn_acc1 and ftxn_acc2 to the desired account address. (22)
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

// Configure an exact amount to recieve
uint64_t exact_amount_value = 10; // 10 XAH

// Configure the amount to emit (In drops)
uint64_t amountOut = 5000000; // 5 XAH

// Configure Account to emit the payment to.
uint8_t ftxn_acc1[20] = { 0x08U, 0x47U, 0x8BU, 0x33U, 0x41U, 0xBBU, 0x50U, 0xFCU, 0x05U, 0x73U, 0x68U, 0xF2U, 0x2AU, 0xF7U, 0x8CU, 0x59U, 0xF8U, 0x5EU, 0x41U, 0xC5U };
uint8_t ftxn_acc2[20] = { 0xA8U, 0x0BU, 0x76U, 0x8AU, 0xDEU, 0xE3U, 0x06U, 0xF6U, 0xC4U, 0x37U, 0xB8U, 0xC1U, 0x97U, 0x6DU, 0x25U, 0xAFU, 0x23U, 0x2BU, 0xD9U, 0x62U };

int64_t hook(uint32_t reserved)
{

    TRACESTR("Hard Multi Emit:: Called.");

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
        DONE("Hard Multi Emit: Outgoing payment transaction accepted");
    }

    // Convert the amount from drops to XAH
    unsigned char amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    double xah_amount = (double)otxn_drops / 1000000.0; // Convert to XAH
    TRACEVAR(xah_amount);

    // Ensure the payment is XAH
    if (amount_len != 8){
        NOPE("Hard Multi Emit: Error: Non-XAH payment rejected.");
    }

    // Check if the payment is equal to the exact amount
    if (xah_amount != exact_amount_value){
        NOPE("Hard Multi Emit: Error: Payment amount doesn't match the exact_amount_value.");
    }

    // Reserve space for two emitted transactions
     etxn_reserve(2);

     // Prepare the first payment transactions
     uint8_t txn1[PREPARE_PAYMENT_SIMPLE_SIZE];
     PREPARE_PAYMENT_SIMPLE(txn, amountOut, ftxn_acc1, 0, 0);

     // Prepare the second payment transaction
     uint8_t txn2[PREPARE_PAYMENT_SIMPLE_SIZE];
     PREPARE_PAYMENT_SIMPLE(txn, amountOut, ftxn_acc2, 0, 0);

     uint8_t emithash1[32];
     uint8_t emithash2[32];

     // Emit the transactions and check if they were successful
     if(emit(SBUF(emithash1), SBUF(txn1)) != 32 ||
        emit(SBUF(emithash2), SBUF(txn2)) != 32)
     {
         NOPE("Hard Multi Emit: Error: Failed to emit transactions");
     }
     
 
    DONE("Hard Multi Emit: Payment recieved and forwarded Successfully");

    _g(1, 1);
    return 0;
}
