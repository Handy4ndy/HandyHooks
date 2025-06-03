/**
 * HandyHooks - 016 - Invoke Multi Emit.
 * This hook limits the incoming payment and forwards an amount to multiple acounts.
 * Install on ttPayment with the following hook params: AM, AO, A1, NUM.
 * Convert decimal to 64bit hex (flipped https://hooks.services/tools/decimal-to-hex-to-fliphex).
 * Example: "AM": "000000000000000A", "AO": "0000000000000005",
 * Convert rADDRESS to Account ID (https://hooks.services/tools/raddress-to-accountid).
 * Example: "A1": "05A506F1996C9E045C1CE7EE7DF7E599C19A6D52", "NUM": "0000000000000001".
 *  Example: "A1": "95DB318777D081E08B9D7CE472F2596FE5C30ACE", "NUM": "0000000000000002".
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)
#define GUARD(maxiter) _g(__LINE__, (maxiter)+1)

// Macro to convert buffer to uint64_t
#define UINT64_FROM_BUF(buf) \
    (((uint64_t)(buf)[0] << 56) + ((uint64_t)(buf)[1] << 48) + \
     ((uint64_t)(buf)[2] << 40) + ((uint64_t)(buf)[3] << 32) + \
     ((uint64_t)(buf)[4] << 24) + ((uint64_t)(buf)[5] << 16) + \
     ((uint64_t)(buf)[6] << 8) + (uint64_t)(buf)[7])

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

int64_t hook(uint32_t reserved)
{
    TRACESTR("Invoke Multi Emit: Called.");

    // Hook and origin accounts
    uint8_t hook_acc[20];
    hook_account(hook_acc, 20);

    uint8_t otxn_acc[20];
    otxn_field(otxn_acc, 20, sfAccount);

    // Transaction type
    int64_t tt = otxn_type();
    
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc) && tt == ttPAYMENT)
        DONE("Invoke Multi Emit: Outgoing payment accepted");

    // State keys
    uint64_t am_num = 0x00000000000F4240; // AM (exactAmount)
    uint8_t am_buf[8];
    UINT64_TO_BUF(am_buf, am_num);

    uint64_t ao_num = 0x00000000000F4241; // AO (amountOut)
    uint8_t ao_buf[8];
    UINT64_TO_BUF(ao_buf, ao_num);
    
    uint64_t count_num = 0x00000000000F4242; // COUNT (number of destination accounts)
    uint8_t count_buf[8];
    UINT64_TO_BUF(count_buf, count_num);

    // Parameters for invoke transactions
    uint8_t am_param[8];
    uint8_t am_key[2] = {'A', 'M'};
    int8_t is_am = otxn_param(SBUF(am_param), SBUF(am_key));

    uint8_t ao_param[8];
    uint8_t ao_key[2] = {'A', 'O'};
    int8_t is_ao = otxn_param(SBUF(ao_param), SBUF(ao_key));

    uint8_t a1_param[20];
    uint8_t a1_key[2] = {'A', '1'};
    int8_t is_a1 = otxn_param(SBUF(a1_param), SBUF(a1_key));


    // Handle ttINVOKE (type 99) for setting parameters
    if (tt == 99)
    {
        // Verify hook owner
        int equal = 0;
        BUFFER_EQUAL(equal, otxn_acc, hook_acc, 20);
        if (!equal)
            NOPE("Invoke Multi Emit: Error: Only hook owner can change settings");


        // Set AM state
        if (is_am > 0)
        {
            uint64_t am_value = UINT64_FROM_BUF(am_param);
            if (am_value <= 0)
                NOPE("Invoke Multi Emit: Error: AM must be positive");
            if (state_set(SBUF(am_param), SBUF(am_buf)) < 0)
                NOPE("Invoke Multi Emit: Error: Failed to set AM state");
            TRACEVAR(am_value);
            DONE("Invoke Multi Emit: AM state set successfully");
        }

        // Set AO state
        if (is_ao > 0)
        {
            uint64_t ao_value = UINT64_FROM_BUF(ao_param);
            if (ao_value <= 0)
                NOPE("Invoke Multi Emit: Error: AO must be positive");
            if (state_set(SBUF(ao_param), SBUF(ao_buf)) < 0)
                NOPE("Invoke Multi Emit: Error: Failed to set AO state");
            TRACEVAR(ao_value);
            DONE("Invoke Multi Emit: AO state set successfully");
        }

        // Set A1 state (add new account with NUM key)
        if (is_a1 > 0)
        {
            if (BUFFER_EQUAL_20(a1_param, hook_acc))
                NOPE("Invoke Multi Emit: Error: A1 cannot match hook account");

            // Load COUNT state
            uint8_t count_state[8];
            int64_t count = 0;
            if (state(SBUF(count_state), SBUF(count_buf)) >= 0)
                count = UINT64_FROM_BUF(count_state);

            // Increment COUNT
            count++;
            UINT64_TO_BUF(count_state, count);
            if (state_set(SBUF(count_state), SBUF(count_buf)) < 0)
                NOPE("Invoke Multi Emit: Error: Failed to set COUNT state");

            // Store account with NUM key (count as key)
            uint8_t num_buf[8];
            UINT64_TO_BUF(num_buf, count);
            if (state_set(SBUF(a1_param), SBUF(num_buf)) < 0)
                NOPE("Invoke Multi Emit: Error: Failed to set A1 state");
            TRACEHEX(a1_param);
            DONE("Invoke Multi Emit: A1 state set successfully");
        }

        NOPE("Invoke Multi Emit: Error: No valid parameters provided for invoke");
    }

    // Handle ttPAYMENT (type 0)
    if (tt == ttPAYMENT)
    {
        // Load parameters from state
        uint8_t am_state[8];
        int64_t exact_amount;
        if (state(SBUF(am_state), SBUF(am_buf)) < 0)
            NOPE("Invoke Multi Emit: Error: AM state not set");
        exact_amount = UINT64_FROM_BUF(am_state);
        if (exact_amount <= 0)
            NOPE("Invoke Multi Emit: Error: AM must be positive");

        uint8_t ao_state[8];
        int64_t amount_out;
        if (state(SBUF(ao_state), SBUF(ao_buf)) < 0)
            NOPE("Invoke Multi Emit: Error: AO state not set");
        amount_out = UINT64_FROM_BUF(ao_state) * 1000000;
        if (amount_out <= 0)
            NOPE("Invoke Multi Emit: Error: AO must be positive");

        // Load COUNT state
        uint8_t count_state[8];
        int64_t count = 0;
        if (state(SBUF(count_state), SBUF(count_buf)) < 0)
            NOPE("Invoke Multi Emit: Error: COUNT state not set");
        count = UINT64_FROM_BUF(count_state);
        if (count <= 0)
            NOPE("Invoke Multi Emit: Error: No destination accounts set");

        // Convert the amount from drops to XAH
        unsigned char amount_buffer[48];
        int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
        int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
        double xah_amount = (double)otxn_drops / 1000000; // Convert to XAH
        TRACEVAR(xah_amount);

        // Ensure the payment is XAH
        if (amount_len != 8){
            NOPE("Invoke Multi Emit: Error: Non-XAH payment rejected.");
        }

        // Check if the payment is equal to the exact amount
        if (xah_amount != exact_amount){
            NOPE("Invoke Multi Emit: Error: Payment amount doesn't match the exact_amount_value.");
        }

        // Reserve space for emitted transactions (one per account)
        etxn_reserve(count);

        // Loop through all accounts and emit transactions
        for (int64_t i = 1; GUARD(100), i <= count; i++)
        {
            // Load destination account from state
            uint8_t ftxn_acc[20];
            uint8_t num_buf[8];
            UINT64_TO_BUF(num_buf, i);
            if (state(SBUF(ftxn_acc), SBUF(num_buf)) < 0)
                NOPE("Invoke Multi Emit: Error: Failed to load destination account");

            if (BUFFER_EQUAL_20(ftxn_acc, hook_acc))
                NOPE("Invoke Multi Emit: Error: Destination account cannot match hook account");

            // Prepare payment transaction
            uint8_t txn[PREPARE_PAYMENT_SIMPLE_SIZE];
            PREPARE_PAYMENT_SIMPLE(txn, amount_out, ftxn_acc, 0, 0);

            // Emit the transaction
            uint8_t emithash[32];
            if (emit(SBUF(emithash), SBUF(txn)) != 32)
                NOPE("Invoke Multi Emit: Error: Failed to emit transaction");
        }

        DONE("Invoke Multi Emit: Payment received and forwarded to all accounts successfully");
    }

    NOPE("Invoke Multi Emit: Error: Transaction type not supported");

    GUARD(1);
    return 0;
}
