/**
 * HandyHooks - Reject Outgoing Payment.
 * This hook has various combinations that reject an Outgoing IOU payment.
 *
 * Uncomment sections below to customize this hooks behavior.
 * 1.  Only reject IOU payments.
 * 2.  Reject IOU payments below a minimum amount.
 * 3.  Reject IOU payments above a maximum amount.
 * 4.  Reject IOU payments not matching an exact amount.
 * 5.  Reject IOU payments not being a multiple of a fixed amount.
 *
 * Install on ttPayment.
 */

#include "hookapi.h"

// Define convenience macros
#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

int64_t hook(uint32_t reserved)
{

    TRACESTR("ROP :: Reject Outgoing Payment :: Called.");

    // Get the Hook account
    uint8_t hook_acc[20];
    int64_t res = hook_account(SBUF(hook_acc));
    if (res < 0)
        NOPE("ROP Hook :: Failed to get hook account.");

    TRACEHEX(hook_acc);

    // Get the Originating account of the transaction
    uint8_t otxn_acc[20];
    res = otxn_field(SBUF(otxn_acc), sfAccount);
    if (res < 0)
        NOPE("ROP Hook :: Failed to get otxn account.");

    TRACEHEX(otxn_acc);

    // Accept all incoming payments
    if (!BUFFER_EQUAL_20(hook_acc, otxn_acc)) {
        DONE("ROP Hook :: Accepted :: Incoming payment.");
    }

    //
    // 1. Reject all outgoing IOU payments
    //

        uint8_t buffer[48];
        res = otxn_field(SBUF(buffer), sfAmount);
            if (res == 48) {
                NOPE("ROP Hook :: Rejected :: Outgoing IOU payment.");
            }

    // Not an IOU payment, accept
    DONE("ROP Hook :: Accepted :: Outgoing non-IOU payment.");
    
    //
    // 2. Reject outgoing IOU payments below a minimum amount
    //

    // Configure the minimum Amount
    uint64_t min_amount = 10;
    uint64_t min_drops = (min_amount * 1000000);

        // Get the outgoing payment amount
        uint8_t buffer[48];
        res = otxn_field(SBUF(buffer), sfAmount);
        int64_t amount_xfl = -INT64_FROM_BUF(buffer);
        int64_t amount_drops = float_int(amount_xfl, 6, 1);

            // Check if the outgoing payment is an IOU
            if (res != 48)
                DONE("ROP Hook :: Accepted :: Outgoing non-IOU payment.");

            // Check if the outgoing payment is below the minimum amount
            if (amount_drops < min_drops)
                NOPE("ROP Hook :: Rejected :: Outgoing IOU payment is below minimum amount.");

    DONE("ROP Hook :: Accepted :: Outgoing IOU payment above minimum amount.");

    //
    // 3. Reject outgoing IOU payments above a maximum amount
    //

    // Configure the maximum Amount
    uint64_t max_amount = 100;
    uint64_t max_drops = (max_amount * 1000000);

        // Get the outgoing payment amount
        uint8_t buffer[48];
        res = otxn_field(SBUF(buffer), sfAmount);
        int64_t amount_xfl = -INT64_FROM_BUF(buffer);
        int64_t amount_drops = float_int(amount_xfl, 6, 1);

            // Check if the outgoing payment is an IOU
            if (res != 48) 
                DONE("ROP Hook :: Accepted :: Outgoing non-IOU payment.");

            // Check if the outgoing payment is above the maximum amount
            if (amount_drops > max_drops)
                NOPE("ROP Hook :: Rejected :: Outgoing IOU payment is above maximum amount.");

    DONE("ROP Hook :: Accepted :: Outgoing IOU payment below maximum amount.");

    //
    // 4. Reject outgoing IOU payments not matching an exact amount
    //

    // Configure the exact Amount
    uint64_t exact_amount = 10;
    uint64_t exact_drops = (exact_amount * 1000000);

        // Get the outgoing payment amount
        uint8_t buffer[48];
        res = otxn_field(SBUF(buffer), sfAmount);
        int64_t amount_xfl = -INT64_FROM_BUF(buffer);
        int64_t amount_drops = float_int(amount_xfl, 6, 1);
        
            // Check if the outgoing payment is an IOU
            if (res != 48) 
                DONE("ROP Hook :: Accepted :: Outgoing non-IOU payment.");

            // Check if the outgoing payment matches the exact amount
            if (amount_drops != exact_drops) {
                if (amount_drops > exact_drops)
                    NOPE("ROP Hook :: Rejected :: Outgoing IOU payment is above the required amount.");
                else
                    NOPE("ROP Hook :: Rejected :: Outgoing IOU payment is below the required amount.");
            }
            
    DONE("ROP Hook :: Accepted :: Outgoing IOU payment matches exact amount.");


    //
    // 5. Reject outgoing IOU payments not a multiple of a fixed IOU amount
    //

    // Configure the cost in IOU
    uint64_t cost_IOU = 10;
    uint64_t multi_count = 0;
    uint64_t cost_drops = (cost_IOU * 1000000);

        uint8_t buffer[48];
        res =otxn_field(SBUF(buffer), sfAmount);
        int64_t amount_xfl = -INT64_FROM_BUF(buffer);
        int64_t amount_drops = float_int(amount_xfl, 6, 1);
        int64_t amount_int = float_int(amount_xfl, 0, 1);

            // Check the Outgoing amount is an IOU Token
            if (res != 48)
                NOPE("ROP Hook :: Rejected :: Outgoing XAH rejected.");

        TRACEVAR(amount_int);
        TRACEVAR(cost_IOU);

            // Check if the payment is equal to the cost
            if (amount_drops == cost_drops) {
                DONE("ROP Hook :: Accepted :: Payment matches the required amount.");
            }

            // Calculate if the payment is a multiple of the cost
            multi_count =  amount_drops / cost_drops;
            if (multi_count == 0) {
                NOPE("ROP Hook :: Rejected :: Payment is less than the required amount!");
            }
            if (amount_drops % cost_drops != 0) {
                NOPE("ROP Hook :: Rejected :: Payment amount must be a multiple of the required!");
            }

        TRACEVAR(multi_count);

    DONE("ROP Hook :: Accepted :: Outgoing XAH payment is a multiple of the required amount.");

    _g(1, 1);
    return 0;
}