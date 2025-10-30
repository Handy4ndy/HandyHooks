/**
 * IssueMultipleInstall.c
 *
 * This hook issues a custom token (default: XPN) to the sender of an incoming XAH payment.
 * The multiplier (M) and currency code (C, optional) are set at installation time as hook parameters.
 *
 * Params:
 *   M: 8-byte multiplier (required, big-endian uint64)
 *   C: 20-byte currency code (optional, defaults to XPN)
 *
 * Example install params:
 *   "M": "000000000000000A" (10x multiplier)
 *   "C": "00000000000000000000000058504E000000000000000000000000" (XPN)
 */

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
    TRACESTR("IssueMultipleInstall: Started.");

    // Only process ttPAYMENT
    if (otxn_type() != ttPAYMENT)
        DONE("IssueMultipleInstall: Non-PAYMENT transaction accepted.");

    // Get hook account
    uint8_t hook_acc[20];
    if (hook_account(SBUF(hook_acc)) != 20)
        NOPE("IssueMultipleInstall: Failed to get hook account.");

    // Get origin account
    uint8_t otxn_acc[20];
    if (otxn_field(SBUF(otxn_acc), sfAccount) != 20)
        NOPE("IssueMultipleInstall: Failed to get origin account.");

    // Ignore outgoing transactions
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc))
        DONE("IssueMultipleInstall: Outgoing transaction accepted.");

    // Only accept XAH payments
    uint8_t amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    if (amount_len != 8)
        NOPE("IssueMultipleInstall: Non-XAH payment rejected.");

    int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    if (otxn_drops <= 0)
        NOPE("IssueMultipleInstall: Invalid or zero XAH amount.");
    TRACEVAR(otxn_drops);

    // Get multiplier parameter (M) from install params
    uint8_t multiplier_buf[8];
    if (hook_param(SBUF(multiplier_buf), "M", 1) != 8)
        NOPE("IssueMultipleInstall: Multiplier (M) parameter must be 8 bytes.");
    uint64_t multiplier = UINT64_FROM_BUF(multiplier_buf);
    if (multiplier == 0)
        NOPE("IssueMultipleInstall: Multiplier (M) must be positive.");
    TRACEVAR(multiplier);

    // Get currency code parameter (C) from install params, default to XPN
    uint8_t currency[20];
    int64_t currency_len = hook_param(SBUF(currency), "C", 1);
    if (currency_len == DOESNT_EXIST)
        for (int i = 0; GUARD(20), i < 20; ++i)
            currency[i] = DEFAULT_CURRENCY[i];
    else if (currency_len != 20)
        NOPE("IssueMultipleInstall: Currency (C) parameter must be 20 bytes.");
    TRACEHEX(currency);

    // Check trustline
    uint8_t keylet[34];
    if (util_keylet(SBUF(keylet), KEYLET_LINE, SBUF(hook_acc), SBUF(otxn_acc), SBUF(currency)) != 34)
        NOPE("IssueMultipleInstall: Could not generate trustline keylet.");

    // Calculate issued amount (XAH * multiplier)
    uint64_t issued_amount = (otxn_drops / 1000000) * multiplier;
    if (issued_amount == 0)
        NOPE("IssueMultipleInstall: Issued amount is zero.");
    TRACEVAR(issued_amount);

    // Prepare issued amount in XFL
    int64_t amount_xfl = float_set(0, issued_amount);
    uint8_t amt_out[48];
    if (float_sto(amt_out - 1, 49, SBUF(currency), SBUF(hook_acc), amount_xfl, sfAmount) < 0)
        NOPE("IssueMultipleInstall: Failed to serialize issued amount.");

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
        NOPE("IssueMultipleInstall: Failed to emit transaction.");

    DONE("IssueMultipleInstall: XPN issued successfully.");

    _g(1, 1);
    return 0;
}
