# Multi Beneficiary Contract (MBC) – HandyHooks

A production-ready Xahau hook for automated multi-beneficiary distribution using a Dead Man's Switch. This contract enables secure, time-based distribution of account balances to up to three beneficiaries if the hook account becomes inactive.

---

## Overview

- **Dead Man's Switch Logic**: Distributes funds to beneficiaries with a delegate command, if no outgoing transactions occur for a configured threshold period.
- **Multi-Beneficiary Support**: Up to three beneficiary accounts, each with configurable percentage splits.
- **Delegate Control**: Only the designated delegate can trigger distribution after inactivity.
- **Automatic Timer Reset**: Any outgoing transaction from the hook account resets the inactivity timer.
- **On-Chain Configuration**: All settings managed via invoke transactions.

---

## Features

- **Threshold-Based Activation**: Distribution only occurs after a set period of inactivity.
- **Configurable Beneficiaries**: Set up to three accounts and their respective percentages.
- **Secure Delegate Trigger**: Only the delegate account can initiate the distribution.
- **Pass-Through Design**: Non-matching transactions pass through without blocking the hook chain.
- **State Storage**: Persistent beneficiary and percentage configuration.

---

## Hook Parameters (Set During Installation)

| Parameter   | Size     | Description                                      |
|-------------|----------|--------------------------------------------------|
| `DELEGATE`  | 20 bytes | Delegate account ID (authorized to trigger SEND) |
| `THRESHOLD` | 4 bytes  | Inactivity threshold in seconds (uint32)         |

---

## Hook Owner Configuration Commands (Invoke by Hook Owner Only)

| Parameter | Size     | Description                                      |
|-----------|----------|--------------------------------------------------|
| `BA1`     | 20 bytes | Set Beneficiary Account 1                        |
| `BP1`     | 4 bytes  | Set Beneficiary Percentage 1 (1-100, uint32)     |
| `BA2`     | 20 bytes | Set Beneficiary Account 2                        |
| `BP2`     | 4 bytes  | Set Beneficiary Percentage 2 (1-100, uint32)     |
| `BA3`     | 20 bytes | Set Beneficiary Account 3                        |
| `BP3`     | 4 bytes  | Set Beneficiary Percentage 3 (1-100, uint32)     |

> **Note:** Each BAx/BPx pair must be submitted together in a single invoke transaction. Duplicate accounts are rejected, and total percentages cannot exceed 100%.

---

## Distribution Command (Invoke by Delegate Only)

| Parameter | Size     | Description                                      |
|-----------|----------|--------------------------------------------------|
| `SEND`    | 1 byte   | Trigger distribution to beneficiaries            |

---

## How It Works

1. **Configuration**: Hook owner sets beneficiary accounts and percentages via invoke transactions. Each BAx/BPx pair must be submitted together. Percentages are capped at 100% individually, and totals are validated to not exceed 100%.
2. **Timer Reset**: Any outgoing payment from the hook account resets the inactivity timer.
3. **Threshold Check**: If no outgoing transactions occur for the configured threshold, the delegate can invoke SEND to distribute the balance.
4. **Distribution**: The contract calculates each beneficiary's share based on configured percentages and emits payments. A reserve for transaction fees remains in the hook account.

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
      { "HookParameterName": "425031", 
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

### Trigger Distribution (Delegate Only)

```json
{
  "TransactionType": "Invoke",
  "Account": "rDelegateAccount...",
  "Destination": "rHookOwner...",
  "HookParameters": [
    { "HookParameter": 
      { 
        "HookParameterName": "53454E44", 
        "HookParameterValue": "01" 
      } 
    }
  ]
}
```

---

## Messages

### Success

- `"MBC:: Success :: BAx and BPx configured"` – Beneficiary and percentage set
- `"MBC:: Success :: Balance distributed to beneficiaries (threshold exceeded)"` – Distribution completed

### Warnings & Errors

- `"MBC:: Error :: BAx and BPx must be submitted together"` – Pair not submitted together
- `"MBC:: Error :: No beneficiary accounts configured"` – No beneficiaries set
- `"MBC:: Error :: Insufficient balance to send (need reserve for fees)"` – Not enough funds
- `"MBC:: Error :: You must wait NNNNNNNN seconds before triggering."` – Threshold not yet reached

---

## Security & Compliance

- **Admin-Only Distribution**: Only the configured admin can trigger SEND.
- **Pair Submission Enforcement**: BAx/BPx pairs must be submitted together to prevent misconfiguration.
- **Minimum Reserve**: At least 1% of the balance remains in the hook account.
- **Duplicate Account Prevention**: Duplicate beneficiary accounts are rejected.

---

## Tools & Resources

- **Xahau Hooks Builder** — https://hooks-builder.xrpl.org/develop
- **Hooks API Reference** — https://xrpl-hooks.readme.io/reference/hook-api-conventions
- **Address → AccountID** — https://hooks.services/tools/raddress-to-accountid
- **Explorer / Logs** — https://xahau-testnet.xrplwin.com/

---

*Built with ❤️ for the Xahau ecosystem by @Handy_4ndy*