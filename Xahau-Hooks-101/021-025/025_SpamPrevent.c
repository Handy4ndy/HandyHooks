/**
 * HandyHooks - 025 - Spam Prevent Hook.
 * Allows outgoing payment transactions and restricts incoming payments based on:
 * - MIN: Rejects payments below 1 XAH if true.
 * - PAR: Rejects incoming payments with hook parameters if true.
 * - MEM: Rejects incoming payments with memos if true.
 * Parameters are toggled via ttINVOKE (00/01) and stored in hook state with 8-byte keys.
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

// Minimum amount in XAH
#define MIN_AMOUNT_VALUE 1 // 1 XAH

int64_t hook(uint32_t reserved) {
    TRACESTR("Spam Prevent Hook: Called.");

    // Get hook account
    uint8_t hook_acc[20];
    hook_account(hook_acc, 20);

    // Get originating account
    uint8_t otxn_acc[20];
    otxn_field(otxn_acc, 20, sfAccount);

    // Get transaction type
    int64_t tt = otxn_type();

    // State keys (8 bytes each, unique uint64 values)

    // MIN: Rejects payments below 1 XAH if true.
    uint64_t min_num = 0x000000000F4D494E; // MIN
    uint8_t min_buf[8];
    UINT64_TO_BUF(min_buf, min_num);

    // PAR: Rejects incoming payments with hook parameters if true.
    uint64_t par_num = 0x000000000F504152; 
    uint8_t par_buf[8];
    UINT64_TO_BUF(par_buf, par_num);

    // MEM: Rejects incoming payments with memos if true.
    uint64_t mem_num = 0x000000000F4D454D; 
    uint8_t mem_buf[8];
    UINT64_TO_BUF(mem_buf, mem_num);

    // Handle ttINVOKE (type 99) to update parameters
    if (tt == 99) {
        
        // Verify hook owner
        int equal = 0;
        BUFFER_EQUAL(equal, otxn_acc, hook_acc, 20);
        if (!equal)
            NOPE("Spam Prevent Hook: Error: Only hook owner can change settings");

        // Parameters for invoke transactions
        uint8_t min_param[1];
        uint8_t min_key[3] = {'M','I','N'};
        int8_t is_min = otxn_param(SBUF(min_param), SBUF(min_key));

        uint8_t par_param[1];
        uint8_t par_key[3] = {'P','A','R'};
        int8_t is_par = otxn_param(SBUF(par_param), SBUF(par_key));

        uint8_t mem_param[1];
        uint8_t mem_key[3] = {'M','E','M'};
        int8_t is_mem = otxn_param(SBUF(mem_param), SBUF(mem_key));

        // Set MIN state
        if (is_min > 0) {
            if (min_param[0] > 1)
                NOPE("Spam Prevent Hook: Error: MIN must be 0 or 1");
            if (state_set(SBUF(min_param), SBUF(min_buf)) < 0)
                NOPE("Spam Prevent Hook: Error: Failed to set MIN state");
            TRACEVAR(min_param[0]);
            DONE("Spam Prevent Hook: MIN state set successfully");
        }

        // Set PAR state
        if (is_par > 0) {
            if (par_param[0] > 1)
                NOPE("Spam Prevent Hook: Error: PAR must be 0 or 1");
            if (state_set(SBUF(par_param), SBUF(par_buf)) < 0)
                NOPE("Spam Prevent Hook: Error: Failed to set PAR state");
            TRACEVAR(par_param[0]);
            DONE("Spam Prevent Hook: PAR state set successfully");
        }

        // Set MEM state
        if (is_mem > 0) {
            if (mem_param[0] > 1)
                NOPE("Spam Prevent Hook: Error: MEM must be 0 or 1");
            if (state_set(SBUF(mem_param), SBUF(mem_buf)) < 0)
                NOPE("Spam Prevent Hook: Error: Failed to set MEM state");
            TRACEVAR(mem_param[0]);
            DONE("Spam Prevent Hook: MEM state set successfully");
        }

        NOPE("Spam Prevent Hook: Error: No valid parameters provided for invoke");
    }

    // Accept outgoing payments
    if (tt == ttPAYMENT && BUFFER_EQUAL_20(hook_acc, otxn_acc))
        DONE("Spam Prevent Hook: Outgoing payment accepted.");

    // Check incoming payments
    if (tt == ttPAYMENT) {
        uint8_t value;

        // MIN: Block payments below 1 XAH
        if (state(SBUF(&value), SBUF(min_buf)) < 0)
            value = 0; 
        if (value) {
            uint8_t amount_buffer[48];
            int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
            if (amount_len != 8)
                DONE("Spam Prevent Hook: Non-XAH payment accepted.");
            int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
            double xah_amount = (double)otxn_drops / 1000000.0;
            if (xah_amount < MIN_AMOUNT_VALUE)
                NOPE("Spam Prevent Hook: Payment below minimum amount.");
        }

        // PAR: Block payments with hook parameters
        if (state(SBUF(&value), SBUF(par_buf)) < 0)
            value = 0; 
        if (value) {
            uint8_t param_buf[256];
            int64_t param_len = otxn_field(SBUF(param_buf), sfHookParameters);
            if (param_len > 0)
                NOPE("Spam Prevent Hook: Payment with hook parameter Blocked.");
        }

        // MEM: Block payments with memos
        if (state(SBUF(&value), SBUF(mem_buf)) < 0)
            value = 0; 
        if (value) {
            uint8_t memo_buf[256]; 
            int64_t memo_len = otxn_field(SBUF(memo_buf), sfMemos);
            if (memo_len > 0)
                NOPE("Spam Prevent Hook: Payment with memo Blocked.");
        }

        DONE("Spam Prevent Hook: Incoming payment accepted.");
    }

    NOPE("Spam Prevent Hook: Error: Unsupported transaction type.");

    _g(1,1)
    return 0;
}