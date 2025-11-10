//**************************************************************
// Native Issue Contract (NIC) – HandyHooks
// Author: @Handy_4ndy
//
// Description:
//   This hook issues a custom token (default: XPN) to the sender of an incoming XAH payment.
//   The multiplier (M) and currency code (C, optional) are set at installation time as hook parameters.
//   Issues tokens based on XAH received, with 10% going to a treasury account.
//
// Hook Parameters (set during installation):
//   'T_ACC' (20 bytes): Treasury account ID (required)
//   'M' (8 bytes): Multiplier (big-endian uint64, required)
//   'C' (20 bytes): Currency code (optional, defaults to XPN)
//
// Usage:
//   - Sender pays XAH to the hook account.
//   - Hook calculates issued amount: (XAH in millions) * multiplier.
//   - Issues tokens to sender, and 10% to treasury.
//   - Only processes incoming XAH payments.
//
// Example install params:
//   "T_ACC": "AABBCCDDEEFF00112233445566778899AABBCCDD" (Treasury Account)
//   "M": "000000000000000A" (10x multiplier)
//   "C": "00000000000000000000000058504E000000000000000000000000" (XPN)
//
//**************************************************************


#include "hookapi.h"

#define DONE(x) accept(SBUF("NIC:: Success :: " x), __LINE__)
#define NOPE(x) rollback(SBUF("NIC:: Error :: " x), __LINE__)
#define GUARD(maxiter) _g(__LINE__, (maxiter) + 1)


#define UINT64_FROM_BUF(buf) \
    (((uint64_t)(buf)[0] << 56) + ((uint64_t)(buf)[1] << 48) + \
     ((uint64_t)(buf)[2] << 40) + ((uint64_t)(buf)[3] << 32) + \
     ((uint64_t)(buf)[4] << 24) + ((uint64_t)(buf)[5] << 16) + \
     ((uint64_t)(buf)[6] << 8) + (uint64_t)(buf)[7])

// Macro to convert XRP Amount buffer to drops (for fee)
#define XRP_AMOUNT_TO_DROPS(buf) \
    ({ \
        uint64_t drops = 0; \
        if ((buf)[0] != 0x40) NOPE("NIC:: Error :: Invalid XRP Amount format"); \
        drops = (((uint64_t)(buf)[1] << 48) + ((uint64_t)(buf)[2] << 40) + \
                 ((uint64_t)(buf)[3] << 32) + ((uint64_t)(buf)[4] << 24) + \
                 ((uint64_t)(buf)[5] << 16) + ((uint64_t)(buf)[6] << 8) + \
                 (uint64_t)(buf)[7]); \
        drops; \
    })

// Constants
#define sfFee ((6U << 16U) + 8U) // Fee field code

// Default currency code: XPN
static uint8_t DEFAULT_CURRENCY[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'X', 'P', 'N', 0, 0, 0, 0, 0};

