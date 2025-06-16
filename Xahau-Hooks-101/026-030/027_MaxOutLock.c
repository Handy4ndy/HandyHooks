/**
 * HandyHooks - 027 Maximum Out with lock Hook
 * Restricts outgoing payments to a configurable maximum without a pass parameter:
 * - MAX: Enables/disables the maximum amount check (0 or 1).
 * - AMT: Sets the maximum allowed amount in XAH (input as uint64_t, converted to drops internally).
 * - PASS: A parameter that, if present, bypasses the maximum amount restriction.
 * Parameters are toggled via ttINVOKE and stored in hook state with 8-byte keys.
 * Install with ttPayment and ttInvoke triggers.
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)
#define GUARD(maxiter) _g(__LINE__, (maxiter)+1)

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

// Conversion factor: 1 XAH = 1,000,000 drops
#define XAH_TO_DROPS(xah) ((uint64_t)(xah) * 1000000ULL)

int64_t hook(uint32_t reserved) {
    TRACESTR("MaxOut Hook: Called.");

    // Get hook account
    uint8_t hook_acc[20];
    hook_account(hook_acc, 20);

    // Get originating account (sfAccount)
    uint8_t otxn_acc[20];
    otxn_field(otxn_acc, 20, sfAccount);

    // Get destination account (sfDestination)
    uint8_t dest_acc[20];
    otxn_field(dest_acc, 20, sfDestination);

    // Get transaction type
    int64_t tt = otxn_type();

    // State keys (8 bytes each, unique uint64 values)
    uint64_t max_num = 0x000000000F4D4158; // MAX
    uint8_t max_buf[8];
    UINT64_TO_BUF(max_buf, max_num);

    uint64_t amt_num = 0x000000000F414D54; // AMT
    uint8_t amt_buf[8];
    UINT64_TO_BUF(amt_buf, amt_num);

    // Handle ttINVOKE (type 99) to update parameters
    if (tt == 99) {
        // Verify hook owner
        int equal = 0;
        BUFFER_EQUAL(equal, otxn_acc, hook_acc, 20);
        if (!equal)
            NOPE("MaxOut Hook: Error: Only hook owner can change settings");

        // Parameters for invoke transactions
        uint8_t max_param[1];
        uint8_t max_key[3] = {'M','A','X'};
        int8_t is_max = otxn_param(SBUF(max_param), SBUF(max_key));

        uint8_t amt_param[8];
        uint8_t amt_key[3] = {'A','M','T'};
        int8_t is_amt = otxn_param(SBUF(amt_param), SBUF(amt_key));

        // Set MAX state
        if (is_max > 0) {
            if (max_param[0] > 1)
                NOPE("MaxOut Hook: Error: MAX must be 0 or 1");
            if (state_set(SBUF(max_param), SBUF(max_buf)) < 0)
                NOPE("MaxOut Hook: Error: Failed to set MAX state");
            TRACEVAR(max_param[0]);
            DONE("MaxOut Hook: MAX state set successfully");
        }

        // Set AMT state (amount in XAH, convert to drops)
        if (is_amt > 0) {
            uint64_t amt_xah = UINT64_FROM_BUF(amt_param);
            if (amt_xah < 1)
                NOPE("MaxOut Hook: Error: AMT must be at least 1 XAH");
            uint64_t amt_drops = XAH_TO_DROPS(amt_xah);
            uint8_t amt_drops_buf[8];
            UINT64_TO_BUF(amt_drops_buf, amt_drops);
            if (state_set(SBUF(amt_drops_buf), SBUF(amt_buf)) < 0)
                NOPE("MaxOut Hook: Error: Failed to set AMT state");
            TRACEVAR(amt_drops);
            DONE("MaxOut Hook: AMT state set successfully");
        }

        NOPE("MaxOut Hook: Error: No valid parameters provided for invoke");
    }

    // Handle payment transactions
    if (tt == ttPAYMENT) {
        // Accept incoming payments (destination is hook account)
        if (BUFFER_EQUAL_20(dest_acc, hook_acc))
            DONE("MaxOut Hook: Incoming payment accepted.");

        // Handle outgoing payments (originating account is hook account)
        if (BUFFER_EQUAL_20(otxn_acc, hook_acc)) {
            uint8_t value;
            if (state(SBUF(&value), SBUF(max_buf)) < 0)
                value = 0; // Default to false

            if (value) {
                // Get transaction amount
                uint8_t amount_buffer[48];
                int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
                if (amount_len != 8)
                    DONE("MaxOut Hook: Non-XAH payment accepted.");

                int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);

                // Get maximum allowed amount
                uint8_t max_amt_buf[8];
                uint64_t max_drops = 1000000; // Default to 1 XAH in drops
                if (state(SBUF(max_amt_buf), SBUF(amt_buf)) >= 0)
                    max_drops = UINT64_FROM_BUF(max_amt_buf);

                // Check if amount exceeds maximum
                if (otxn_drops > max_drops) {
                    // Check for PASS parameter to bypass restriction
                    uint8_t pass_param[1];
                    uint8_t pass_key[4] = {'P','A','S','S'};
                    int8_t has_pass = otxn_param(SBUF(pass_param), SBUF(pass_key));
                    if (has_pass <= 0 || pass_param[0] != 1) {
                        NOPE("MaxOut Hook: Payment above maximum amount and no valid PASS parameter.");
                    }
                    DONE("MaxOut Hook: PASS parameter provided, bypassing max amount check.");
                }
            }
        }

        DONE("MaxOut Hook: Outgoing payment accepted.");
    }

    NOPE("MaxOut Hook: Error: Unsupported transaction type.");

    
    _g(1, 1);
    return 0;
}