/**
 * HandyHooks - 007 - Accept Max.
 * This hook allows outgoing transactions and blocks payments above the max_amount_value.
 * Install on ttPayment.
 * Set the max_amount_value to the desired amount.(14)
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

// Configure a maximum amount
uint64_t max_amount_value = 10; // 10 XAH

int64_t hook(uint32_t reserved)
{

    TRACESTR("Accept Maximum: Called.");

    // ACCOUNT: Hook Account
    uint8_t hook_acc[20];
    hook_account(hook_acc, 20);

    // ACCOUNT: Origin Tx Account
    uint8_t otxn_acc[20];
    otxn_field(otxn_acc, 20, sfAccount);

    // To know the type of origin txn
    int64_t tt = otxn_type();

    // If the transaction is outgoing from the hook account, accept it
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc) && tt == ttPAYMENT)
    {
        DONE("Accept Maximum: Outgoing payment transaction accepted");
    }

    // Convert the amount from drops to XAH
    unsigned char amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    double xah_amount = (double)otxn_drops / 1000000.0; // Convert to XAH
    TRACEVAR(xah_amount);

    // Ensure the payment is XAH
    if (amount_len != 8){
        NOPE("Accept Maximum: Error: Non-XAH payment rejected.");
    }

    // Check if the payment exceeds the maximum amount
    if (xah_amount > max_amount_value){
        NOPE("Accept Maximum: Error: Payment amount is greater than the maximum allowed.");
    }

    DONE("Accept Maximum: Incoming XAH payment accepted.");

    _g(1, 1);
    return 0;
}
