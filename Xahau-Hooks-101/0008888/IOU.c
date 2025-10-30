// //
    // // Accept incoming IOU payment above a minimum amount
    // //
    // uint64_t min_amount = 1; // 1 IOU
    // uint8_t amount_buffer[48];

    //     // Check the incoming amount is an IOU
    //     int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    //         if (amount_len == 8)
    //             NOPE("Accept Payment: Incoming XAH payment rejected");

    //         // Convert amount to drops to compare
    //         int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    //         double xah_amount = (double)otxn_drops / 1000000.0;
    //         if (xah_amount < min_amount)
    //             NOPE("Accept Payment: Incoming payment is below minimum amount rejected.");

    // DONE("Accept Payment: Incoming amount above the minimum amount threshold accepted.");

    // //
    // // Accept incoming IOU payment below a maximum amount
    // //
    // uint64_t max_amount = 100; // 100 IOU
    // uint8_t amount_buffer[48];

    //     // Check the incoming amount is XAH
    //     int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    //         if (amount_len == 8)
    //             NOPE("Accept Payment: Incoming XAH payment rejected");

    //         // Convert amount to drops to compare
    //         int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    //         double xah_amount = (double)otxn_drops / 1000000.0;
    //         if (xah_amount > max_amount)
    //             NOPE("Accept Payment: Incoming Payment above maximum amount, rejected.");

    // DONE("Accept Payment: Incoming amount below the maximum amount threshold accepted.");