/**
 * HandyHooks - 009 - Accept Multiple:
 * This hook allows outgoing transactions and blocks payments that are not a multiple of the cost_xah.
 * Install on ttPayment.
 * Set the cost_xah to the desired amount.(50)
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)
uint64_t multi_count = 0;

int64_t hook(uint32_t reserved)
{

    TRACESTR("Accept Multiple: Called.");

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
        DONE("Accept Multiple: Outgoing payment transaction accepted");
    }

    // fetch the sent Amount
    unsigned char amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    double xah_amount = (double)otxn_drops / 1000000.0; // Convert to XAH

    TRACEVAR(xah_amount);

    // Ensure the payment is XAH
    if (amount_len != 8){
        NOPE("Accept Multiple: Error: Non-XAH payment rejected.");
    }

    // Reconstruct COST value (in XAH, convert to drops)
    uint64_t cost_xah = 10;
    uint64_t cost_drops = cost_xah * 1000000; // Convert XAH to drops
    
    TRACEVAR(cost_drops);

    // Calculate number of URIs to mint
    multi_count = otxn_drops / cost_drops;
    if (multi_count == 0) {
        NOPE("Accept Multiple: Error: Payment amount is less than COST!");
    }
    if (otxn_drops % cost_drops != 0) {
        NOPE("Accept Multiple: Error: Payment amount must be an exact multiple of COST!");
    }

    TRACEVAR(multi_count);

    DONE("Accept Multiple: Incoming XAH payment accepted.");

    _g(1, 1);
    return 0;
}
