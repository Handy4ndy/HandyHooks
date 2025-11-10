//**************************************************************
// Single Beneficiary Threshold Contract (SBTC) - Xahau HandyHook Collection
// Author: @Handy_4ndy
//
// Description:
//   When the inactivity threshold has been exceeded, any incoming transaction (payment or invoke)
//   from anyone other than the hook owner triggers an automatic forward of the full XAH balance to the beneficiary.
//   Timer resets on outgoing transactions from the hook account.
//
// Hook Parameters (set during installation):
//   'BENEFICIARY' (20 bytes): Beneficiary account ID (recipient of forwarded balance).
//   'THRESHOLD' (4 bytes): Inactivity threshold in seconds (uint32) before auto-trigger.
//
//**************************************************************

#include "hookapi.h"

#define DONE(x) accept(SBUF("SBTC:: Success :: " x), __LINE__)
#define WARN(x) rollback(SBUF("SBTC:: Warning :: " x), __LINE__)
#define NOPE(x) rollback(SBUF("SBTC:: Error :: " x), __LINE__)
#define COMP(x) rollback(SBUF("SBTC:: Complete :: " x), __LINE__)
uint8_t msg_buf[68] = "SBTC:: Error :: You must wait 00000000  seconds before triggering.";

#define SET_TIME_MSG(remaining_seconds)                            \
    {                                                              \
        msg_buf[32] = '0' + ((remaining_seconds) / 10000000) % 10; \
        msg_buf[33] = '0' + ((remaining_seconds) / 1000000) % 10;  \
        msg_buf[34] = '0' + ((remaining_seconds) / 100000) % 10;   \
        msg_buf[35] = '0' + ((remaining_seconds) / 10000) % 10;    \
        msg_buf[36] = '0' + ((remaining_seconds) / 1000) % 10;     \
        msg_buf[37] = '0' + ((remaining_seconds) / 100) % 10;      \
        msg_buf[38] = '0' + ((remaining_seconds) / 10) % 10;       \
        msg_buf[39] = '0' + ((remaining_seconds)) % 10;            \
        rollback((uint32_t)msg_buf, sizeof(msg_buf), __LINE__);    \
    }

#define LAST_CHECKIN_KEY 0x4C41535443484543ULL

int64_t hook(uint32_t reserved)
{

    TRACESTR("SBC :: Single Beneficiary Contract :: Called");

    // Get the Hook account
    uint8_t hook_acc[20];
    hook_account(SBUF(hook_acc));

    // Get the Originating account of the transaction
    uint8_t otxn_acc[20];
    otxn_field(SBUF(otxn_acc), sfAccount);

    // Load admin account from Hook Parameters (set during installation)
    uint8_t beneficiary_acc[20];
    if (hook_param(SBUF(beneficiary_acc), "BENEFICIARY", 11) != 20)
        WARN("Beneficiary account not set during installation");

    // Load threshold from Hook Parameters
    uint8_t threshold_buf[4];
    uint32_t threshold = 2592000; // Default 30 days in seconds
    if (hook_param(SBUF(threshold_buf), "THRESHOLD", 9) == 4)
        threshold = UINT32_FROM_BUF(threshold_buf);

    int64_t tt = otxn_type();

    // Accept outgoing payments from hook account and reset timer
    if (tt == ttPAYMENT || (tt == ttINVOKE && BUFFER_EQUAL_20(hook_acc, otxn_acc)))
    {
        // Reset the timer on outgoing transaction
        uint32_t current_time = (uint32_t)ledger_last_time();
        uint8_t time_buf[4];
        UINT32_TO_BUF(time_buf, current_time);
        uint8_t key_buf[8];
        UINT64_TO_BUF(key_buf, LAST_CHECKIN_KEY);
        state_set(SBUF(time_buf), SBUF(key_buf));
        DONE("Outgoing payment from hook account accepted, timer reset");
    }

    // Trigger balance transfer on any incoming transaction (payment or invoke) from non-owner, if threshold exceeded
    if ((tt == ttPAYMENT || tt == ttINVOKE) && !BUFFER_EQUAL_20(hook_acc, otxn_acc))
    {
        // Load last outgoing time
        uint8_t key_buf[8];
        UINT64_TO_BUF(key_buf, LAST_CHECKIN_KEY);
        uint8_t last_checkin_buf[4];
        uint32_t last_checkin = 0;
        if (state(SBUF(last_checkin_buf), SBUF(key_buf)) == 4)
            last_checkin = UINT32_FROM_BUF(last_checkin_buf);

        // Get current time
        uint32_t current_time = (uint32_t)ledger_last_time();

        // Check if threshold exceeded
        uint32_t time_elapsed = current_time - last_checkin;
        if (time_elapsed < threshold)
        {
            uint32_t remaining = threshold - time_elapsed;
            SET_TIME_MSG(remaining);
        }

        // KEYLET: Account Root
        uint8_t acct_kl[34];
        util_keylet(SBUF(acct_kl), KEYLET_ACCOUNT, SBUF(hook_acc), 0, 0, 0, 0);

        // SLOT SET: Slot 1
        if (slot_set(SBUF(acct_kl), 1) != 1)
            NOPE("Could not load account keylet");

        // SLOT SUBFIELD: sfBalance
        if (slot_subfield(1, sfBalance, 1) != 1)
            NOPE("Could not load account keylet `sfBalance`");

        int64_t balance_xfl = slot_float(1); // amount in XFL

        if (balance_xfl <= 0)
            NOPE("Insufficient balance to send");

        // Convert XFL balance to drops
        int64_t balance_drops = float_int(balance_xfl, 6, 1); // 6 for drops, 1 for rounding

        uint64_t reserve_drops = 5000000; // 5 XAH reserve for fees

        if (balance_drops <= (int64_t)reserve_drops)
            COMP("This contract appears to have been completed, Farewell Adventurer <3 ");

        uint64_t send_amount = (uint64_t)(balance_drops - reserve_drops);

        // Reserve space for emitted transaction
        etxn_reserve(1);

        // Prepare payment transaction
        uint8_t txn[PREPARE_PAYMENT_SIMPLE_SIZE];
        PREPARE_PAYMENT_SIMPLE(txn, send_amount, beneficiary_acc, 0, 0);

        uint8_t emithash[32];

        // Emit the transaction
        if (emit(SBUF(emithash), SBUF(txn)) != 32)
            NOPE("Failed to emit balance transfer transaction");

        DONE("Full balance forwarded to beneficiary (threshold exceeded)");
    }

    // Pass through all other transaction types
    DONE("Transaction passed through");

    _g(1, 1); // Guard
    return 0;
}