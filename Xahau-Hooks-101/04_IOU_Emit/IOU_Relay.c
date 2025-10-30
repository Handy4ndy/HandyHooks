/**
 * IOU_Relay.c
 *
 * This hook accepts a specific IOU (currency+issuer) and forwards the same incoming amount to a designated account set at install time.
 *
 * Install params:
 *   F: 20-byte AccountID to forward the IOU to (required)
 *   C: 20-byte currency code (required)
 *   I: 20-byte issuer AccountID (required)
 *
 * Only accepts incoming IOU payments matching the specified currency+issuer.
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)
#define GUARD(maxiter) _g(__LINE__, (maxiter) + 1)

int64_t hook(uint32_t reserved) {
    TRACESTR("IOU_Relay: Started.");

    // Only process ttPAYMENT
    if (otxn_type() != ttPAYMENT)
        DONE("IOU_Relay: Non-PAYMENT transaction accepted.");

    // Get hook account
    uint8_t hook_acc[20];
    if (hook_account(SBUF(hook_acc)) != 20)
        NOPE("IOU_Relay: Failed to get hook account.");

    // Get origin account
    uint8_t otxn_acc[20];
    if (otxn_field(SBUF(otxn_acc), sfAccount) != 20)
        NOPE("IOU_Relay: Failed to get origin account.");

    // Ignore outgoing transactions
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc))
        DONE("IOU_Relay: Outgoing transaction accepted.");

    // Get forward account from install param
    uint8_t fwd_acc[20];
    if (hook_param(SBUF(fwd_acc), "F", 1) != 20)
        NOPE("IOU_Relay: Forward account param (F) must be 20 bytes.");

    // Get required currency and issuer from install params
    uint8_t req_currency[20];
    if (hook_param(SBUF(req_currency), "C", 1) != 20)
        NOPE("IOU_Relay: Currency param (C) must be 20 bytes.");
    uint8_t req_issuer[20];
    if (hook_param(SBUF(req_issuer), "I", 1) != 20)
        NOPE("IOU_Relay: Issuer param (I) must be 20 bytes.");

    // Only accept IOU payments (not XAH)
    uint8_t buffer[48];
    int64_t res = otxn_field(SBUF(buffer), sfAmount);
    if (res != 48)
        DONE("IOU_Relay: Incoming payment is not IOU (must be 48 bytes), skipping..");

    // Extract IOU amount (XFL), currency, and issuer
    int64_t amount_xfl = -INT64_FROM_BUF(buffer);
    int64_t amount_drops = float_int(amount_xfl, 6, 1); // 6 = drops, 1 = round
    if (amount_drops <= 0)
        NOPE("IOU_Relay: Invalid or zero IOU amount.");
    TRACEVAR(amount_drops);
    TRACEXFL(amount_xfl);
    TRACEHEX(buffer);

    // Extract currency (8..27) and issuer (28..47)
    uint8_t currency[20];
    uint8_t issuer_accid[20];
    for (int i = 0; GUARD(20), i < 20; ++i) {
        currency[i] = buffer[8 + i];
        issuer_accid[i] = buffer[28 + i];
    }
    TRACEHEX(currency);
    TRACEHEX(issuer_accid);

    // Check if currency and issuer match the required ones
    for (int i = 0; GUARD(20), i < 20; ++i) {
        if (currency[i] != req_currency[i])
            NOPE("IOU_Relay: Incoming currency does not match required.");
        if (issuer_accid[i] != req_issuer[i])
            NOPE("IOU_Relay: Incoming issuer does not match required.");
    }

    // Prepare issued amount in XFL (same as received)
    uint8_t amt_out[48];
    if (float_sto(amt_out - 1, 49, SBUF(currency), SBUF(issuer_accid), amount_xfl, sfAmount) < 0)
        NOPE("IOU_Relay: Failed to serialize issued amount.");

    // Set currency and issuer
    for (int i = 0; GUARD(20), i < 20; ++i) {
        amt_out[i + 8] = currency[i];
        amt_out[i + 28] = issuer_accid[i];
    }

    // Reserve and prepare payment
    etxn_reserve(1);
    uint8_t txn[PREPARE_PAYMENT_SIMPLE_TRUSTLINE_SIZE];
    PREPARE_PAYMENT_SIMPLE_TRUSTLINE(txn, amt_out, fwd_acc, 0, 0);

    // Emit transaction
    uint8_t emithash[32];
    if (emit(SBUF(emithash), SBUF(txn)) != 32)
        NOPE("IOU_Relay: Failed to emit transaction.");

    DONE("IOU_Relay: IOU forwarded successfully.");

    _g(1, 1);
    return 0;
}
