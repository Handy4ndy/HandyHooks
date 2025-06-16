/**
 * HandyHooks - Fee Reflect Hook
 * For incoming ttPAYMENT transactions, emits a payment back to the origin account
 * with the amount equal to the transaction's sfFee in drops.
 * Uses otxn_field(sfFee) and XRP_AMOUNT_TO_DROPS for accurate fee calculation.
 * Install on ttPayment.
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

// Macro to convert XRP Amount buffer to drops
#define XRP_AMOUNT_TO_DROPS(buf) \
    ({ \
        uint64_t drops = 0; \
        if ((buf)[0] != 0x40) NOPE("Fee Reflect Hook: Invalid XRP Amount format"); \
        drops = (((uint64_t)(buf)[1] << 48) + ((uint64_t)(buf)[2] << 40) + \
                 ((uint64_t)(buf)[3] << 32) + ((uint64_t)(buf)[4] << 24) + \
                 ((uint64_t)(buf)[5] << 16) + ((uint64_t)(buf)[6] << 8) + \
                 (uint64_t)(buf)[7]); \
        drops; \
    })

// Constants
#define sfFee ((6U << 16U) + 8U) // Fee field code

int64_t hook(uint32_t reserved) {
    TRACESTR("Fee Reflect Hook: Called.");

    // Get transaction type
    int64_t tt = otxn_type();
    TRACEVAR(tt);
    if (tt != ttPAYMENT)
        DONE("Fee Reflect Hook: Non-payment transaction");

    // Get hook and origin accounts
    uint8_t hook_acc[20];
    hook_account(SBUF(hook_acc));
    uint8_t otxn_acc[20];
    int64_t acc_len = otxn_field(SBUF(otxn_acc), sfAccount);
    if (acc_len != 20)
        NOPE("Fee Reflect Hook: Error: Failed to get origin account");

    // Accept outgoing payments
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc))
        DONE("Fee Reflect Hook: Outgoing payment, skipping");

    // Get transaction fee
    uint8_t tx_fee[8];
    int64_t fee_len = otxn_field(SBUF(tx_fee), sfFee);
    TRACEVAR(fee_len);
    if (fee_len != 8) {
        NOPE("Fee Reflect Hook: Error: Failed to get transaction fee");
    }

    // Convert fee to drops
    int64_t fee_drops = XRP_AMOUNT_TO_DROPS(tx_fee);
    TRACEVAR(fee_drops);
    if (fee_drops <= 0)
        NOPE("Fee Reflect Hook: Error: Invalid fee amount");

    // Reserve space for emitted transaction
    etxn_reserve(1);

    // Prepare payment back to otxn_acc with fee_drops
    uint8_t txn[PREPARE_PAYMENT_SIMPLE_SIZE];
    PREPARE_PAYMENT_SIMPLE(txn, fee_drops, otxn_acc, 0, 0);

    // Emit the transaction
    uint8_t emithash[32];
    int64_t emit_result = emit(SBUF(emithash), SBUF(txn));
    TRACEVAR(emit_result);
    if (emit_result != 32) {
        NOPE("Fee Reflect Hook: Error: Failed to emit transaction");
    }


    DONE("Fee Reflect Hook: Incoming payment processed, fee reflected");

    _g(1,1);
    return 0;
}