/**
 * HandyHooks - Reject Incoming IOU Payment.
 * This hook has various combinations that reject an incoming IOU payment.
 *
 * Uncomment sections below to customize this hook's behavior.
 * 1.  Only reject IOU payments.
 * 2.  Reject IOU payments below a minimum amount.
 * 3.  Reject IOU payments above a maximum amount.
 * 4.  Reject if not exact IOU amount.
 * 5.  Reject if not a multiple of a fixed IOU amount.
 *
 * Install on ttPayment.
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

int64_t hook(uint32_t reserved)
{
    TRACESTR("RIP :: Reject Incoming IOU Payment :: Called.");

    // Get the Hook account
    uint8_t hook_acc[20];
    int64_t res = hook_account(SBUF(hook_acc));
    if (res < 0)
        NOPE("RIP Hook :: Failed to get hook account.");
    TRACEHEX(hook_acc);

    // Get the Originating account of the transaction
    uint8_t otxn_acc[20];
    res = otxn_field(SBUF(otxn_acc), sfAccount);
    if (res < 0)
        NOPE("RIP Hook :: Failed to get otxn account.");
    TRACEHEX(otxn_acc);

    // Accept all outgoing payments
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc)) {
        DONE("RIP Hook :: Accepted :: Outgoing payment.");
    }

    //
    // 1. Only reject incoming IOU payments
    //
        // Get the incoming payment amount
        uint8_t buffer[48];
        res = otxn_field(SBUF(buffer), sfAmount);
        if (res != 48)
            DONE("RIP Hook :: Accepted :: Not an incoming IOU payment.");
        NOPE("RIP Hook :: Rejected :: Incoming IOU payment.");

    // //
    // // 2. Reject IOU payments below a minimum amount
    // //

    // // Configure the minimum amount
    // uint64_t min_amount = 10;
    // uint64_t min_drops = (min_amount * 1000000);

    //     // Get the incoming payment amount
    //     uint8_t buffer[48];
    //     res = otxn_field(SBUF(buffer), sfAmount);
    //     int64_t amount_xfl = -INT64_FROM_BUF(buffer);
    //     int64_t amount_drops = float_int(amount_xfl, 6, 1);

    //         // Check if the incoming amount is an IOU Token
    //         if (res != 48)
    //             NOPE("RIP Hook :: Rejected :: Not an incoming IOU payment.");

    //         // Check if the incoming amount is below the minimum amount
    //         if (amount_drops < min_drops)
    //             NOPE("RIP Hook :: Rejected :: Incoming IOU payment is below minimum amount.");

    // DONE("RIP Hook :: Accepted :: Incoming IOU payment above minimum amount.");

    // //
    // // 3. Reject IOU payments above a maximum amount
    // //

    // // Configure the maximum amount
    // uint64_t max_amount = 10;
    // uint64_t max_drops = (max_amount * 1000000);

    //     // Get the incoming payment amount
    //     uint8_t buffer[48];
    //     res = otxn_field(SBUF(buffer), sfAmount);
    //     int64_t amount_xfl = -INT64_FROM_BUF(buffer);
    //     int64_t amount_drops = float_int(amount_xfl, 6, 1);

    //         // Check if the incoming amount is an IOU Token
    //         if (res != 48)
    //             NOPE("RIP Hook :: Rejected :: Not an incoming IOU payment.");

    //         // Check if the incoming amount is above the maximum amount
    //         if (amount_drops > max_drops)
    //             NOPE("RIP Hook :: Rejected :: Incoming IOU payment is above maximum amount.");

    // DONE("RIP Hook :: Accepted :: Incoming IOU payment below maximum amount.");

    // //
    // // 4. Reject if not exact IOU amount
    // //

    // // Configure the exact amount
    // uint64_t exact_amount = 10;
    // uint64_t exact_drops = (exact_amount * 1000000);

    //     // Get the incoming payment amount
    //     uint8_t buffer[48];
    //     res = otxn_field(SBUF(buffer), sfAmount);
    //     int64_t amount_xfl = -INT64_FROM_BUF(buffer);
    //     int64_t amount_drops = float_int(amount_xfl, 6, 1);

    //         // Check if the incoming amount is an IOU Token
    //         if (res != 48)
    //             NOPE("RIP Hook :: Rejected :: Not an incoming IOU payment.");

    //         // Check if the incoming amount matches the exact amount
    //         if (amount_drops != exact_drops) {
    //             if (amount_drops > exact_drops)
    //                 NOPE("RIP Hook :: Rejected :: Incoming IOU payment is above the required amount.");
    //             else
    //                 NOPE("RIP Hook :: Rejected :: Incoming IOU payment is below the required amount.");
    //         }
            
    // DONE("RIP Hook :: Accepted :: Incoming IOU payment matches exact amount.");

    // //
    // // 5. Reject if not a multiple of a fixed IOU amount
    // //

    // // Configure the fixed IOU cost
    // uint64_t cost_IOU = 10;
    // uint64_t multi_count = 0;
    // uint64_t cost_drops = (cost_IOU * 1000000);

    //     // Get the incoming payment amount
    //     uint8_t buffer[48];
    //     res = otxn_field(SBUF(buffer), sfAmount);
    //     int64_t amount_xfl = -INT64_FROM_BUF(buffer);
    //     int64_t amount_drops = float_int(amount_xfl, 6, 1);
    //     int64_t amount_int = float_int(amount_xfl, 0, 1);
            
    //         // Check if the incoming amount is an IOU Token
    //         if (res != 48)
    //         NOPE("RIP Hook :: Rejected :: Not an incoming IOU payment.");

    //     TRACEVAR(amount_int);
    //     TRACEVAR(cost_IOU);
            
    //         // Check if the incoming amount matches the required amount
    //         if (amount_drops == cost_drops) {
    //             DONE("RIP Hook :: Accepted :: Payment matches the required amount.");
    //         }

    //         // Calculate if the payment is a multiple of the cost
    //         multi_count = amount_drops / cost_drops;
    //         if (multi_count == 0) {
    //             NOPE("RIP Hook :: Rejected :: Payment is less than the required amount!");
    //         }
    //         if (amount_drops % cost_drops != 0) {
    //             NOPE("RIP Hook :: Rejected :: Payment amount must be a multiple of the required!");
    //         }
    
    // DONE("RIP Hook :: Accepted :: Incoming IOU payment is a multiple of the required amount.");

    _g(1, 1);
    return 0;
}
