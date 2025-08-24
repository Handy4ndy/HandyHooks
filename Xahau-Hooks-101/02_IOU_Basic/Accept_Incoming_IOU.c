/**
 * HandyHooks - Accept Incoming Payment.
 * This hook has various combinations that accept an incoming IOU payment.
 *
 * Uncomment sections below to customize this hooks behavior.
 * 1.  Only accept IOU payments.
 * 2.  Accept IOU payments above a minimum amount.
 * 3.  Accept IOU payments below a maximum amount.
 * 4.  Accept exact IOU amount.
 * 5.  Accept a multiple of a fixed IOU amount.
 *
 * Install on ttPayment.
 */

#include "hookapi.h"

// Define convenience macros
#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

int64_t hook(uint32_t reserved)
{

    TRACESTR("AIP :: Accept Incoming Payment :: Called.");

    // Get the Hook account
    uint8_t hook_acc[20];
        int64_t res = hook_account(SBUF(hook_acc));
            if (res < 0)
                NOPE("AIP Hook :: Failed to get hook account.");

    TRACEHEX(hook_acc);

    // Get the Originating account of the transaction
    uint8_t otxn_acc[20];
        res = otxn_field(SBUF(otxn_acc), sfAccount);
            if (res < 0)
                NOPE("AIP Hook :: Failed to get otxn account.");

    TRACEHEX(otxn_acc);

    // Accept all outgoing payments
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc))
    {
        DONE("AIP Hook :: Accepted :: Outgoing payment.");
    }

    // UNCOMMENT SECTIONS BELOW TO TEST FUNCTIONALITY

    //
    // 1. Only accept incoming IOU Payments.
    //  
        // Get the incoming payment amount
        uint8_t buffer[48];
        res =otxn_field(SBUF(buffer), sfAmount);
        int64_t amount_xfl = -INT64_FROM_BUF(buffer);
        int64_t amount_int = float_int(amount_xfl, 0, 1);

            // Check if the incoming amount is an IOU Token
            if (res != 48) 
                NOPE("AIP Hook :: Rejected :: Not an incoming IOU payment.");
            
        DONE("AIP Hook :: Accepted :: Incoming IOU payment.");
            

    // //
    // // 2. Accept IOU payments above a minimum amount
    // //

    // // Configure the minimum amount
    // uint64_t min_amount = 10;
    // uint64_t min_drops = (min_amount * 1000000);

    //     // Get the incoming payment amount
    //     uint8_t buffer[48];
    //     res =otxn_field(SBUF(buffer), sfAmount);
    //     int64_t amount_xfl = -INT64_FROM_BUF(buffer);
    //     int64_t amount_drops = float_int(amount_xfl, 6, 1);

    //         // Check the incoming amount is an IOU Token
    //         if (res != 48)
    //             NOPE("AIP Hook :: Rejected :: Incoming XAH rejected.");

    //         // Check if the incoming amount is below the minimum amount
    //         if (amount_drops <= min_drops)
    //             NOPE("AIP Hook :: Rejected :: Incoming payment is below minimum amount.");

    // DONE("AIP Hook :: Accepted :: Incoming amount above the minimum amount threshold.");

    // //
    // // 3. Accept IOU payments below a maximum amount
    // //

    // // Configure the maximum amount
    // uint64_t max_amount = 10; // 10 IOU Tokens
    // uint64_t max_drops = (max_amount * 1000000);

    //     uint8_t buffer[48];
    //     res =otxn_field(SBUF(buffer), sfAmount);
    //     int64_t amount_xfl = -INT64_FROM_BUF(buffer);
    //     int64_t amount_drops = float_int(amount_xfl, 6, 1);

    //         // Check the incoming amount is an IOU Token
    //         if (res != 48)
    //             NOPE("AIP Hook :: Rejected :: Incoming XAH rejected.");

    //         // Check if the incoming amount is above the maximum amount
    //         if (amount_drops >= max_drops)
    //             NOPE("AIP Hook :: Rejected :: Incoming payment is above maximum amount.");

    // DONE("AIP Hook :: Accepted :: Incoming amount below the maximum amount threshold.");

    // //
    // // 4. Accept exact IOU amount
    // //

    // // Configure an exact amount
    // uint64_t exact_amount = 10; // 10 IOU Tokens
    // uint64_t exact_drops = (exact_amount * 1000000);

    //     uint8_t buffer[48];
    //     res =otxn_field(SBUF(buffer), sfAmount);
    //     int64_t amount_xfl = -INT64_FROM_BUF(buffer);
    //     int64_t amount_drops = float_int(amount_xfl, 6, 1);

    //         // Check the incoming amount is an IOU Token
    //         if (res != 48)
    //             NOPE("AIP Hook :: Rejected :: Incoming XAH rejected.");

    //         if (amount_drops != exact_drops) {
    //             if (amount_drops > exact_drops) {
    //                 NOPE("AIP Hook :: Rejected :: Incoming payment is above the required amount.");
    //                 } else {
    //                 NOPE("AIP Hook :: Rejected :: Incoming payment is below the required amount");
    //                 }
    //             }

    // DONE("AIP Hook :: Accepted :: Incoming amount below the maximum amount threshold.");

    // //
    // // 5. Accept a multiple of a fixed IOU amount
    // //

    // // Configure the cost in IOU
    // uint64_t cost_IOU = 10;
    // uint64_t multi_count = 0;
    // uint64_t cost_drops = (cost_IOU * 1000000);

    //     uint8_t buffer[48];
    //     res =otxn_field(SBUF(buffer), sfAmount);
    //     int64_t amount_xfl = -INT64_FROM_BUF(buffer);
    //     int64_t amount_drops = float_int(amount_xfl, 6, 1);
    //     int64_t amount_int = float_int(amount_xfl, 0, 1);

    //         // Check the incoming amount is an IOU Token
    //         if (res != 48)
    //             NOPE("AIP Hook :: Rejected :: Incoming XAH rejected.");

    //     TRACEVAR(amount_int);
    //     TRACEVAR(cost_IOU);

    //         // Check if the payment is equal to the cost
    //         if (amount_drops == cost_drops) {
    //             DONE("AIP Hook :: Accepted :: Payment matches the required amount.");
    //         }

    //         // Calculate if the payment is a multiple of the cost
    //         multi_count =  amount_drops / cost_drops;
    //         if (multi_count == 0) {
    //             NOPE("AIP Hook :: Rejected :: Payment is less than the required amount!");
    //         }
    //         if (amount_drops % cost_drops != 0) {
    //             NOPE("AIP Hook :: Rejected :: Payment amount must be a multiple of the required!");
    //         }

    //     TRACEVAR(multi_count);

    // DONE("AIP Hook :: Accepted :: Incoming XAH payment is a multiple of the required amount.");

    _g(1, 1);
    return 0;
}