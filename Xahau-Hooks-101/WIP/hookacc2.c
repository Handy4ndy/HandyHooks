#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

// Configure an exact amount to receive (in XAH)
uint64_t exact_amount_value = 5; // 5 XAH

// Configure the amount to emit (in drops)
uint64_t amountOut = 5000000; // 5 XAH

// Namespace and key to access first Hook's state
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

// First Hook account (to access its state)
uint8_t first_hook_acc[20] = {
    0x80U, 0x30U, 0xADU, 0x83U, 0x7CU, 
    0xC9U, 0x4AU, 0x33U, 0x65U, 0xBFU, 
    0x20U, 0x4BU, 0x7BU, 0x5EU, 0xD1U, 
    0x45U, 0x7BU, 0x3FU, 0x16U, 0x2AU
};

int64_t hook(uint32_t reserved)
{
    TRACESTR("Second Hook: Called.");

    // ACCOUNT: Hook Account
    uint8_t hook_acc[20];
    hook_account(hook_acc, 20);

    // ACCOUNT: Origin Tx Account
    uint8_t otxn_acc[20];
    otxn_field(otxn_acc, 20, sfAccount);

    // Check transaction type
    int64_t tt = otxn_type();
    if (tt != ttPAYMENT) {
        NOPE("Second Hook: Only ttPAYMENT transactions are processed.");
    }

    // If the transaction is outgoing, accept it
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc)) {
        DONE("Second Hook: Outgoing payment transaction accepted.");
    }

    // Convert the amount from drops to XAH
    unsigned char amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    double xah_amount = (double)otxn_drops / 1000000.0;
    TRACEVAR(xah_amount);

    // Ensure the payment is XAH
    if (amount_len != 8) {
        NOPE("Second Hook: Non-XAH payment rejected.");
    }

    // Check if the payment matches the exact amount
    if (xah_amount != exact_amount_value) {
        NOPE("Second Hook: Payment amount doesn't match exact_amount_value.");
    }

    // Retrieve otxn_acc from first Hook's state
    uint8_t ftxn_acc[20];
    int64_t state_result = state_foreign(SBUF(ftxn_acc), SBUF(state_key), SBUF(namespace), SBUF(first_hook_acc));
    if (state_result != 20) {
        NOPE("Second Hook: Failed to retrieve foreign state.");
    }

    // Reserve space for emitted transaction
    etxn_reserve(1);

    // Prepare the payment transaction to ftxn_acc
    uint8_t txn[PREPARE_PAYMENT_SIMPLE_SIZE];
    PREPARE_PAYMENT_SIMPLE(txn, amountOut, ftxn_acc, 0, 0);

    // Emit the transaction
    uint8_t emithash[32];
    if (emit(SBUF(emithash), SBUF(txn)) != 32) {
        NOPE("Second Hook: Failed to emit transaction.");
    }

    DONE("Second Hook: Payment received and forwarded to original account successfully.");
    _g(1, 1);
    return 0;
}