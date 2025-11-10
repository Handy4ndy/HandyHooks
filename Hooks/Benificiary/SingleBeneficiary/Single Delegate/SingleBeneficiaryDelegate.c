//**************************************************************
// Single Beneficiary Delegate Contract (SBDC) - Xahau HandyHook Collection
// Author: @Handy_4ndy
//
// Description:
//   Delegate invokes to allow the beneficiary to receive the full XAH balance from this account.
//   All other transaction types pass through normally.
//
// Hook Parameters (set during installation):
//   'BENEFICIARY' (20 bytes): Beneficiary account ID.
//   'DELEGATE' (20 bytes): Delegate account ID.
//
// Delegate Controls (via ttINVOKE):
//   'SEND' (1 byte): Any value to send full balance to beneficiary.
//
//**************************************************************

#include "hookapi.h"

#define DONE(x) accept(SBUF("SBDC:: Success :: " x), __LINE__)
#define WARN(x) rollback(SBUF("SBDC:: Warning :: " x), __LINE__)
#define NOPE(x) rollback(SBUF("SBDC:: Error :: " x), __LINE__)
#define COMP(x) rollback(SBUF("SBDC:: Complete :: " x), __LINE__)

int64_t hook(uint32_t reserved) {

    TRACESTR("SBDC :: Single Beneficiary Delegate Contract :: Called");
    // Get the Hook account
    uint8_t hook_acc[20];
    hook_account(SBUF(hook_acc));

    // Get the Originating account of the transaction
    uint8_t otxn_acc[20];
    otxn_field(SBUF(otxn_acc), sfAccount);

    // Load beneficiary account from Hook Parameters (set during installation)
    uint8_t beneficiary_acc[20];
    if (hook_param(SBUF(beneficiary_acc), "BENEFICIARY", 11) != 20)
        WARN("Beneficiary account not set during installation");

    // Load delegate account from Hook Parameters (set during installation)
    uint8_t delegate_acc[20];
    if (hook_param(SBUF(delegate_acc), "DELEGATE", 8) != 20)
        WARN("Delegate account not set during installation");

    int64_t tt = otxn_type();

    // Accept outgoing payments from hook account
    if (tt == ttPAYMENT && BUFFER_EQUAL_20(hook_acc, otxn_acc))
        DONE("Outgoing payment from hook account accepted");

    // Only process INVOKE transactions from the admin account
    if (tt != ttINVOKE || !BUFFER_EQUAL_20(delegate_acc, otxn_acc))
        DONE("Transaction not relevant - passing through");

    // Handle ttINVOKE for delegate control
    // Check for SEND parameter - send full balance to beneficiary
    uint8_t send_param[1];
    if (otxn_param(SBUF(send_param), "SEND", 4) == 1)
    {
        // KEYLET: Account Root
        uint8_t acct_kl[34];
        util_keylet(SBUF(acct_kl), KEYLET_ACCOUNT, SBUF(hook_acc), 0,0,0,0);

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

        DONE("Full balance sent to admin successfully");
    }

    // If no recognized parameters, pass through
    DONE("No recognized parameters in INVOKE");

    _g(1,1); // Guard
    return 0;
}