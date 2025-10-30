#include "hookapi.h"
#include <stdint.h>

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

// Buffer for parameter names (max 32 bytes as per Hook API)
#define PARAM_NAME_MAX 32
// Buffer for parameter values
#define PARAM_VALUE_MAX 128
// Parameter names as arrays
static uint8_t param_name_am[] = {0x41U, 0x4DU}; // "AM" in ASCII
static uint8_t param_name_ac[] = {0x41U, 0x43U}; // "AC" in ASCII

// Hex char to byte conversion
static inline uint8_t hex_to_byte(uint8_t c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0xFF; // Invalid hex char
}

// Decode 40-byte hex string to 20-byte binary
static int64_t decode_hex(uint8_t* dest, uint8_t* src, int64_t len)
{
    if (len != 40) return -1;
    for (int64_t i = 0; i < 20; i++)
    {
        uint8_t high = hex_to_byte(src[i * 2]);
        uint8_t low = hex_to_byte(src[i * 2 + 1]);
        if (high == 0xFF || low == 0xFF) return -1;
        dest[i] = (high << 4) | low;
    }
    return 20;
}

int64_t hook(uint32_t reserved)
{
    TRACESTR("ForwardPaymentHook: Started.");

    // Validate transaction type (must be Payment)
    int64_t tt = otxn_type();
    if (tt != ttPAYMENT)
    {
        NOPE("ForwardPaymentHook: Error: Only Payment transactions are processed.");
    }

    // Get hook account for validation
    uint8_t hook_acc[20];
    if (hook_account(SBUF(hook_acc)) != 20)
    {
        NOPE("ForwardPaymentHook: Error: Failed to get hook account.");
    }

    // Get origin transaction account
    uint8_t otxn_acc[20];
    if (otxn_field(SBUF(otxn_acc), sfAccount) != 20)
    {
        NOPE("ForwardPaymentHook: Error: Failed to get origin account.");
    }

    // Accept outgoing payments from hook account
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc))
    {
        DONE("ForwardPaymentHook: Outgoing payment accepted without forwarding.");
    }

    // Validate incoming payment is in XAH
    uint8_t amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    if (amount_len != 8)
    {
        NOPE("ForwardPaymentHook: Error: Non-XAH payment rejected.");
    }

    // Extract incoming amount in drops
    int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    if (otxn_drops <= 0)
    {
        NOPE("ForwardPaymentHook: Error: Invalid or zero amount.");
    }
    // TRACEVAR(otxn_drops);

    // Extract AM parameter (uint64 amount in drops)
    uint8_t param_am[PARAM_VALUE_MAX];
    int64_t param_am_len = otxn_param(SBUF(param_am), param_name_am, sizeof(param_name_am));
    if (param_am_len == DOESNT_EXIST)
    {
        NOPE("ForwardPaymentHook: Error: AM parameter not found.");
    }
    else if (param_am_len != 8)
    {
        NOPE("ForwardPaymentHook: Error: AM parameter must be 8 bytes (uint64).");
    }
    else if (param_am_len < 0)
    {
        NOPE("ForwardPaymentHook: Error: Failed to read AM parameter.");
    }

    // Convert AM parameter to drops
    int64_t am_drops = AMOUNT_TO_DROPS(param_am);
    if (am_drops <= 0)
    {
        NOPE("ForwardPaymentHook: Error: Invalid AM parameter amount.");
    }
    // TRACESTR("ForwardPaymentHook: AM parameter found:");
    // TRACEHEX(param_name_am);
    // TRACEVAR(param_am_len);
    TRACEVAR(am_drops);

    int64_t am_send = am_drops * 1000000;

    TRACEVAR(am_send);

    // Extract AC parameter (20-byte account ID or 40-byte hex string)
    uint8_t param_ac_raw[PARAM_VALUE_MAX];
    int64_t param_ac_len = otxn_param(SBUF(param_ac_raw), param_name_ac, sizeof(param_name_ac));
    uint8_t param_ac[20];
    if (param_ac_len == DOESNT_EXIST)
    {
        NOPE("ForwardPaymentHook: Error: AC parameter not found.");
    }
    else if (param_ac_len == 20)
    {
        // Direct binary account ID
        for (int i = 0; i < 20; i++) param_ac[i] = param_ac_raw[i];
    }
    else if (param_ac_len == 40)
    {
        // Hex string, decode to binary
        if (decode_hex(param_ac, param_ac_raw, param_ac_len) != 20)
        {
            NOPE("ForwardPaymentHook: Error: Invalid AC hex string format.");
        }
    }
    else
    {
        NOPE("ForwardPaymentHook: Error: AC parameter must be 20 bytes (binary) or 40 bytes (hex).");
    }

    // Validate AC is not hook account
    if (BUFFER_EQUAL_20(hook_acc, param_ac))
    {
        NOPE("ForwardPaymentHook: Error: AC parameter cannot be hook account.");
    }
    // TRACESTR("ForwardPaymentHook: AC parameter found:");
    // TRACEHEX(param_name_ac);
    TRACEHEX(param_ac);
    // TRACEVAR(param_ac_len);

    // Reserve space for two emitted transactions
     etxn_reserve(1);

     // Prepare the first payment transactions
     uint8_t txn1[PREPARE_PAYMENT_SIMPLE_SIZE];
     PREPARE_PAYMENT_SIMPLE(txn1, am_send, param_ac, 0, 0);

     uint8_t emithash1[32];
    

     // Emit the transactions and check if they were successful
     if(emit(SBUF(emithash1), SBUF(txn1)) != 32)
     {
         NOPE("Hard Multi Emit: Error: Failed to emit transactions");
     }

    // Accept the original transaction
    DONE("ForwardPaymentHook: Payment forwarded successfully.");

    // Prevent reentrancy
    GUARD(1);
    return 0;
}