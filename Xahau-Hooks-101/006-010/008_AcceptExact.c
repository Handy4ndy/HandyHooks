/**
 * HandyHooks - 008 - Accept Exact.
 * This hook allows outgoing transactions and blocks payments that dont match the exact_amount_value.
 * Install on ttPayment.
 * Set the exact_amount_value to the desired amount.(49)
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

int64_t hook(uint32_t reserved)
{

    TRACESTR("Accept Exact: Called.");

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
        DONE("Accept Exact: Outgoing payment transaction accepted");
    }

    // fetch the sent Amount
    unsigned char amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    double xah_amount = (double)otxn_drops / 1000000.0; // Convert to XAH
    TRACEVAR(xah_amount);

    // Ensure the payment is XAH
    if (amount_len != 8){
        NOPE("Accept Exact: Error: Non-XAH payment rejected.");
    }

    // Configure a minimum amount
    uint64_t exact_amount_value = 10; // 10 XAH

    // Check if the payment is less than the minimum amount
    if (xah_amount != exact_amount_value){
        NOPE("Accept Exact: Error: Payment amount doesn't match the exact_amount_value.");
    }

    DONE("Accept Exact: Incoming XAH payment accepted.");

    _g(1, 1);
    return 0;
}
