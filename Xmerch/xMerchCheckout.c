/*
  HandyHooks Split Payment Hook
  ----------------------------
  This Hook splits an incoming Payment into multiple outgoing payments
  based on HookParameters AMn (amount in XAH) and ACn (destination account).
  https://hooks-builder.xrpl.org/develop
  Example HookParameters (JSON): --- COPY THIS INTO HOOKS BUILDER AS JSON then EDIT

{
  "Destination": "rk8f7AaxW5vPbWQXVJhh3MG6H3TuzVh6E",
  "Amount": "100000000",
  "Fee": "12",
  "HookParameters": [
    {
      "HookParameter": {
        "HookParameterName": "56504F31",
        "HookParameterValue": "0000000000000032"
      }
    },
    {
      "HookParameter": {
        "HookParameterName": "56504131",
        "HookParameterValue": "D489232F4CF351A8572388D887F8C57A09C166B8"
      }
    },
    {
      "HookParameter": {
        "HookParameterName": "56504F32",
        "HookParameterValue": "0000000000000032"
      }
    },
    {
      "HookParameter": {
        "HookParameterName": "56504132",
        "HookParameterValue": "E80190A479858F27E281CF732BA75C3B6F939572"
      }
    },
    {
      "HookParameter": {
        "HookParameterName": "56504F33",
        "HookParameterValue": "0000000000000032"
      }
    },
    {
      "HookParameter": {
        "HookParameterName": "56504133",
        "HookParameterValue": "05A506F1996C9E045C1CE7EE7DF7E599C19A6D52"
      }
    }
  ],
  "Flags": "2147483648",
  "TransactionType": "Payment",
  "Account": "rNC4GQuq3gL9dD8XinHVcTGi9XDJTuZyBr",
  "Memos": []
}


  HookParameter Details:
    - AMn: Amount in XAH (uint64, 8 bytes, big-endian hex, e.g. "0000000000000064" = 100 XAH) https://transia-rnd.github.io/xrpl-hex-visualizer/
    - ACn: Destination account (20 bytes, hex-encoded classic address) https://hooks.services/tools/raddress-to-accountid
    - n: Index (1-5), up to 5 pairs supported

  The sum of all AMn must not exceed the incoming payment amount.
  Each ACn must be unique and not the Hook's own account.

  Author: HandyHooks
  License: MIT
*/

#include "hookapi.h"
#include <stdint.h>

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)
#define GUARD(maxiter) _g(__LINE__, (maxiter) + 1)

// Macro to convert buffer to uint64_t (from first script)
#define UINT64_FROM_BUF(buf)                                   \
    (((uint64_t)(buf)[0] << 56) + ((uint64_t)(buf)[1] << 48) + \
     ((uint64_t)(buf)[2] << 40) + ((uint64_t)(buf)[3] << 32) + \
     ((uint64_t)(buf)[4] << 24) + ((uint64_t)(buf)[5] << 16) + \
     ((uint64_t)(buf)[6] << 8) + (uint64_t)(buf)[7])

// Buffer for parameter names (max 32 bytes as per Hook API)
#define PARAM_NAME_MAX 32
// Buffer for parameter values
#define PARAM_VALUE_MAX 128
// Maximum number of account/amount pairs to process
#define MAX_ACCOUNTS 5

// Parameter names as arrays
static uint8_t param_name_am[] = {0x56U, 0x50U, 0x4FU}; // "VPO"
static uint8_t param_name_ac[] = {0x56U, 0x50U, 0x41U}; // "VPA"
static uint8_t param_name_am_n[PARAM_NAME_MAX][4] = {
    {0x56U, 0x50U, 0x4FU, 0x31U}, // "VPO1"
    {0x56U, 0x50U, 0x4FU, 0x32U}, // "VPO2"
    {0x56U, 0x50U, 0x4FU, 0x33U}, // "VPO3"
    {0x56U, 0x50U, 0x4FU, 0x34U}, // "VPO4"
    {0x56U, 0x50U, 0x4FU, 0x35U}  // "VPO5"
};
static uint8_t param_name_ac_n[PARAM_NAME_MAX][4] = {
    {0x56U, 0x50U, 0x41U, 0x31U}, // "VPA1"
    {0x56U, 0x50U, 0x41U, 0x32U}, // "VPA2"
    {0x56U, 0x50U, 0x41U, 0x33U}, // "VPA3"
    {0x56U, 0x50U, 0x41U, 0x34U}, // "VPA4"
    {0x56U, 0x50U, 0x41U, 0x35U}  // "VPA5"
};

