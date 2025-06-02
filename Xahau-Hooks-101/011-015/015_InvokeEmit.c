/**
 * HandyHooks - 015 - Invoke Emit.
 * This hook limits the incoming payment and forwards an amount to an acounts.
 * Install on ttPayment with the following hook params: AM, AO, A1.
 * Convert decimal to uint64 (https://transia-rnd.github.io/xrpl-hex-visualizer/).
 * Example: "AM": "000000000000000A", "AO": "0000000000000005",
 * Convert rADDRESS to Account ID (https://hooks.services/tools/raddress-to-accountid).
 * Example: "A1": "05A506F1996C9E045C1CE7EE7DF7E599C19A6D52", "NUM": "0000000000000001".
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

int64_t hook(uint32_t reserved)
{
    TRACESTR("Invoke Emit: Called.");

    // Hook and origin accounts
    uint8_t hook_acc[20];
    hook_account(hook_acc, 20);

    uint8_t otxn_acc[20];
    otxn_field(otxn_acc, 20, sfAccount);

    // Transaction type
    int64_t tt = otxn_type();
    
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc) && tt == ttPAYMENT)
        DONE("Invoke Emit: Outgoing payment accepted");

    // State keys
    uint64_t am_num = 0x00000000000F4240; // AM (exactAmount)
    uint8_t am_buf[8];
    UINT64_TO_BUF(am_buf, am_num);

    uint64_t ao_num = 0x00000000000F4241; // AO (amountOut)
    uint8_t ao_buf[8];
    UINT64_TO_BUF(ao_buf, ao_num);
    
    uint64_t a1_num = 0x00000000000F4242; // A1 (destination account)
    uint8_t a1_buf[8];
    UINT64_TO_BUF(a1_buf, a1_num);


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
            NOPE("Invoke Emit: Error: Only hook owner can change settings");

        
        // Set AM state
        if (is_am > 0)
        {
            uint64_t am_value = UINT64_FROM_BUF(am_param);
            if (am_value <= 0)
                NOPE("Invoke Emit: Error: AM must be positive");
            if (state_set(SBUF(am_param), SBUF(am_buf)) < 0)
                NOPE("Invoke Emit: Error: Failed to set AM state");
            TRACEVAR(am_value);
            DONE("Invoke Emit: AM state set successfully");
        }

        // Set AO state
        if (is_ao > 0)
        {
            uint64_t ao_value = UINT64_FROM_BUF(ao_param);
            if (ao_value <= 0)
                NOPE("Invoke Emit: Error: AO must be positive");
            if (state_set(SBUF(ao_param), SBUF(ao_buf)) < 0)
                NOPE("Invoke Emit: Error: Failed to set AO state");
            TRACEVAR(ao_value);
            DONE("Invoke Emit: AO state set successfully");
        }

        // Set A1 state
        if (is_a1 > 0)
        {
            if (BUFFER_EQUAL_20(a1_param, hook_acc))
                NOPE("Invoke Emit: Error: A1 cannot match hook account");
            if (state_set(SBUF(a1_param), SBUF(a1_buf)) < 0)
                NOPE("Invoke Emit: Error: Failed to set A1 state");
            TRACEHEX(a1_param);
            DONE("Invoke Emit: A1 state set successfully");
        }

        NOPE("Invoke Emit: Error: No valid parameters provided for invoke");
    }

    // Handle ttPAYMENT (type 0)
    if (tt == ttPAYMENT)
    {
        // Load parameters from state
        uint8_t am_state[8];
        int64_t exact_amount;
        if (state(SBUF(am_state), SBUF(am_buf)) < 0)
            NOPE("Invoke Emit: Error: AM state not set");
        exact_amount = UINT64_FROM_BUF(am_state);
        if (exact_amount <= 0)
            NOPE("Invoke Emit: Error: AM must be positive");

        uint8_t ao_state[8];
        int64_t amount_out;
        if (state(SBUF(ao_state), SBUF(ao_buf)) < 0)
            NOPE("Invoke Emit: Error: AO state not set");

        amount_out = UINT64_FROM_BUF(ao_state) * 1000000.0; // Convert to drops
        if (amount_out <= 0)
            NOPE("Invoke Emit: Error: AO must be positive");

        uint8_t ftxn_acc[20];
        if (state(SBUF(ftxn_acc), SBUF(a1_buf)) < 0)
            NOPE("Invoke Emit: Error: A1 state not set");

        if (BUFFER_EQUAL_20(ftxn_acc, hook_acc))
            NOPE("Invoke Emit: Error: A1 cannot match hook account");

        // Validate incoming payment
        uint8_t amount_buffer[48];
        int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
        int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
        double xah_amount = (double)otxn_drops / 1000000.0; // Convert to XAH
        TRACEVAR(xah_amount);

        // Ensure the payment is XAH
        if (amount_len != 8)
            NOPE("Invoke Emit: Error: Non-XAH payment rejected");
            
        // Check if payment matches exact_amount
        if (xah_amount != exact_amount)
            NOPE("Invoke Emit: Error: Payment amount doesn't match AM");

        // Reserve space for the emitted transaction
            etxn_reserve(1);

            // Prepare the first payment transactions
            uint8_t txn[PREPARE_PAYMENT_SIMPLE_SIZE];
            PREPARE_PAYMENT_SIMPLE(txn, amount_out, ftxn_acc, 0, 0);

            uint8_t emithash[32];

            // Emit the transaction and check if it was successful
            if(emit(SBUF(emithash), SBUF(txn)) != 32) {
                NOPE("Invoke Emit: Error: Failed to emit transactions");
            }
            
            DONE("Invoke Emit: Payment recieved and forwarded Successfully");

            }

    NOPE("Invoke Emit: Error: Transaction type not supported");

    GUARD(1);
    return 0;
}