int64_t hook(uint32_t reserved) {
    TRACESTR("NIC :: Native Issue:: Called.");

    // Only process ttPAYMENT
    if (otxn_type() != ttPAYMENT)
        DONE("Non-PAYMENT transaction passed through.");

    // Get hook account
    uint8_t hook_acc[20];
    if (hook_account(SBUF(hook_acc)) != 20)
        NOPE("Failed to get hook account.");

    // Get origin account
    uint8_t otxn_acc[20];
    if (otxn_field(SBUF(otxn_acc), sfAccount) != 20)
        NOPE("Failed to get origin account.");

    // Ignore outgoing transactions
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc))
        DONE("Outgoing transaction passed through.");
    // Only accept XAH payments
    uint8_t amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    if (amount_len != 8)
        NOPE("Non-XAH payment rejected.");

    int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    if (otxn_drops <= 0)
        NOPE("Invalid or zero XAH amount.");
    TRACEVAR(otxn_drops);

    // Get treasury account parameter
    uint8_t treasury_acc[20];
    if (hook_param(SBUF(treasury_acc), "T_ACC", 5) != 20)
        NOPE("Treasury account not set as Hook Parameter.");

    // Get multiplier parameter (M) from install params
    uint8_t multiplier_buf[8];
    if (hook_param(SBUF(multiplier_buf), "M", 1) != 8)
        NOPE("Multiplier (M) parameter must be 8 bytes.");
    uint64_t multiplier = UINT64_FROM_BUF(multiplier_buf);
    if (multiplier == 0)
        NOPE("Multiplier (M) must be positive.");
    TRACEVAR(multiplier);

    // Get currency code parameter (C) from install params, default to XPN
    uint8_t currency[20];
    int64_t currency_len = hook_param(SBUF(currency), "C", 1);
    if (currency_len == DOESNT_EXIST)
        for (int i = 0; GUARD(20), i < 20; ++i)
            currency[i] = DEFAULT_CURRENCY[i];
    else if (currency_len != 20)
        NOPE("Currency (C) parameter must be 20 bytes.");
    TRACEHEX(currency);

    // Check trustline
    uint8_t keylet[34];
    if (util_keylet(SBUF(keylet), KEYLET_LINE, SBUF(hook_acc), SBUF(otxn_acc), SBUF(currency)) != 34)
        NOPE("Could not generate trustline keylet.");

    // Calculate issued amount (XAH * multiplier)
    uint64_t issued_amount = (otxn_drops / 1000000) * multiplier;
    if (issued_amount == 0)
        NOPE("Issued amount is zero.");
    TRACEVAR(issued_amount);

    // Calculate treasury amount (10% of issued amount)
    uint64_t treasury_amount = issued_amount / 10;
    if (treasury_amount == 0)
        treasury_amount = 1; // Minimum 1 token for treasury

    // Prepare main issued amount in XFL
    int64_t amount_xfl = float_set(0, issued_amount);
    uint8_t amt_out[48];
    if (float_sto(amt_out - 1, 49, SBUF(currency), SBUF(hook_acc), amount_xfl, sfAmount) < 0)
        NOPE("Failed to serialize issued amount.");

    // Set currency and issuer
    for (int i = 0; GUARD(20), i < 20; ++i) {
        amt_out[i + 8] = currency[i];
        amt_out[i + 28] = hook_acc[i];
    }

    // Prepare treasury amount in XFL
    int64_t treasury_amount_xfl = float_set(0, treasury_amount);
    uint8_t treasury_amt_out[48];
    if (float_sto(treasury_amt_out - 1, 49, SBUF(currency), SBUF(hook_acc), treasury_amount_xfl, sfAmount) < 0)
        NOPE("Failed to serialize treasury amount.");

    // Set currency and issuer for treasury
    for (int i = 0; GUARD(20), i < 20; ++i) {
        treasury_amt_out[i + 8] = currency[i];
        treasury_amt_out[i + 28] = hook_acc[i];
    }

    // Reserve and prepare both payments (main token issue + treasury)
    etxn_reserve(2);

    // Prepare main issued token payment to sender
    uint8_t txn[PREPARE_PAYMENT_SIMPLE_TRUSTLINE_SIZE];
    PREPARE_PAYMENT_SIMPLE_TRUSTLINE(txn, amt_out, otxn_acc, 0, 0);

    // Emit main token transaction
    uint8_t emithash[32];
    if (emit(SBUF(emithash), SBUF(txn)) != 32)
        NOPE("Failed to emit main token transaction.");

    // Prepare treasury token payment
    uint8_t treasury_txn[PREPARE_PAYMENT_SIMPLE_TRUSTLINE_SIZE];
    PREPARE_PAYMENT_SIMPLE_TRUSTLINE(treasury_txn, treasury_amt_out, treasury_acc, 0, 0);

    // Emit treasury transaction
    uint8_t treasury_emithash[32];
    if (emit(SBUF(treasury_emithash), SBUF(treasury_txn)) != 32)
        NOPE("Failed to emit treasury transaction.");

    DONE("Tokens issued to sender + 10% to treasury successfully.");

    _g(1, 1);
    return 0;
}
