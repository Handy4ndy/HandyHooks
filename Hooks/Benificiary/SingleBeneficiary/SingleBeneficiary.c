//*****************************************************************
//  Single Beneficiary Contract – Xahau HandyHook Collection
//  Author: @Handy_4ndy
//
//  Overview:
//  Automatically distributes the entire hook account balance to a
//  single designated beneficiary if the account remains inactive for a configurable period.
//
//  Key Features:
//  • Threshold resets on ANY outgoing transaction from the hook account.
//  • After the inactivity threshold is exceeded, the DELEGATE may
//    invoke the hook allowing the BENEFICIARY to claim the full balance.
//
//  Hook Parameters (set at install):
//  • BENEFICIARY   (20 bytes) – Beneficiary account ID (r-address or X-address).
//  • DELEGATE      (20 bytes) – Delegate account ID (r-address or X-address).
//  • THRESHOLD     (4 bytes)  – Inactivity timeout in seconds (uint32).
//
//  Delegate Commands (ttINVOKE from DELEGATE only, post-threshold):
//  • SEND (1 byte) – Any value triggers immediate transfer of the
//                    full account balance to the beneficiary.
//  State Storage:
//  • LAST_CHECKIN (4 bytes) – Unix timestamp (uint32) of the most recent outgoing transaction.
//
//*****************************************************************

#include "hookapi.h"

#define DONE(x) accept(SBUF("SBC:: Success :: " x), __LINE__)
#define WARN(x) rollback(SBUF("SBC:: Warning :: " x), __LINE__)
#define NOPE(x) rollback(SBUF("SBC:: Error :: " x), __LINE__)
#define COMP(x) rollback(SBUF("SBC:: Complete :: " x), __LINE__)
uint8_t msg_buf[67] = "SBC:: Error :: You must wait 00000000 seconds before triggering.";

#define SET_TIME_MSG(remaining_seconds)                            \
    {                                                              \
        msg_buf[31] = '0' + ((remaining_seconds) / 10000000) % 10; \
        msg_buf[32] = '0' + ((remaining_seconds) / 1000000) % 10;  \
        msg_buf[33] = '0' + ((remaining_seconds) / 100000) % 10;   \
        msg_buf[34] = '0' + ((remaining_seconds) / 10000) % 10;    \
        msg_buf[35] = '0' + ((remaining_seconds) / 1000) % 10;     \
        msg_buf[36] = '0' + ((remaining_seconds) / 100) % 10;      \
        msg_buf[37] = '0' + ((remaining_seconds) / 10) % 10;       \
        msg_buf[38] = '0' + ((remaining_seconds)) % 10;            \
        rollback((uint32_t)msg_buf, sizeof(msg_buf), __LINE__);    \
    }

#define LAST_CHECKIN_KEY 0x4C41535443484543ULL // "LASTCHCK"

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

    // Load delegate account from Hook Parameters (set during installation)
    uint8_t delegate_acc[20];
    if (hook_param(SBUF(delegate_acc), "DELEGATE", 8) != 20)
        WARN("Delegate account not set during installation");

    // Load threshold from Hook Parameters
    uint8_t threshold_buf[4];
    uint32_t threshold = 2592000; // Default 30 days in seconds
    if (hook_param(SBUF(threshold_buf), "THRESHOLD", 9) == 4)
        threshold = UINT32_FROM_BUF(threshold_buf);

    int64_t tt = otxn_type();

    // Accept outgoing payments from hook account and reset timer
    if (tt == ttPAYMENT && BUFFER_EQUAL_20(hook_acc, otxn_acc))
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

    // Handle ttINVOKE
    if (tt == ttINVOKE)
    {
        // Admin send (only if threshold exceeded)
        if (BUFFER_EQUAL_20(delegate_acc, otxn_acc))
        {
            uint8_t send_param[1];
            if (otxn_param(SBUF(send_param), "SEND", 4) == 1)
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

                // Amount in XFL
                int64_t balance_xfl = slot_float(1); 

                if (balance_xfl <= 0)
                    NOPE("Insufficient balance to send");

                // Convert XFL balance to drops
                int64_t balance_drops = float_int(balance_xfl, 6, 1);

                // 5 XAH reserve for fees
                uint64_t reserve_drops = 5000000; 

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

                DONE("Balance sent to beneficiary successfully (threshold exceeded)");
            }
            NOPE("Invalid Delegate invoke");
        }
        NOPE("Unauthorized invoke");
    }

    // Pass through all other transaction types
    DONE("Transaction passed through");

    _g(1, 1); // Guard
    return 0;
}