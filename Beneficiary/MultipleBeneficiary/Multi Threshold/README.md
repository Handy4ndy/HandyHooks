# Multi Beneficiary Threshold Contract (MBTC) – HandyHooks

A production-ready Xahau hook for automated multi-beneficiary distribution with automatic triggering after inactivity. This contract enables secure, time-based transfer of the account balance to up to three beneficiaries when the hook account becomes inactive and receives an incoming transaction.

---

## Overview

- **Automatic Trigger Logic**: Transfers the balance to configured beneficiaries automatically upon any incoming transaction (payment or invoke) from a non-owner account, but only if the inactivity threshold has been exceeded.
- **Threshold-Based Activation**: Distribution only occurs after a set period of inactivity.
- **Multi-Beneficiary Support**: Up to three beneficiary accounts, each with configurable percentage splits.
- **Automatic Timer Reset**: Any outgoing transaction from the hook account resets the inactivity timer.
- **On-Chain Configuration**: All settings managed via invoke transactions.

---

## Features

- **Threshold-Based Activation**: Distribution only occurs after a set period of inactivity.
- **Automatic Trigger**: No manual invoke needed; triggered by incoming transactions from external accounts.
- **Configurable Beneficiaries**: Set up to three accounts and their respective percentages.
- **Pass-Through Design**: Non-matching transactions pass through without blocking the hook chain.
- **State Storage**: Persistent beneficiary and percentage configuration.

---

## Hook Parameters (Set During Installation)

| Parameter   | Size     | Description                                      |
|-------------|----------|--------------------------------------------------|
| `THRESHOLD` | 4 bytes  | Inactivity threshold in seconds (uint32)         |

---

## Hook Owner Configuration Commands (Invoke by Hook Owner Only)

| Parameter | Size     | Description                                      |
|-----------|----------|--------------------------------------------------|
| `BA1`     | 20 bytes | Set Beneficiary Account 1                        |
| `BP1`     | 4 bytes  | Set Beneficiary Percentage 1 (1-99, uint32)      |
| `BA2`     | 20 bytes | Set Beneficiary Account 2                        |
| `BP2`     | 4 bytes  | Set Beneficiary Percentage 2 (1-99, uint32)      |
| `BA3`     | 20 bytes | Set Beneficiary Account 3                        |
| `BP3`     | 4 bytes  | Set Beneficiary Percentage 3 (1-99, uint32)      |

> **Note:** Each BAx/BPx pair must be submitted together in a single invoke transaction.

---

## How It Works

1. **Configuration**: Hook owner sets beneficiary accounts and percentages via invoke transactions. Each BAx/BPx pair must be submitted together.
2. **Timer Reset**: Any outgoing payment or invoke from the hook account resets the inactivity timer.
3. **Threshold Check**: If no outgoing transactions occur for the configured threshold, any incoming transaction from a non-owner account triggers the distribution.
4. **Distribution**: The contract calculates each beneficiary's share based on configured percentages and emits payments. At least 1% of the balance remains in the hook account.

---

## State Storage

- **Beneficiary Accounts**: Stored as `"BA1"`, `"BA2"`, `"BA3"` (20 bytes each)
- **Percentages**: Stored as `"BP1"`, `"BP2"`, `"BP3"` (4 bytes each, uint32)
- **Last Outgoing Time**: Stored as `"LASTCHC"` (4 bytes, uint32 seconds since epoch)

---

## Example Usage

### Set Beneficiary 1 (50%)

```json
{
  "TransactionType": "Invoke",
  "Account": "rHookOwner...",
  "Destination": "rHookOwner...",
  "HookParameters": [
    { "HookParameter": 
      { 
        "HookParameterName": "424131", 
        "HookParameterValue": "AABBCCDDEEFF00112233445566778899AABBCCDD" 
      } 
    },
    { "HookParameter": 
      { 
        "HookParameterName": "425031", 
        "HookParameterValue": "00000032" 
      }
    }
  ]
}
```

### Set Beneficiary 2 (30%)

```json
{
  "TransactionType": "Invoke",
  "Account": "rHookOwner...",
  "Destination": "rHookOwner...",
  "HookParameters": [
    { "HookParameter": 
      { 
        "HookParameterName": "424132", 
        "HookParameterValue": "BBCCDDEEFF00112233445566778899AABBCCDDEE" 
      } 
    },
    { "HookParameter": 
      { 
        "HookParameterName": "425032", 
        "HookParameterValue": "0000001E" 
      } 
    }
  ]
}
```

## Automatic Trigger

Any incoming payment or invoke from an external account (after threshold exceeded) will automatically forward the balance.

## Messages

- **Success**
### Messages

#### Success

- `"MBTC:: Success :: Outgoing payment from hook account accepted, timer reset"` – Timer reset
- `"MBTC:: Success :: BAx and BPx configured"` – Beneficiary and percentage set
- `"MBTC:: Success :: Balance distributed to beneficiaries (threshold exceeded)"` – Distribution completed
- `"MBTC:: Success :: Transaction passed through"` – Non-matching transaction allowed

#### Warnings & Errors

- `"MBTC:: Warning :: No valid configuration parameters, submit BAx and BPx pairs"` – Invalid invoke
- `"MBTC:: Warning :: BA2 cannot match BA1"` – Duplicate account
- `"MBTC:: Warning :: BA3 cannot match BA1 or BA2"` – Duplicate account
- `"MBTC:: Warning :: BP2 missing for multiple accounts"` – Percentage not set
- `"MBTC:: Warning :: BP3 missing for multiple accounts"` – Percentage not set
- `"MBTC:: Warning :: Total beneficiary percentage cannot exceed 100%"` – Over 100%
- `"MBTC:: Error :: BAx and BPx must be submitted together"` – Pair not submitted together
- `"MBTC:: Error :: You must wait NNNNNNNN seconds before triggering."` – Threshold not yet reached
- `"MBTC:: Error :: No beneficiary accounts configured"` – No beneficiaries set
- `"MBTC:: Error :: Could not load account keylet"` – Balance load failure
- `"MBTC:: Error :: Could not load sfBalance"` – Balance load failure
- `"MBTC:: Error :: Insufficient balance"` – Not enough funds
- `"MBTC:: Error :: Failed to emit payment to beneficiary"` – Emit failed

#### Complete

- `"MBTC:: Complete :: This contract appears to have been completed, Farewell Adventurer <3"` – Balance below reserve

---

### Security & Compliance

- **Automatic Trigger**: Only activates after threshold and on incoming transactions from non-owners.
- **Pair Submission Enforcement**: BAx/BPx pairs must be submitted together to prevent misconfiguration.
- **Minimum Reserve**: At least 1% of the balance remains in the hook account.
- **Duplicate Account Prevention**: Duplicate beneficiary accounts are rejected.

---

### Tools & Resources

- [Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop)
- [Hooks API Reference](https://xrpl-hooks.readme.io/reference/hook-api-conventions)
- [Address → AccountID](https://hooks.services/tools/raddress-to-accountid)
- [Explorer / Logs](https://xahau-testnet.xrplwin.com/)

_Built with ❤️ for the Xahau ecosystem by @Handy_4ndy_
