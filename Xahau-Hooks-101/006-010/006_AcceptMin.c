/**
 * HandyHooks - 006 - Accept Minimum.
 * This hook allows outgoing transactions and blocks payments below the min_amount_value.
 * Install on ttPayment.
 * Set the min_amount_value to the desired amount.(50)
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

int64_t hook(uint32_t reserved)
{

    TRACESTR("Accept Minimum: Called.");

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
        DONE("Accept Minimum: Outgoing payment transaction accepted");
    }

    // fetch the sent Amount
    unsigned char amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    double xah_amount = (double)otxn_drops / 1000000.0; // Convert to XRP
    TRACEVAR(xah_amount);

    // Ensure the payment is XAH
    if (amount_len != 8)
    {
        NOPE("Accept Minimum: Error: Non-XAH payment rejected.");
    }

    // Configure a minimum amount
    uint64_t min_amount_value = 10; // 10 XAH

    // Check if the payment is less than the minimum amount
    if (xah_amount < min_amount_value)
    {
        NOPE("Accept Minimum: Error: Payment amount is less than the minimum required.");
    }

    DONE("Accept Minimum: Incoming XAH payment accepted.");

    _g(1, 1);
    return 0;
}
