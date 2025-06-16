/**
 * HandyHooks - 029 - Max Fee Hook
 * Limits the fee of outgoing ttPAYMENT transactions to a configured maximum.
 * Logs sfFee in drops using otxn_field and XRP_AMOUNT_TO_DROPS.
 * - MF: Configure the Maximum fee allowed.
 * - FE: Fee Check state to Enabled (0 or 1).
 * Configurable via ttINVOKE with parameters: MF, FE (Fee Check Enabled).
 * Install on ttPayment and ttInvoke.
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

// Macro to convert uint64_t to buffer
#define UINT64_TO_BUF(buf, i) \
{ \
    (buf)[0] = (i >> 56) & 0xFFU; \
    (buf)[1] = (i >> 48) & 0xFFU; \
    (buf)[2] = (i >> 40) & 0xFFU; \
    (buf)[3] = (i >> 32) & 0xFFU; \
    (buf)[4] = (i >> 24) & 0xFFU; \
    (buf)[5] = (i >> 16) & 0xFFU; \
    (buf)[6] = (i >> 8) & 0xFFU; \
    (buf)[7] = i & 0xFFU; \
}

// Macro to convert buffer to uint64_t
#define UINT64_FROM_BUF(buf) \
    (((uint64_t)(buf)[0] << 56) + ((uint64_t)(buf)[1] << 48) + \
     ((uint64_t)(buf)[2] << 40) + ((uint64_t)(buf)[3] << 32) + \
     ((uint64_t)(buf)[4] << 24) + ((uint64_t)(buf)[5] << 16) + \
     ((uint64_t)(buf)[6] << 8) + (uint64_t)(buf)[7])


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
    TRACESTR("Max Fee Hook: Called.");

    // Get transaction type
    int64_t tt = otxn_type();
    TRACEVAR(tt);

    // Get hook and origin accounts
    uint8_t hook_acc[20];
    hook_account(SBUF(hook_acc));
    uint8_t otxn_acc[20];
    int64_t acc_len = otxn_field(SBUF(otxn_acc), sfAccount);
    if (acc_len != 20)
        NOPE("Max Fee Hook: Error: Failed to get origin account");

    // State keys
    uint64_t mf_num = 0x000000000F4D4645; // MF (Max Fee)
    uint8_t mf_buf[8];
    UINT64_TO_BUF(mf_buf, mf_num);

    uint64_t fe_num = 0x000000000F464520; // FE (Fee Check Enabled)
    uint8_t fe_buf[8];
    UINT64_TO_BUF(fe_buf, fe_num);

    // Handle ttINVOKE to update parameters
    if (tt == ttINVOKE) {
        // Verify hook owner
        if (!BUFFER_EQUAL_20(hook_acc, otxn_acc))
            NOPE("Max Fee Hook: Error: Only hook owner can change settings");

        // Parameters for invoke
        uint8_t mf_param[8];
        uint8_t mf_key[2] = {'M', 'F'};
        int8_t is_mf = otxn_param(SBUF(mf_param), SBUF(mf_key));

        uint8_t fe_param[1];
        uint8_t fe_key[2] = {'F', 'E'};
        int8_t is_fe = otxn_param(SBUF(fe_param), SBUF(fe_key));

        // Set MF (Max Fee) state
        if (is_mf > 0) {
            uint64_t mf_value = UINT64_FROM_BUF(mf_param);
            if (mf_value <= 0)
                NOPE("Max Fee Hook: Error: Max fee must be positive");
            if (state_set(SBUF(mf_param), SBUF(mf_buf)) < 0)
                NOPE("Max Fee Hook: Error: Failed to set MF state");
            TRACEVAR(mf_value);
            DONE("Max Fee Hook: MF state set successfully");
        }

        // Set FE (Fee Check Enabled) state
        if (is_fe > 0) {
            if (fe_param[0] > 1)
                NOPE("Max Fee Hook: Error: FE must be 0 or 1");
            if (state_set(SBUF(fe_param), SBUF(fe_buf)) < 0)
                NOPE("Max Fee Hook: Error: Failed to set FE state");
            TRACEVAR(fe_param[0]);
            DONE("Max Fee Hook: FE state set successfully");
        }

        NOPE("Max Fee Hook: Error: No valid parameters provided for invoke");
    }

    // Handle ttPAYMENT
    if (tt == ttPAYMENT) {
        // Accept incoming payments
        if (!BUFFER_EQUAL_20(hook_acc, otxn_acc))
            DONE("Max Fee Hook: Incoming payment, skipping");

        // Check if fee enforcement is enabled
        uint8_t fe_value;
        if (state(SBUF(fe_value), SBUF(fe_buf)) < 0)
            fe_value = 1; // Default to enabled
        TRACEVAR(fe_value);
        if (!fe_value)
            DONE("Max Fee Hook: Fee check disabled, accepting");

        // Load max fee from state
        uint8_t mf_state[8];
        uint64_t max_fee;
        if (state(SBUF(mf_state), SBUF(mf_buf)) < 0) {
            max_fee = 1000;
        } else {
            max_fee = UINT64_FROM_BUF(mf_state);
            if (max_fee <= 0)
                NOPE("Max Fee Hook: Error: Invalid max fee in state");
        }
        TRACEVAR(max_fee);

        // Get transaction fee
        uint8_t tx_fee[8];
        int64_t fee_len = otxn_field(SBUF(tx_fee), sfFee);
        TRACEVAR(fee_len);
        if (fee_len != 8) {
            NOPE("Max Fee Hook: Error: Failed to get transaction fee");
        }

        // Convert fee to drops
        int64_t fee_drops = XRP_AMOUNT_TO_DROPS(tx_fee);
        TRACEVAR(fee_drops);

        // Enforce max fee
        if (fee_drops > max_fee)
            NOPE("Max Fee Hook: Error: Fee exceeds maximum allowed");

        DONE("Max Fee Hook: Outgoing payment fee within limit");
    }

    DONE("Max Fee Hook: Non-target transaction");

     _g (1,1);
    return 0;
}
