# Single Beneficiary Delegate Contract (SBDC) – HandyHooks

A production-ready Xahau hook for delegate-controlled single-beneficiary distribution. This contract allows a designated delegate to transfer the entire account balance to a beneficiary at any time via invoke.

---

## Overview

- **Delegate Control Logic**: The delegate can invoke at any time to transfer the full balance to the beneficiary.
- **No Threshold**: Distribution can occur immediately upon delegate invoke.
- **On-Chain Configuration**: All settings managed via hook parameters.

---

## Features

- **Delegate-Only Trigger**: Only the configured delegate can initiate the distribution.
- **Pass-Through Design**: Non-matching transactions pass through without blocking the hook chain.
- **Immediate Transfer**: No waiting period required.

---

## Hook Parameters (Set During Installation)

| Parameter    | Size     | Description                                       |
|--------------|----------|---------------------------------------------------|
| `BENEFICIARY`| 20 bytes | Beneficiary account ID (recipient of full balance)|
| `DELEGATE`   | 20 bytes | Delegate account ID (authorized to trigger SEND)  |

---

## Delegate Controls (Invoke by Delegate Only)

| Parameter | Size     | Description                                         |
|-----------|----------|-----------------------------------------------------|
| `SEND`    | 1 byte   | Trigger distribution of full balance to beneficiary |

---

## How It Works

1. **Configuration**: Hook owner sets the beneficiary and delegate via hook parameters.
2. **Delegate Invoke**: The delegate invokes "SEND" to transfer the full balance to the beneficiary.
3. **Distribution**: The contract transfers the entire balance (minus a small reserve for fees) to the beneficiary.

---

## Example Usage

### Set Hook Parameters (Install Time)

| Parameter    | Example Value (Hex)                        | Description            |
|--------------|--------------------------------------------|------------------------|
| `BENEFICIARY`| `AABBCCDDEEFF00112233445566778899AABBCCDD` | Beneficiary Account ID |
| `DELEGATE`   | `BBCCDDEEFF00112233445566778899AABBCCDDEE` | Delegate Account ID    |

### Trigger Distribution (Delegate Only)

```json
{
  "TransactionType": "Invoke",
  "Account": "rDelegateAccount...",
  "Destination": "rHookOwner...",
  "HookParameters": [
    { "HookParameter": { 
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

- `"SBDC:: Success :: Outgoing payment from hook account accepted"` – Outgoing payment passed
- `"SBDC:: Success :: Full balance sent to admin successfully"` – Distribution completed

### Warnings & Errors

- `"SBDC:: Warning :: Beneficiary account not set during installation"` – Beneficiary not configured
- `"SBDC:: Warning :: Delegate account not set during installation"` – Delegate not configured
- `"SBDC:: Error :: Insufficient balance to send (need reserve for fees)"` – Not enough funds
- `"SBDC:: Error :: Failed to emit balance transfer transaction"` – Emit failed

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