int64_t hook(uint32_t reserved)
{
    TRACESTR("CheckoutPaymentHook: Started.");

    // Validate transaction type (must be Payment)
    int64_t tt = otxn_type();
    if (tt != ttPAYMENT)
    {
        NOPE("CheckoutPaymentHook: Error: Only Payment transactions are processed.");
    }

    // Get hook account for validation
    uint8_t hook_acc[20];
    if (hook_account(SBUF(hook_acc)) != 20)
    {
        NOPE("CheckoutPaymentHook: Error: Failed to get hook account.");
    }

    // Get origin transaction account
    uint8_t otxn_acc[20];
    if (otxn_field(SBUF(otxn_acc), sfAccount) != 20)
    {
        NOPE("CheckoutPaymentHook: Error: Failed to get origin account.");
    }

    // Accept outgoing payments from hook account
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc))
    {
        DONE("CheckoutPaymentHook: Outgoing payment accepted without forwarding.");
    }

    // Validate incoming payment is in XAH
    uint8_t amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    if (amount_len != 8)
    {
        NOPE("CheckoutPaymentHook: Error: Non-XAH payment rejected.");
    }

    // Extract incoming amount in drops
    int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    if (otxn_drops <= 0)
    {
        NOPE("CheckoutPaymentHook: Error: Invalid or zero amount.");
    }
    TRACEVAR(otxn_drops);

    // Arrays to store amounts and accounts
    int64_t am_drops[MAX_ACCOUNTS];
    uint8_t accounts[MAX_ACCOUNTS][20];
    int64_t account_count = 0;
    int64_t total_am_drops = 0;

    // Loop through possible parameter pairs (AM1/AC1, AM2/AC2, etc.)
    for (int64_t i = 0; GUARD(5), i < MAX_ACCOUNTS; i++)
    {
        // Extract AM parameter (uint64 amount in XAH)
        uint8_t param_am[PARAM_VALUE_MAX];
        int64_t param_am_len = otxn_param(SBUF(param_am), param_name_am_n[i], sizeof(param_name_am_n[i]));
        if (param_am_len == DOESNT_EXIST)
        {
            break;
        }
        else if (param_am_len != 8)
        {
            NOPE("CheckoutPaymentHook: Error: AM parameter must be 8 bytes (uint64).");
        }
        else if (param_am_len < 0)
        {
            NOPE("CheckoutPaymentHook: Error: Failed to read AM parameter.");
        }

        // Convert AM parameter to drops (AM is in XAH)
        uint64_t am_value = UINT64_FROM_BUF(param_am);
        TRACEVAR(am_value); // Log raw AM value for debugging
        if (am_value <= 0)
        {
            NOPE("CheckoutPaymentHook: Error: Invalid AM parameter amount.");
        }

        // Convert XAH to Drops
        am_drops[account_count] = am_value * 1000000;
        total_am_drops += am_drops[account_count];
        TRACEVAR(am_drops[account_count]);

        // Validate total AM amounts match incoming payment
        TRACEVAR(total_am_drops);

        if (total_am_drops > otxn_drops)
        {
            NOPE("CheckoutPaymentHook: Error: Sum of AM amounts exceeds incoming payment amount.");
        }

        // Extract AC parameter (20-byte account ID or 40-byte hex string)
        uint8_t param_ac_raw[PARAM_VALUE_MAX];
        int64_t param_ac_len = otxn_param(SBUF(param_ac_raw), param_name_ac_n[i], sizeof(param_name_ac_n[i]));
        if (param_ac_len == DOESNT_EXIST)
        {
            NOPE("CheckoutPaymentHook: Error: AC parameter not found for corresponding AM.");
        }
        else if (param_ac_len == 20)
        {
            // Direct binary account ID
            for (int j = 0; GUARD(100), j < 20; j++)
                accounts[account_count][j] = param_ac_raw[j];
        }

        // Validate AC is not hook account
        if (BUFFER_EQUAL_20(hook_acc, accounts[account_count]))
        {
            NOPE("CheckoutPaymentHook: Error: AC parameter cannot be hook account.");
        }

        // Check for duplicate accounts
        for (int64_t j = 0; GUARD(5), j < account_count; j++)
        {
            if (BUFFER_EQUAL_20(accounts[j], accounts[account_count]))
            {
                NOPE("CheckoutPaymentHook: Error: Duplicate destination account detected.");
            }
        }

        TRACEHEX(accounts[account_count]);
        account_count++;
    }

    // Ensure at least one pair was found
    if (account_count == 0)
    {
        NOPE("CheckoutPaymentHook: Error: No valid AM/AC parameter pairs found.");
    }

    // Reserve space for emitted transactions
    etxn_reserve(account_count);

    // Emit payment transactions for each account
    for (int64_t i = 0; GUARD(5), i < account_count; i++)
    {
        uint8_t txn[PREPARE_PAYMENT_SIMPLE_SIZE];
        PREPARE_PAYMENT_SIMPLE(txn, am_drops[i], accounts[i], 0, 0);

        uint8_t emithash[32];
        if (emit(SBUF(emithash), SBUF(txn)) != 32)
        {
            NOPE("CheckoutPaymentHook: Error: Failed to emit transaction.");
        }
    }

    // Accept the original transaction
    DONE("CheckoutPaymentHook: Payment forwarded successfully to all accounts.");

    // Prevent reentrancy
    GUARD(1);
    return 0;
}