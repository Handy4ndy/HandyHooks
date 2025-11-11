# Multi Beneficiary Delegate Contract (MBDC) – HandyHooks

A production-ready Xahau hook for automated multi-beneficiary distribution using a delegate-controlled trigger. This contract enables secure distribution of account balances to up to three beneficiaries upon command from a designated delegate.

---

## Overview

- **Delegate Trigger Logic**: Distributes funds to beneficiaries via a delegate invoke command.
- **Multi-Beneficiary Support**: Up to three beneficiary accounts, each with configurable percentage splits.
- **Delegate Control**: Only the designated delegate can trigger distribution.
- **On-Chain Configuration**: All settings managed via invoke transactions.

---

## Features

- **Configurable Beneficiaries**: Set up to three accounts and their respective percentages.
- **Secure Delegate Trigger**: Only the delegate account can initiate the distribution.
- **Pass-Through Design**: Non-matching transactions pass through without blocking the hook chain.
- **State Storage**: Persistent beneficiary and percentage configuration.

---

## Hook Parameters (Set During Installation)

| Parameter  | Size     | Description                                      |
|------------|----------|--------------------------------------------------|
| `DELEGATE` | 20 bytes | Delegate account ID (authorized to trigger SEND) |

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

## Distribution Command (Invoke by Delegate Only)

| Parameter | Size    | Description                                      |
|-----------|---------|--------------------------------------------------|
| `SEND`    | 1 byte  | Trigger distribution to beneficiaries            |

---

## How It Works

1. **Configuration**: Hook owner sets beneficiary accounts and percentages via invoke transactions. Each BAx/BPx pair must be submitted together.
2. **Trigger**: The delegate invokes SEND to distribute the balance.
3. **Distribution**: The contract calculates each beneficiary's share based on configured percentages and emits payments. At least 1% of the balance remains in the hook account.

---

## State Storage

- **Beneficiary Accounts**: Stored as `"BA1"`, `"BA2"`, `"BA3"` (20 bytes each)
- **Percentages**: Stored as `"BP1"`, `"BP2"`, `"BP3"` (4 bytes each, uint32)

---

## Example Usage

### Set Beneficiary 1 (50%) (BA1, BP1)

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

### Set Beneficiary 2 (30%) (BA2, BP2)

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
## Messages

### Success

- `"MBDC:: Success :: Outgoing payment from hook account accepted"` – Outgoing payment processed
- `"MBDC:: Success :: BAx and BPx configured"` – Beneficiary and percentage successfully set
- `"MBDC:: Success :: Balance distributed to beneficiaries"` – Distribution completed
- `"MBDC:: Success :: Transaction passed through"` – Non-matching transaction allowed

### Warnings & Errors

- `"MBDC:: Warning :: Delegate account not set during installation"` – Delegate not configured
- `"MBDC:: Warning :: No valid configuration parameters, submit BAx and BPx pairs"` – Invalid invoke parameters
- `"MBDC:: Warning :: BA2 cannot match BA1"` – Duplicate beneficiary account detected
- `"MBDC:: Warning :: BA3 cannot match BA1 or BA2"` – Duplicate beneficiary account detected
- `"MBDC:: Warning :: BP2 missing for multiple accounts"` – Percentage for second beneficiary not set
- `"MBDC:: Warning :: BP3 missing for multiple accounts"` – Percentage for third beneficiary not set
- `"MBDC:: Warning :: Total beneficiary percentage cannot exceed 100%"` – Percentage allocation exceeds 100%
- `"MBDC:: Error :: BAx and BPx must be submitted together"` – Beneficiary and percentage pair not submitted together
- `"MBDC:: Error :: No beneficiary accounts configured"` – No beneficiaries configured
- `"MBDC:: Error :: Could not load account keylet"` – Failed to load account keylet
- `"MBDC:: Error :: Could not load sfBalance"` – Failed to load account balance
- `"MBDC:: Error :: Insufficient balance"` – Insufficient funds for distribution
- `"MBDC:: Error :: Failed to emit payment to beneficiary"` – Payment emission failed
- `"MBDC:: Error :: Invalid delegate invoke"` – Unauthorized delegate invocation

### Complete

- `"MBDC:: Complete :: This contract appears to have been completed, Farewell Adventurer <3"` – Balance below required reserve

## Security & Compliance

- **Delegate-Only Distribution:** Only the configured delegate can trigger SEND.
- **Pair Submission Enforcement:** BAx/BPx pairs must be submitted together to prevent misconfiguration.
- **Reserve for Fees:** A reserve for transaction fees remains in the hook account.
- **Duplicate Account Prevention:** Duplicate beneficiary accounts are rejected.

## Tools & Resources

- [Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop)
- [Hooks API Reference](https://xrpl-hooks.readme.io/reference/hook-api-conventions)
- [Address → AccountID](https://hooks.services/tools/raddress-to-accountid)
- [Explorer / Logs](https://xahau-testnet.xrplwin.com/)

_Built with ❤️ for the Xahau ecosystem by @Handy_4ndy_