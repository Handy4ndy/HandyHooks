#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

// Configure an exact amount to receive (in XAH)
uint64_t exact_amount_value = 10; // 10 XAH

// Configure the amount to emit (in drops)
uint64_t amountOut = 5000000; // 5 XAH

// Configure second Hook account to emit the payment to
uint8_t second_hook_acc[20] = {
    0x08U, 0x47U, 0x8BU, 0x33U, 0x41U, 0xBBU, 
    0x50U, 0xFCU, 0x05U, 0x73U, 0x68U, 0xF2U, 
    0x2AU, 0xF7U, 0x8CU, 0x59U, 0xF8U, 0x5EU, 
    0x41U, 0xC5U
};

// Namespace and key for state storage
uint8_t namespace[32] = {
    0x01U, 0xEAU, 0xF0U, 0x93U, 0x26U, 0xB4U, 0x91U, 0x15U,
    0x54U, 0x38U, 0x41U, 0x21U, 0xFFU, 0x56U, 0xFAU, 0x8FU,
    0xECU, 0xC2U, 0x15U, 0xFDU, 0xDEU, 0x2EU, 0xC3U, 0x5DU,
    0x9EU, 0x59U, 0xF2U, 0xC5U, 0x3EU, 0xC6U, 0x65U, 0xA0U
};
uint8_t state_key[32] = {
    0x45U, 0x56U, 0x52U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
};

int64_t hook(uint32_t reserved)
{
    TRACESTR("First Hook: Called.");

    // ACCOUNT: Hook Account
    uint8_t hook_acc[20];
    hook_account(hook_acc, 20);

    // ACCOUNT: Origin Tx Account
    uint8_t otxn_acc[20];
    otxn_field(otxn_acc, 20, sfAccount);

    // Check transaction type
    int64_t tt = otxn_type();
    if (tt != ttPAYMENT) {
        NOPE("First Hook: Only ttPAYMENT transactions are processed.");
    }

    // If the transaction is outgoing, accept it
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc)) {
        DONE("First Hook: Outgoing payment transaction accepted.");
    }

    // Convert the amount from drops to XAH
    unsigned char amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    double xah_amount = (double)otxn_drops / 1000000.0;
    TRACEVAR(xah_amount);

    // Ensure the payment is XAH
    if (amount_len != 8) {
        NOPE("First Hook: Non-XAH payment rejected.");
    }

    // Check if the payment matches the exact amount
    if (xah_amount != exact_amount_value) {
        NOPE("First Hook: Payment amount doesn't match exact_amount_value.");
    }

    // Store otxn_acc in Hook State with namespace
    int64_t state_result = state_foreign_set(SBUF(otxn_acc), SBUF(state_key), SBUF(namespace), SBUF(hook_acc));
    if (state_result < 0) {
        NOPE("First Hook: Failed to set state.");
    }

    // Reserve space for emitted transaction
    etxn_reserve(1);

    // Prepare the payment transaction to second Hook
    uint8_t txn[PREPARE_PAYMENT_SIMPLE_SIZE];
    PREPARE_PAYMENT_SIMPLE(txn, amountOut, second_hook_acc, 0, 0);

    // Emit the transaction
    uint8_t emithash[32];
    if (emit(SBUF(emithash), SBUF(txn)) != 32) {
        NOPE("First Hook: Failed to emit transaction.");
    }

    DONE("First Hook: Payment received, state set, and forwarded successfully.");
    _g(1, 1);
    return 0;
}