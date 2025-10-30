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
#define UINT64_FROM_BUF(a) \
    (((uint64_t)(a)[0] << 56) + ((uint64_t)(a)[1] << 48) + \
     ((uint64_t)(a)[2] << 40) + ((uint64_t)(a)[3] << 32) + \
     ((uint64_t)(a)[4] << 24) + ((uint64_t)(a)[5] << 16) + \
     ((uint64_t)(a)[6] << 8) + (uint64_t)(a)[7])

int64_t hook(uint32_t _)
{
    TRACESTR("VendorSplitEmit: Called.");

    // Hook and origin account
    uint8_t hook_acc[20];
    hook_account(hook_acc, 20);

    uint8_t otxn_acc[20];
    otxn_field(otxn_acc, 20, sfAccount);

    // Transaction type
    int64_t tt = otxn_type();

    // Accept outgoing payments
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc) && tt == ttPAYMENT)
        DONE("Outgoing payment accepted.");

    // Only handle ttPAYMENT for incoming
    if (tt != ttPAYMENT)
        NOPE("Only Payment transactions supported.");

    // Validate incoming payment
    uint8_t amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);

    // Ensure the payment is XAH
    if (amount_len != 8)
        NOPE("Non-XAH payment rejected.");

    // Retrieve and validate hook parameters
    uint64_t total_amount = 0;
    int64_t vendor_count = 0;
    uint64_t amounts[8]; // Up to 8 vendors
    uint8_t accounts[8][20];
    uint8_t seen_accounts[8][20];
    int64_t seen_count = 0;

    // Iterate through parameters (AM1/AC1, AM2/AC2, ...)
    for (int i = 1; GUARD(8), i <= 8; i++)
    {
        // Construct parameter keys (e.g., "AM1", "AC1")
        uint8_t am_key[3] = {'A', 'M', '0' + i};
        uint8_t ac_key[3] = {'A', 'C', '0' + i};

        // Get amount (AMn)
        uint8_t am_param[8];
        int64_t am_result = hook_param(SBUF(am_param), SBUF(am_key));
        TRACEVAR(am_result);
        if (am_result != 8)
            break; // No more vendors

        uint64_t amount = UINT64_FROM_BUF(am_param);
        TRACEVAR(amount);
        if (amount <= 0)
            NOPE("Amount must be positive.");

        // Get account (ACn)
        uint8_t ac_param[20];
        int64_t ac_result = hook_param(SBUF(ac_param), SBUF(ac_key));
        TRACEVAR(ac_result);
        if (ac_result != 20)
            NOPE("Invalid account parameter.");

        // Validate account
        if (BUFFER_EQUAL_20(ac_param, hook_acc))
            NOPE("Vendor account cannot match hook account.");

        // Check for unique accounts
        for (int j = 0; GUARD(64), j < seen_count; j++)
            if (BUFFER_EQUAL_20(ac_param, seen_accounts[j]))
                NOPE("Duplicate vendor account detected.");

        // Store amount and account
        amounts[vendor_count] = amount;
        for (int j = 0; GUARD(20), j < 20; j++)
            accounts[vendor_count][j] = ac_param[j];
        for (int j = 0; GUARD(20), j < 20; j++)
            seen_accounts[seen_count][j] = ac_param[j];
        seen_count++;
        vendor_count++;

        // Accumulate total amount (in drops)
        total_amount += amount * 1000000;
    }

    // Ensure at least one vendor
    TRACEVAR(vendor_count);
    if (vendor_count == 0)
        NOPE("No vendor parameters provided.");

    // Check if incoming amount matches total
    TRACEVAR(otxn_drops);
    TRACEVAR(total_amount);
    if (otxn_drops != total_amount)
        NOPE("Payment amount does not match sum of vendor amounts.");

    // Reserve space for emitted transactions
    etxn_reserve(vendor_count);

    // Emit payments to vendors
    for (int i = 0; GUARD(8), i < vendor_count; i++)
    {
        // Prepare payment transaction
        uint8_t txn[PREPARE_PAYMENT_SIMPLE_SIZE];
        uint64_t amount_drops = amounts[i] * 1000000;
        PREPARE_PAYMENT_SIMPLE(txn, amount_drops, accounts[i], 0, 0);

        // Emit the transaction
        uint8_t emithash[32];
        if (emit(SBUF(emithash), SBUF(txn)) != 32)
            NOPE("Failed to emit transaction.");
    }

    DONE("Payment received and forwarded to all vendors successfully.");

    GUARD(1);
    return 0;
}