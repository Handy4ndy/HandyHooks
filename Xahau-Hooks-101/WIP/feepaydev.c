/**
 * HandyHooks - FeePayDev
 *
 * This hook accepts incoming XAH payments that are a multiple of a fixed amount (10 XAH).
 * For each accepted payment, it emits a payment to a developer address equal to the transaction fee (effectively doubling the fee paid by the sender).
 *
 * Install on ttPayment.
 *
 * To use: Set the developer address in the ftxn_acc array below.
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

// Macro to convert XRP Amount buffer to drops
#define XRP_AMOUNT_TO_DROPS(buf) \
    ({ \
        uint64_t drops = 0; \
        if ((buf)[0] != 0x40) NOPE("FeePayDev: Invalid XRP Amount format"); \
        drops = (((uint64_t)(buf)[1] << 48) + ((uint64_t)(buf)[2] << 40) + \
                 ((uint64_t)(buf)[3] << 32) + ((uint64_t)(buf)[4] << 24) + \
                 ((uint64_t)(buf)[5] << 16) + ((uint64_t)(buf)[6] << 8) + \
                 (uint64_t)(buf)[7]); \
        drops; \
    })

// Configure Account to emit the payment to (developer address)
uint8_t ftxn_acc[20] = { 0xB9U, 0x59U, 0xD6U, 0xACU, 0x9DU, 0x15U, 0x89U, 0x17U, 0xECU, 0x26U, 0x9EU, 0xACU, 0x05U, 0x70U, 0x19U, 0x07U, 0x4FU, 0x56U, 0xD6U, 0x39U };

// Constants
#define sfFee ((6U << 16U) + 8U) // Fee field code

int64_t hook(uint32_t reserved) {
    TRACESTR("FeePayDev :: Accept Incoming Payment :: Called.");

    // Get the Hook account
    uint8_t hook_acc[20];
    hook_account(SBUF(hook_acc));
    // Get the Originating account of the transaction
    uint8_t otxn_acc[20];
    otxn_field(SBUF(otxn_acc), sfAccount);

    // Accept all outgoing payments
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc)){
        DONE("FeePayDev :: Accepted :: Outgoing payment.");
    }

    // Configure the cost in XAH
    uint64_t cost_xah = 10;
    uint64_t multi_count = 0;
    uint64_t cost_drops = cost_xah * 1000000; // Convert XAH to drops

    // Convert the amount from drops to XAH
    unsigned char amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    double xah_amount = (double)otxn_drops / 1000000.0; // Convert to XAH

    // Ensure the payment is XAH
    if (amount_len != 8){
        NOPE("FeePayDev :: Rejected :: Non-XAH payment rejected.");
    }

    // // Check if the payment is equal to the cost
    // if (otxn_drops == cost_drops) {
    //     DONE("FeePayDev :: Accepted :: Payment matches the required amount.");
    // }

    // Calculate if the payment is a multiple of the cost
    multi_count = otxn_drops / cost_drops;
    if (multi_count == 0) {
        NOPE("FeePayDev :: Rejected :: Payment is less than the required amount!");
    }
    if (otxn_drops % cost_drops != 0) {
        NOPE("FeePayDev :: Rejected :: Payment amount must be a multiple of the required!");
    }

    // Get transaction fee
    uint8_t tx_fee[8];
    int64_t fee_len = otxn_field(SBUF(tx_fee), sfFee);
    if (fee_len != 8) {
        NOPE("FeePayDev: Error: Failed to get transaction fee");
    }
    int64_t fee_drops = XRP_AMOUNT_TO_DROPS(tx_fee);
    if (fee_drops <= 0)
        NOPE("FeePayDev: Error: Invalid fee amount");

    // Reserve space for emitted transaction
    etxn_reserve(1);

    uint8_t txn[PREPARE_PAYMENT_SIMPLE_SIZE];
    // Prepare the payment transaction to the dev for the fee amount
    PREPARE_PAYMENT_SIMPLE(txn, fee_drops, ftxn_acc, 0, 0);

    uint8_t emithash[32];
    // Emit the transaction and check if it was successful
    if (emit(SBUF(emithash), SBUF(txn)) != 32) {
        NOPE("FeePayDev: Error: Failed to emit dev payment");
    } else {
        DONE("FeePayDev: Hook initiated and Dev Payment (fee) forwarded Successfully");
    }

    _g(1,1);
    return 0;
}
