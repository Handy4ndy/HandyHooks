#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)
#define GUARD(maxiter) _g(__LINE__, (maxiter) + 1)

#define UINT64_FROM_BUF(buf) \
    (((uint64_t)(buf)[0] << 56) + ((uint64_t)(buf)[1] << 48) + \
     ((uint64_t)(buf)[2] << 40) + ((uint64_t)(buf)[3] << 32) + \
     ((uint64_t)(buf)[4] << 24) + ((uint64_t)(buf)[5] << 16) + \
     ((uint64_t)(buf)[6] << 8) + (uint64_t)(buf)[7])

// Default currency code: XPN
static uint8_t DEFAULT_CURRENCY[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'X', 'P', 'N', 0, 0, 0, 0, 0};

int64_t hook(uint32_t reserved) {
    TRACESTR("MultiplyEmitXPN: Started.");

    // Check transaction type
    if (otxn_type() != ttPAYMENT)
        DONE("MultiplyEmitXPN: Non-PAYMENT transaction accepted.");

    // Get hook account
    uint8_t hook_acc[20];
    if (hook_account(SBUF(hook_acc)) != 20)
        NOPE("MultiplyEmitXPN: Failed to get hook account.");

    // Get origin account
    uint8_t otxn_acc[20];
    if (otxn_field(SBUF(otxn_acc), sfAccount) != 20)
        NOPE("MultiplyEmitXPN: Failed to get origin account.");

    // Ignore outgoing transactions
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc))
        DONE("MultiplyEmitXPN: Outgoing transaction accepted.");

    // Verify incoming amount is XAH
    uint8_t amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    if (amount_len != 8)
        NOPE("MultiplyEmitXPN: Non-XAH payment rejected.");

    int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    if (otxn_drops <= 0)
        NOPE("MultiplyEmitXPN: Invalid or zero XAH amount.");
    TRACEVAR(otxn_drops);

    // Get multiplier parameter (M)
    uint8_t multiplier_buf[8];
    uint8_t multiplier_key[1] = {'M'};
    int64_t multiplier_len = otxn_param(SBUF(multiplier_buf), SBUF(multiplier_key));
    if (multiplier_len != 8)
        NOPE("MultiplyEmitXPN: Multiplier (M) parameter must be 8 bytes.");
    
    uint64_t multiplier = UINT64_FROM_BUF(multiplier_buf);
    if (multiplier <= 0)
        NOPE("MultiplyEmitXPN: Multiplier (M) must be positive.");
    TRACEVAR(multiplier);

    // Get currency code parameter (C), default to XPN
    uint8_t currency[20];
    uint8_t currency_key[1] = {'C'};
    int64_t currency_len = otxn_param(SBUF(currency), SBUF(currency_key));
    if (currency_len == DOESNT_EXIST)
        for (int i = 0; GUARD(20), i < 20; ++i)
            currency[i] = DEFAULT_CURRENCY[i];
    else if (currency_len != 20)
        NOPE("MultiplyEmitXPN: Currency (C) parameter must be 20 bytes.");
    TRACEHEX(currency);

    // Check trustline
    uint8_t keylet[34];
    if (util_keylet(SBUF(keylet), KEYLET_LINE, SBUF(hook_acc), SBUF(otxn_acc), SBUF(currency)) != 34)
        NOPE("MultiplyEmitXPN: Could not generate trustline keylet.");

    // Calculate issued amount (XAH * multiplier)
    uint64_t issued_amount = (otxn_drops / 1000000) * multiplier; // Convert drops to XAH, then multiply
    if (issued_amount == 0)
        NOPE("MultiplyEmitXPN: Issued amount is zero.");
    TRACEVAR(issued_amount);

    // Prepare issued amount in XFL
    int64_t amount_xfl = float_set(0, issued_amount);
    uint8_t amt_out[48];
    if (float_sto(amt_out - 1, 49, SBUF(currency), SBUF(hook_acc), amount_xfl, sfAmount) < 0)
        NOPE("MultiplyEmitXPN: Failed to serialize issued amount.");

    // Set currency and issuer
    for (int i = 0; GUARD(20), i < 20; ++i) {
        amt_out[i + 8] = currency[i];
        amt_out[i + 28] = hook_acc[i];
    }

    // Reserve and prepare payment
    etxn_reserve(1);
    uint8_t txn[PREPARE_PAYMENT_SIMPLE_TRUSTLINE_SIZE];
    PREPARE_PAYMENT_SIMPLE_TRUSTLINE(txn, amt_out, otxn_acc, 0, 0);

    // Emit transaction
    uint8_t emithash[32];
    if (emit(SBUF(emithash), SBUF(txn)) != 32)
        NOPE("MultiplyEmitXPN: Failed to emit transaction.");

    DONE("MultiplyEmitXPN: XPN issued successfully.");

    _g(1, 1);
    return 0;
}