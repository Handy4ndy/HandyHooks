/**
 * HandyHooks - Accept Incoming XAH Payment.
 * This hook has various combinations that accept an incoming payment.
 * 
 * Uncomment sections below to customize this hooks behavior.
 * 1.  Simply accept the incoming payment.
 * 2.  Only accept incoming payments.
 * 3.  Accept XAH payments.
 * 4.  Only accept XAH payments.
 * 5.  Accept IOU payments.
 * 6.  Only accept IOU payments.
 * 7.  Accept XAH payments above a minimum amount.
 * 8.  Accept XAH payments below a maximum amount.
 * 9.  Accept exact amount.
 * 10. Accept a multiple of a fixed amount.
 *
 * Install on ttPayment.
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

// Configure the amount to emit (In drops)
uint64_t amountOut = 5000000; // 5 XAH

// Configure Account to emit the payment to
uint8_t ftxn_acc[20] = { 0xB9U, 0x59U, 0xD6U, 0xACU, 0x9DU, 0x15U, 0x89U, 0x17U, 0xECU, 0x26U, 0x9EU, 0xACU, 0x05U, 0x70U, 0x19U, 0x07U, 0x4FU, 0x56U, 0xD6U, 0x39U };


int64_t hook(uint32_t reserved) {

    TRACESTR("AIP :: Accept Incoming Payment :: Called.");

    // Get the Hook account
    uint8_t hook_acc[20];
    hook_account(SBUF(hook_acc));

    TRACEHEX(hook_acc);

    // Get the Originating account of the transaction
    uint8_t otxn_acc[20];
    otxn_field(SBUF(otxn_acc), sfAccount);

    TRACEHEX(otxn_acc);

    //
    // Accept all outgoing payments
    // ** Comment out for testing purposes **
    //
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc)){
        DONE("AIP Hook :: Accepted :: Outgoing payment.");
    }
    //
    // 10. Accept a multiple of a fixed amount
    //

    // Configure the cost in XAH
    uint64_t cost_xah = 10;
    uint64_t multi_count = 0;
    uint64_t cost_drops = cost_xah * 1000000; // Convert XAH to drops

        // Convert the amount from drops to XAH
        unsigned char amount_buffer[48];
        int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
        int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
        double xah_amount = (double)otxn_drops / 1000000.0; // Convert to XAH

        TRACEVAR(xah_amount);
        TRACEVAR(cost_xah);
        TRACEVAR(cost_drops);
        TRACEVAR(otxn_drops);

            // Ensure the payment is XAH
            if (amount_len != 8){
                NOPE("AIP Hook :: Rejected :: Non-XAH payment rejected.");
            }

            // Check if the payment is equal to the cost
            if (otxn_drops == cost_drops) {
                DONE("AIP Hook :: Accepted :: Payment matches the required amount.");
            }

            // Calculate if the payment is a multiple of the cost
            multi_count = otxn_drops / cost_drops;
            if (multi_count == 0) {
                NOPE("AIP Hook :: Rejected :: Payment is less than the required amount!");
            }
            if (otxn_drops % cost_drops != 0) {
                NOPE("AIP Hook :: Rejected :: Payment amount must be a multiple of the required!");
            }

        TRACEVAR(multi_count);

    // DONE("AIP Hook :: Accepted :: Incoming XAH payment is a multiple of the required amount.");

    // multi cost is finished here moving onto dev payment


    TRACESTR("Dev Emit:: Called.");

    // Reserve space for emitted transaction
     etxn_reserve(1);

     uint8_t txn[PREPARE_PAYMENT_SIMPLE_SIZE];
     // Prepare the payment transactions
     PREPARE_PAYMENT_SIMPLE(txn, amountOut, ftxn_acc, 0, 0);

     uint8_t emithash[32];

     // Emit the transactions and check if they were successful
     if (emit(SBUF(emithash), SBUF(txn)) != 32) {
         NOPE("Dev Emit: Error: Failed to emit transactions");
     } else {
         DONE("Dev Emit: Hook initiated and Payment forwarded Successfully");
     }

     _g(1,1);
    return 0;
}