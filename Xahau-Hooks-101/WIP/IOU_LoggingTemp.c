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
        TRACESTR("Hook account matched the originating account.");
    }

     if (!BUFFER_EQUAL_20(hook_acc, otxn_acc)){
        TRACESTR("Hook account did not match the originating account.");
    }

    //
    // 1. Accept IOU payments above a minimum amount
    //

    // Configure the minimum amount
    uint64_t min_amount = 10;
    uint64_t min_drops = (min_amount * 1000000);

        // Get the incoming payment amount
        uint8_t buffer[48];
        uint64_t res = otxn_field(SBUF(buffer), sfAmount);
        int64_t amount_xfl = -INT64_FROM_BUF(buffer);
        int64_t amount_drops = float_int(amount_xfl, 6, 1);

        TRACEHEX(buffer);
        TRACEVAR(amount_xfl);
        TRACEVAR(min_drops); 
        TRACEVAR(amount_drops); 

            uint64_t issuer_accid[20];
                // Copy from buffer + 28 (after 8-byte XFL + 20-byte currency)
                for (int i = 0; GUARD(50), i < 20; ++i) {
                    issuer_accid[i] = buffer[28 + i];
                }

                uint8_t issuer_raddr[35];

                int64_t len = util_raddr(SBUF(issuer_raddr), SBUF(issuer_accid));
                    if (len < 0) {
                        TRACESTR("Failed to convert account ID to r-address.");
                    }
           
                TRACEHEX(issuer_raddr);


            // Check the incoming amount is an IOU Token
            if (res != 48)
                NOPE("AIP Hook :: Rejected :: XAH Payment rejected.");

            // Check if the incoming amount is below the minimum amount
            if (amount_drops < min_drops)
                NOPE("AIP Hook :: Rejected :: IOU payment is below minimum amount.");

    DONE("AIP Hook :: Accepted :: Incoming amount above the minimum amount threshold.");
    
    
    // //
    // // 2. Accept a multiple of a fixed IOU amount
    // //

    // // Configure the cost in IOU
    // uint64_t cost_IOU = 10;
    // uint64_t multi_count = 0;
    // uint64_t cost_drops = (cost_IOU * 1000000);

    //     uint8_t buffer[48];
    //     uint64_t res = otxn_field(SBUF(buffer), sfAmount);
    //     int64_t amount_xfl = -INT64_FROM_BUF(buffer);
    //     int64_t amount_drops = float_int(amount_xfl, 6, 1);
    //     int64_t amount_int = float_int(amount_xfl, 0, 1);

    //         // Check the incoming amount is an IOU Token
    //         if (res != 48)
    //             NOPE("AIP Hook :: Rejected :: Incoming XAH rejected.");

    //     TRACEHEX(buffer);
    //     TRACEVAR(cost_drops);
    //     TRACEVAR(amount_drops);
    //     TRACEXFL(amount_xfl);
    //     TRACEVAR(cost_IOU);
    //     TRACEVAR(amount_int);


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



