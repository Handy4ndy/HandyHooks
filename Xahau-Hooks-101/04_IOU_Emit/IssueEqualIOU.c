/**
 * IssueEqualIOU.c
 *
 * This hook issues a custom IOU token (same issuer/currency as received) to the sender of an incoming IOU payment.
 * The amount issued is exactly equal to the IOU amount received (in drops).
 *
 * Only accepts incoming IOU payments (not XAH).
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)
#define GUARD(maxiter) _g(__LINE__, (maxiter) + 1)

int64_t hook(uint32_t reserved) {
    TRACESTR("IssueEqualIOU: Started.");

    // Only process ttPAYMENT
    if (otxn_type() != ttPAYMENT)
        DONE("IssueEqualIOU: Non-PAYMENT transaction accepted.");

    // Get hook account
    uint8_t hook_acc[20];
    if (hook_account(SBUF(hook_acc)) != 20)
        NOPE("IssueEqualIOU: Failed to get hook account.");

    // Get origin account
    uint8_t otxn_acc[20];
    if (otxn_field(SBUF(otxn_acc), sfAccount) != 20)
        NOPE("IssueEqualIOU: Failed to get origin account.");

    // Ignore outgoing transactions
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc))
        DONE("IssueEqualIOU: Outgoing transaction accepted.");

    // Only accept IOU payments (not XAH)
    uint8_t buffer[48];
    int64_t res = otxn_field(SBUF(buffer), sfAmount);
    if (res != 48)
        NOPE("IssueEqualIOU: Incoming payment is not IOU (must be 48 bytes).");

    // Extract IOU amount (XFL), currency, and issuer
    int64_t amount_xfl = -INT64_FROM_BUF(buffer);
    int64_t amount_drops = float_int(amount_xfl, 6, 1); // 6 = drops, 1 = round
    if (amount_drops <= 0)
        NOPE("IssueEqualIOU: Invalid or zero IOU amount.");
    TRACEVAR(amount_drops);
    TRACEXFL(amount_xfl);
    TRACEHEX(buffer);

    // Extract currency (12..31) and issuer (32..51)
    uint8_t currency[20];
    uint8_t issuer_accid[20];
    for (int i = 0; GUARD(20), i < 20; ++i) {
        currency[i] = buffer[8 + i];
        issuer_accid[i] = buffer[28 + i];
    }
    TRACEHEX(currency);
    TRACEHEX(issuer_accid);

    // Check trustline
    uint8_t keylet[34];
    if (util_keylet(SBUF(keylet), KEYLET_LINE, SBUF(hook_acc), SBUF(otxn_acc), SBUF(currency)) != 34)
        NOPE("IssueEqualIOU: Could not generate trustline keylet.");

    // Prepare issued amount in XFL (same as received)
    uint8_t amt_out[48];
    if (float_sto(amt_out - 1, 49, SBUF(currency), SBUF(hook_acc), amount_xfl, sfAmount) < 0)
        NOPE("IssueEqualIOU: Failed to serialize issued amount.");

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
        NOPE("IssueEqualIOU: Failed to emit transaction.");

    DONE("IssueEqualIOU: IOU issued for exact IOU received.");

    _g(1, 1);
    return 0;
}
