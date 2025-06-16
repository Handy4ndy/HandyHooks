/**
 * HandyHooks - 026 Maximum Out Hook.
 * Allows outgoing payment transactions and restricts outgoing payments to a maximum without the pass param:
 * - MAX: Rejects payments below 1 XAH if true(01).
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

// Maximum amount in XAH
#define MAX_AMOUNT_VALUE 1 // 1 XAH

int64_t hook(uint32_t reserved) {
    TRACESTR("Spam Prevent Hook: Called.");

    // Get hook account
    uint8_t hook_acc[20];
    hook_account(hook_acc, 20);

    // Get originating account
    uint8_t otxn_acc[20];
    otxn_field(otxn_acc, 20, sfAccount);

    // Get destination account
    uint8_t dest_acc[20];
    otxn_field(dest_acc, 20, sfDestination);

    // Get transaction type
    int64_t tt = otxn_type();

    // State keys (8 bytes each, unique uint64 values)
    uint64_t max_num = 0x000000000F4D4158; // MAX
    uint8_t max_buf[8];
    UINT64_TO_BUF(max_buf, max_num);


    // Handle ttINVOKE (type 99) to update parameters
    if (tt == 99) {
        
        // Verify hook owner
        int equal = 0;
        BUFFER_EQUAL(equal, otxn_acc, hook_acc, 20);
        if (!equal)
            NOPE("Maximum Out Hook: Error: Only hook owner can change settings");

        // Parameters for invoke transactions
        uint8_t max_param[1];
        uint8_t max_key[3] = {'M','A','X'};
        int8_t is_max = otxn_param(SBUF(max_param), SBUF(max_key));

        // Set MAX state
        if (is_max > 0) {
            if (max_param[0] > 1)
                NOPE("Maximum Out Hook: Error: MAX must be 0 or 1");
            if (state_set(SBUF(max_param), SBUF(max_buf)) < 0)
                NOPE("Maximum Out Hook: Error: Failed to set MAX state");
            TRACEVAR(max_param[0]);
            DONE("Maximum Out Hook: MAX state set successfully");
        }


        NOPE("Maximum Out Hook: Error: No valid parameters provided for invoke");
    }

    // Accept incoming payments (destination is hook account)
        if (BUFFER_EQUAL_20(dest_acc, hook_acc))
            DONE("MaxOut Hook: Incoming payment accepted.");


    // Check outgoing payments
    if (tt == ttPAYMENT) {

        uint8_t value;

        // MAX: Block payments above 1 XAH
        if (state(SBUF(&value), SBUF(max_buf)) < 0)
            value = 0; // Default to false
        if (value) {
            uint8_t amount_buffer[48];
            int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
            if (amount_len != 8)
                DONE("Maximum Out Hook: Non-XAH payment accepted.");
            int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
            double xah_amount = (double)otxn_drops / 1000000.0;
            if (xah_amount > MAX_AMOUNT_VALUE)
                NOPE("Maximum Out Hook: Payment above maximum amount.");
        }

        DONE("Maximum Out Hook: outgoing payment accepted.");
    }

    NOPE("Maximum Out Hook: Error: Unsupported transaction type.");

    _g(1,1);
    return 0;
}