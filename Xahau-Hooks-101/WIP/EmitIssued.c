#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

#define EXACT_XAH_AMOUNT 10000000 // 10 XAH in drops
#define ISSUED_AMOUNT 100         // 100 BAR tokens
#define CURRENCY_CODE "BAR\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // 20-byte currency code

// Hardcoded destination account (replace with actual r-address bytes)
uint8_t DEST_ACC[20] = {0x08, 0x47, 0x8B, 0x33, 0x41, 0xBB, 0x50, 0xFC, 0x05, 0x73, 
                        0x68, 0xF2, 0x2A, 0xF7, 0x8C, 0x59, 0xF8, 0x5E, 0x41, 0xC5};

int64_t hook(uint32_t reserved) {
    // Check transaction type
    if (otxn_type() != ttPAYMENT)
        DONE("HardEmitAsset: Non-PAYMENT transaction accepted.");

    // Get hook account
    uint8_t hook_acc[20];
    hook_account(SBUF(hook_acc));

    // Get origin account
    uint8_t otxn_acc[20];
    otxn_field(SBUF(otxn_acc), sfAccount);

    // Ignore outgoing transactions
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc))
        DONE("HardEmitAsset: Outgoing transaction accepted.");

    // Verify incoming amount is XAH and exactly 10 XAH
    uint8_t amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    if (amount_len != 8)
        NOPE("HardEmitAsset: Non-XAH payment rejected.");
    
    int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    if (otxn_drops != EXACT_XAH_AMOUNT)
        NOPE("HardEmitAsset: Payment must be exactly 10 XAH.");

    // Prepare issued asset amount (100 BAR)
    int64_t amount_xfl = float_set(0, ISSUED_AMOUNT);
    uint8_t amt_out[48];
    if (float_sto(amt_out - 1, 49, SBUF(CURRENCY_CODE), SBUF(hook_acc), amount_xfl, sfAmount) < 0)
        NOPE("HardEmitAsset: Failed to serialize amount.");

    // Set issuer and currency in amount
    for (int i = 0; GUARD(20), i < 20; ++i) {
        amt_out[i + 8] = CURRENCY_CODE[i];
        amt_out[i + 28] = hook_acc[i];
    }

    // Reserve and prepare payment
    etxn_reserve(1);
    uint8_t txn[PREPARE_PAYMENT_SIMPLE_TRUSTLINE_SIZE];
    PREPARE_PAYMENT_SIMPLE_TRUSTLINE(txn, amt_out, DEST_ACC, 0, 0);

    // Emit transaction
    uint8_t emithash[32];
    if (emit(SBUF(emithash), SBUF(txn)) != 32)
        NOPE("HardEmitAsset: Failed to emit transaction.");

    DONE("HardEmitAsset: Issued 100 BAR successfully.");

    _g(1, 1);
    return 0;
}