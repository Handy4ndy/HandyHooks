/**
 * HandyHooks - Native Logging Example
 * Demonstrates how to use tracing and logging macros in a Xahau Hook.
 * This example logs account info and payment details, and accepts incoming XAH payments above a minimum amount.
 * 
 * Uncomment sections below to test additional logging scenarios.
 * 
 * Install on ttPayment.
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

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
    // 
    //
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc))
        TRACESTR("The Hook Account Matches the Originating Transaction Account.");

    //
    // Accept all Incoming payments
    // 
    //
    if (!BUFFER_EQUAL_20(hook_acc, otxn_acc)){
        TRACESTR("The Hook Account does not Match the Originating Transaction Account.");
        }
        
    //
    // 1. Accept incoming XAH payment above a minimum amount
    //

    // Configure the minimum amount
    uint64_t min_amount = 1; // 1 XAH
    uint8_t amount_buffer[48];
    uint64_t min_drops = (min_amount * 1000000);

        TRACEVAR(min_amount);
        TRACEVAR(min_drops);
    
        // Check the incoming amount is XAH
        int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
            if (amount_len != 8)
            NOPE("AIP Hook :: Rejected :: Incoming IOU rejected.");

        TRACEVAR(amount_len);
        TRACEVAR(amount_buffer);

            // Convert amount to drops to compare
            int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
            double xah_amount = (double)otxn_drops / 1000000.0;

        TRACEVAR(otxn_drops);
        TRACEVAR(xah_amount);

            if (otxn_drops < min_drops)
                NOPE("AIP Hook :: Rejected :: Incoming payment is below minimum amount.");

    DONE("AIP Hook :: Accepted :: Incoming amount above the minimum amount threshold.");

    // //
    // // 2. Accept a multiple of a fixed amount
    // //

    // // Configure the cost in XAH
    // uint64_t cost_xah = 10;
    // uint64_t multi_count = 0;
    // uint64_t cost_drops = cost_xah * 1000000; // Convert XAH to drops

    
    //     // Convert the amount from drops to XAH
    //     unsigned char amount_buffer[48];
    //     int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    //     int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    //     double xah_amount = (double)otxn_drops / 1000000.0; // Convert to XAH

    //     TRACEVAR(cost_xah);
    //     TRACEVAR(xah_amount);
    //     TRACEVAR(cost_drops);
    //     TRACEVAR(otxn_drops);

    //         // Ensure the payment is XAH
    //         if (amount_len != 8){
    //             NOPE("AIP Hook :: Rejected :: Non-XAH payment rejected.");
    //         }

    //         // Check if the payment is equal to the cost
    //         if (otxn_drops == cost_drops) {
    //             DONE("AIP Hook :: Accepted :: Payment matches the required amount.");
    //         }

    //         // Calculate if the payment is a multiple of the cost
    //         multi_count = otxn_drops / cost_drops;
    //         if (multi_count == 0) {
    //             NOPE("AIP Hook :: Rejected :: Payment is less than the required amount!");
    //         }
    //         if (otxn_drops % cost_drops != 0) {
    //             NOPE("AIP Hook :: Rejected :: Payment amount must be a multiple of the required!");
    //         }

    //     TRACEVAR(multi_count);

    // DONE("AIP Hook :: Accepted :: Incoming XAH payment is a multiple of the required amount.");

    _g(1,1);
    return 0;
}