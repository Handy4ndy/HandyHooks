# Single Beneficiary Contract (SBC) – HandyHooks

A production-ready Xahau hook for automated single-beneficiary distribution using a Dead Man's Switch. This contract enables secure, time-based transfer of the entire account balance to a designated beneficiary if the hook account becomes inactive.

---

## Overview

- **Dead Man's Switch Logic**: Automatically transfers the full balance to the beneficiary if no outgoing transactions occur for a configured threshold period.
- **Delegate Control**: Only the designated delegate can trigger distribution after inactivity.
- **Automatic Timer Reset**: Any outgoing transaction from the hook account resets the inactivity timer.
- **On-Chain Configuration**: All settings managed via invoke transactions.

---

## Features

- **Threshold-Based Activation**: Distribution only occurs after a set period of inactivity.
- **Secure Delegate Trigger**: Only the delegate account can initiate the distribution.
- **Pass-Through Design**: Non-matching transactions pass through without blocking the hook chain.
- **State Storage**: Persistent configuration and last outgoing time.

---

## Hook Parameters (Set During Installation)

| Parameter    | Size     | Description                                       |
|--------------|----------|---------------------------------------------------|
| `BENEFICIARY`| 20 bytes | Beneficiary account ID (recipient of full balance)|
| `DELEGATE`   | 20 bytes | Delegate account ID (authorized to trigger SEND)  |
| `THRESHOLD`  | 4 bytes  | Inactivity threshold in seconds (uint32)          |

---

## Delegate Command (Invoke by Delegate Only)

| Parameter | Size     | Description                                         |
|-----------|----------|-----------------------------------------------------|
| `SEND`    | 1 byte   | Trigger distribution of full balance to beneficiary |

---

## How It Works

1. **Configuration**: Hook owner sets the beneficiary, delegate, and threshold via hook parameters.
2. **Timer Reset**: Any outgoing payment from the hook account resets the inactivity timer.
3. **Threshold Check**: If no outgoing transactions occur for the configured threshold, the delegate can invoke SEND to claim the full balance.
4. **Distribution**: The contract transfers the entire balance (minus a small reserve for fees) to the beneficiary.

---

## State Storage

- **Last Outgoing Time**: Stored as `"LASTCHCK"` (4 bytes, uint32 seconds since epoch)

---

## Example Usage

### Set Hook Parameters (Install Time)

| Parameter    | Example Value (Hex)                        | Description            |
|--------------|--------------------------------------------|------------------------|
| `BENEFICIARY`| `AABBCCDDEEFF00112233445566778899AABBCCDD` | Beneficiary Account ID |
| `DELEGATE`   | `BBCCDDEEFF00112233445566778899AABBCCDDEE` | Delegate Account ID    |
| `THRESHOLD`  | `00015180`                                 | 86400 seconds (1 day)  |

### Trigger Distribution (Delegate Only)

```json
{
  "TransactionType": "Invoke",
  "Account": "rDelegateAccount...",
  "Destination": "rHookOwner...",
  "HookParameters": [
    { 
        "HookParameter": { 
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

- `"SBC:: Success :: Outgoing payment from hook account accepted, timer reset"` – Timer reset
- `"SBC:: Success :: Full balance sent to beneficiary successfully (threshold exceeded)"` – Distribution completed

### Warnings & Errors

- `"SBC:: Warning :: Beneficiary account not set during installation"` – Beneficiary not configured
- `"SBC:: Warning :: Delegate account not set during installation"` – Delegate not configured
- `"SBC:: Error :: You must wait NNNNNNNN seconds before triggering."` – Threshold not yet reached
- `"SBC:: Error :: Insufficient balance to send"` – Not enough funds
- `"SBC:: Error :: Failed to emit balance transfer transaction"` – Emit failed

### Complete
- `"SBDC:: Complete :: "This contract appears to have been completed, Farewell Adventurer <3"` – Balance below reserve

---

## Security & Compliance

- **Delegate-Only Distribution**: Only the configured delegate can trigger SEND.
- **Minimum Reserve**: A small reserve remains in the hook account for fees.
- **Pass-Through Design**: Non-matching transactions do not block the hook chain.

---

## Tools & Resources

- **Xahau Hooks Builder** — https://hooks-builder.xrpl.org/develop
- **Hooks API Reference** — https://xrpl-hooks.readme.io/reference/hook-api-conventions
- **Address → AccountID** — https://hooks.services/tools/raddress-to-accountid
- **Explorer / Logs** — https://xahau-testnet.xrplwin.com/

---

*Built with ❤️ for the Xahau ecosystem by @Handy_4ndy*