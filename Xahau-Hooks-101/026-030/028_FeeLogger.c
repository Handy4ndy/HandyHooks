/**
 * HandyHooks - 028 Fee Logger Hook
 * Logs the sfFee field of outgoing ttPAYMENT transactions using otxn_field.
 * Install on ttPayment.
 * Minimal logic for efficiency and simplicity.
 * Correctly handles XAH Amount format for sfFee.
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

// Macro to convert XRP Amount buffer to drops
#define XRP_AMOUNT_TO_DROPS(buf) \
    ({ \
        uint64_t drops = 0; \
        if ((buf)[0] != 0x40) NOPE("Fee Logger Hook: Invalid XRP Amount format"); \
        drops = (((uint64_t)(buf)[1] << 48) + ((uint64_t)(buf)[2] << 40) + \
                 ((uint64_t)(buf)[3] << 32) + ((uint64_t)(buf)[4] << 24) + \
                 ((uint64_t)(buf)[5] << 16) + ((uint64_t)(buf)[6] << 8) + \
                 (uint64_t)(buf)[7]); \
        drops; \
    })

#define sfFee ((6U << 16U) + 8U)

int64_t hook(uint32_t reserved) {
    TRACESTR("Fee Logger Hook: Called.");

    // Get transaction type
    int64_t tt = otxn_type();
    TRACEVAR(tt);
    if (tt != ttPAYMENT)
        DONE("Fee Logger Hook: Non-payment transaction");

    // Get hook and origin accounts
    uint8_t hook_acc[20];
    hook_account(SBUF(hook_acc));
    uint8_t otxn_acc[20];
    int64_t acc_len = otxn_field(SBUF(otxn_acc), sfAccount);
    if (acc_len != 20)
        NOPE("Fee Logger Hook: Error: Failed to get origin account");

    // Check if outgoing (origin account == hook account)
    if (!BUFFER_EQUAL_20(hook_acc, otxn_acc))
        DONE("Fee Logger Hook: Incoming payment, skipping");

    // Get transaction fee
    uint8_t tx_fee[8];
    int64_t fee_len = otxn_field(SBUF(tx_fee), sfFee);
    TRACEVAR(fee_len);
    if (fee_len != 8)
        NOPE("Fee Logger Hook: Error: Failed to get transaction fee");


    // Convert fee buffer to drops
    int64_t fee_drops = XRP_AMOUNT_TO_DROPS(tx_fee);
    TRACEVAR(fee_drops);

    DONE("Fee Logger Hook: Outgoing payment fee logged");

    _g (1,1);
    return 0;
}