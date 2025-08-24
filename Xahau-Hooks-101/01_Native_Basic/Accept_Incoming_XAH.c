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
    

    // UNCOMMENT SECTIONS BELOW TO TEST FUNCTIONALITY

    //
    // 1. Simply accept the incoming payment.
    //
        DONE("AIP Hook :: Accepted :: Incoming payment.");

    // //
    // // 2. Only accept incoming Payments. 
    // // ** Requires Accept all outgoing payments above to be commented **
    // //
    //     if (BUFFER_EQUAL_20(hook_acc, otxn_acc)){
    //         NOPE("AIP Hook :: Rejected :: Outgoing payment.");
    //     } else{
    //         DONE("AIP Hook :: Accepted :: Incoming payment.");
    //     }

    // //
    // // 3. Accept incoming XAH Payment.
    // //
    // uint8_t amount[8];
    //     if (otxn_field(SBUF(amount),sfAmount) == 8)
    //         DONE("AIP Hook :: Accepted :: Incoming XAH payment.");

    // //
    // // 4. Only accept incoming XAH Payment.
    // //
    // uint8_t xah_amount[8];
    //     if (otxn_field(SBUF(xah_amount),sfAmount) == 8){
    //         DONE("AIP Hook :: Accepted :: Incoming XAH payment.");
    //     } else {
    //         NOPE("AIP Hook :: Rejected :: Incoming IOU payment.");
    //     }
    
    // //
    // // 5. Accept incoming IOU 
    // //
    // uint8_t iou_amount[48];
    //     if (otxn_field(SBUF(iou_amount),sfAmount) != 8)
    //         DONE("AIP Hook :: Accepted :: Incoming IOU payment.");

    // //
    // // 6. Only accept incoming IOU payments
    // //
    // uint8_t IOU_amount[48];
    //     if (otxn_field(SBUF(IOU_amount),sfAmount) == 8){
    //         NOPE("AIP Hook :: Rejected :: Incoming XAH payment.");
    //         } else {
    //         DONE("AIP Hook :: Accepted :: Incoming IOU payment.");
    //     }

    // //
    // // 7. Accept incoming XAH payment above a minimum amount
    // //

    // // Configure the minimum amount
    // uint64_t min_amount = 1; // 1 XAH
    // uint8_t amount_buffer[48];

    //     // Check the incoming amount is XAH
    //     int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    //         if (amount_len != 8)
    //             NOPE("AIP Hook :: Rejected :: Incoming IOU rejected.");

    //     // Convert amount to drops to compare
    //     int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    //     double xah_amount = (double)otxn_drops / 1000000.0;
    //         if (xah_amount < min_amount)
    //             NOPE("AIP Hook :: Rejected :: Incoming payment is below minimum amount.");

    // DONE("AIP Hook :: Accepted :: Incoming amount above the minimum amount threshold.");

    // //
    // // 8. Accept incoming XAH payment below a maximum amount
    // //

    // // Configure the maximum amount
    // uint64_t max_amount = 100; // 100 XAH

    //     // Check the incoming amount is XAH
    //     uint8_t amount_buffer[48];
    //     int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    //         if (amount_len != 8)
    //             NOPE("AIP Hook :: Rejected :: Incoming IOU rejected.");

    //     // Convert amount to drops to compare
    //     int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    //     double xah_amount = (double)otxn_drops / 1000000.0;
    //         if (xah_amount > max_amount)
    //             NOPE("AIP Hook :: Rejected :: Incoming Payment above maximum amount.");

    // DONE("AIP Hook :: Accepted :: Incoming amount below the maximum amount threshold.");


    // //
    // // 9. Accept exact incoming incoming amount
    // //

    // // Configure an Exact amount
    // uint64_t exact_amount = 10; // 10 XAH
    // TRACEVAR(exact_amount);

    // // Convert the amount from drops to XAH
    //     uint8_t amount_buffer[48];
    //     int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    //     int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    //     double xah_amount = (double)otxn_drops / 1000000.0; // Convert to XAH
    //     TRACEVAR(xah_amount);

    //         // Ensure the payment is XAH
    //         if (amount_len != 8){
    //             NOPE("AIP Hook :: Rejected :: Incoming IOU rejected.");
    //         }

    //         // Check if the payment is equal to the exact amount
    //         if (xah_amount != exact_amount){
    //             NOPE("AIP Hook :: Rejected :: Incoming Payment doesn't match the amount required.");
    //         }

    // DONE("AIP Hook :: Accepted :: Incoming Payment matches the amount required.");

    // //
    // // 10. Accept a multiple of a fixed amount
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

    //     TRACEVAR(xah_amount);
    //     TRACEVAR(cost_xah);
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