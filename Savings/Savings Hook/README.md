# Incoming Payment Savings Hook (IPS) – Xahau HandyHook Collection

## Overview

The **Incoming Payment Savings Hook** is an automated savings tool for Xahau accounts that automatically forwards a configurable percentage of incoming XAH payments to up to three designated savings accounts. It ensures at least 1% of the payment remains in the hook account to fund future operations, providing a simple yet effective way to enforce savings discipline on incoming funds.

---

## Key Features

- **Automatic Savings Forwarding:** Forwards a percentage of incoming XAH payments to configured savings accounts.
- **Multiple Accounts Support:** Supports up to 3 savings accounts with individual percentage splits.
- **Percentage Limits:** Total forwarding percentage capped at 99% to ensure operational funds remain.
- **Admin-Only Configuration:** Only the hook owner can configure savings accounts and percentages via Invoke transactions.
- **Outgoing Payment Pass-Through:** Accepts all outgoing payments without modification.
- **On-Chain Configuration:** All settings managed transparently via Invoke transactions.

---

## Configuration Parameters (Invoke Transactions, Owner Only)

| Parameter | Size     | Description                                      |
|-----------|----------|--------------------------------------------------|
| `SA1`     | 20 bytes | Set Savings Account 1 (Account ID)               |
| `SP1`     | 4 bytes  | Set Savings Percentage 1 (1-99, uint32)          |
| `SA2`     | 20 bytes | Set Savings Account 2 (Account ID)               |
| `SP2`     | 4 bytes  | Set Savings Percentage 2 (1-99, uint32)          |
| `SA3`     | 20 bytes | Set Savings Account 3 (Account ID)               |
| `SP3`     | 4 bytes  | Set Savings Percentage 3 (1-99, uint32)          |

---

## How It Works

### 1. Configuration (Invoke by Owner Only)

The hook owner sends an Invoke transaction with parameters to set savings accounts (`SA1`, `SA2`, `SA3`) and their corresponding percentages (`SP1`, `SP2`, `SP3`).

- Percentages must be between 1-99
- The total across all configured accounts cannot exceed 99% (preserving at least 1% in the hook account)
- Duplicate accounts across slots are rejected

### 2. Payment Processing

For incoming XAH payments (`ttPAYMENT`):

- Retrieves configured savings accounts and percentages from hook state
- Calculates the forwarding amounts for each savings account based on configured percentages using XFL (Xahau Float) arithmetic
- Emits separate payment transactions to each configured savings account
- Ensures at least 1% of the original payment remains in the hook account
- Outgoing payments (from hook owner) are accepted without modification
- Non-XAH payments pass through unchanged

### 3. State Storage

- Savings accounts are stored in hook state using keys: `SA1_KEY`, `SA2_KEY`, `SA3_KEY` (20 bytes each)
- Savings percentages are stored using keys: `SP1_KEY`, `SP2_KEY`, `SP3_KEY` (4 bytes each, uint32)
- Configuration is persistent and account-specific

---

## Example Use Cases

- **Personal Savings Automation:** Automatically save a portion of every incoming payment into dedicated savings accounts
- **Family Budgeting:** Parents can configure child accounts to forward allowances to savings
- **Business Revenue Splitting:** Distribute a percentage of incoming revenue to reserve or investment accounts
- **Charitable Giving:** Automatically forward a portion of donations to savings for future use
- **Emergency Fund Building:** Automatically accumulate savings across multiple beneficiary accounts

---

## Example Configuration Transactions

### Set Savings Account 1 and Percentage (50%)

```json
{
  "TransactionType": "Invoke",
  "Account": "rHookOwner...",
  "Destination": "rHookOwner...",
  "HookParameters": [
    {
      "HookParameter": {
        "HookParameterName": "534131",
        "HookParameterValue": "AABBCCDDEEFF00112233445566778899AABBCCDD"
      }
    }
  ]
}
```

```json
{
  "TransactionType": "Invoke",
  "Account": "rHookOwner...",
  "Destination": "rHookOwner...",
  "HookParameters": [
    {
      "HookParameter": {
        "HookParameterName": "535031",
        "HookParameterValue": "00000032"
      }
    }
  ]
}
```

### Set Savings Account 2 and Percentage (30%)

```json
{
  "TransactionType": "Invoke",
  "Account": "rHookOwner...",
  "Destination": "rHookOwner...",
  "HookParameters": [
    {
      "HookParameter": {
        "HookParameterName": "534132",
        "HookParameterValue": "BBCCDDEEFF00112233445566778899AABBCCDDEE"
      }
    }
  ]
}
```

```json
{
  "TransactionType": "Invoke",
  "Account": "rHookOwner...",
  "Destination": "rHookOwner...",
  "HookParameters": [
   {
      "HookParameter": {
        "HookParameterName": "535032",
        "HookParameterValue": "0000001E"
      }
    }
  ]
}

```

### Third account not set 20% remains in hook account.

---

## Security & Compliance

- **Owner-Only Configuration:** Only the hook owner can modify savings settings
- **Percentage Validation:** Ensures total percentages do not exceed 99% and individual percentages are between 1-99
- **Duplicate Account Prevention:** Prevents configuring the same account for multiple slots to avoid confusion
- **Minimum Reserve Enforcement:** Always preserves at least 1% of incoming payments to fund future hook operations
- **On-Chain Transparency:** All configurations and forwarding operations are visible on-chain

---

## Technical Notes

- **State Keys:** Uses 64-bit keys for storing accounts and percentages (e.g., `SA1_KEY = 0x5341310000000000ULL`, `SP1_KEY = 0x5350310000000000ULL`)
- **Amount Handling:** Uses XFL (Xahau Float) for precise percentage calculations and conversions between percentages and native drops
- **Emission Reserve:** Reserves emission slots equal to the number of configured savings accounts (up to 3)
- **Percentage Limits:** Individual percentages 1-99; total ≤ 99%; minimum remaining 1%
- **Float Arithmetic:** All percentage-based calculations use XFL functions (`float_divide`, `float_multiply`, `float_sum`, etc.) for precision

---

## Summary

The Incoming Payment Savings Hook provides a straightforward, automated way to enforce savings discipline on incoming XAH payments, with flexible configuration for multiple accounts and percentages. It supports financial planning and fund management while preserving operational resources for the hook account itself. The hook is ideal for personal finance automation, family budgeting, business revenue management, and charitable giving scenarios.

---

## Tools & Resources

- **Xahau Hooks Builder** — https://hooks-builder.xrpl.org/develop
- **Hooks API Reference** — https://xrpl-hooks.readme.io/reference/hook-api-conventions
- **Address → AccountID** — https://hooks.services/tools/raddress-to-accountid
- **Explorer / Logs** — https://xahau-testnet.xrplwin.com/

---

*Built with ❤️ for the Xahau ecosystem by @Handy_4ndy*