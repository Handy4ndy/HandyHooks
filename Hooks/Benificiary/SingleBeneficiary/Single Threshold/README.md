# Single Beneficiary Threshold Contract (SBTC) – HandyHooks

A production-ready Xahau hook for automated single-beneficiary distribution with automatic triggering after inactivity. This contract enables secure, time-based transfer of the entire account balance to a designated beneficiary when the hook account becomes inactive and receives an incoming transaction.

---

## Overview

- **Automatic Trigger Logic**: Transfers the full balance to the beneficiary automatically upon any incoming transaction (payment or invoke) from a non-owner account, but only if the inactivity threshold has been exceeded.
- **Threshold-Based Activation**: Distribution only occurs after a set period of inactivity.
- **Automatic Timer Reset**: Any outgoing transaction from the hook account resets the inactivity timer.
- **On-Chain Configuration**: All settings managed via hook parameters.

---

## Features

- **Threshold-Based Activation**: Distribution only occurs after a set period of inactivity.
- **Automatic Trigger**: No manual invoke needed; triggered by incoming transactions from external accounts.
- **Pass-Through Design**: Non-matching transactions pass through without blocking the hook chain.
- **State Storage**: Persistent last outgoing time.

---

## Hook Parameters (Set During Installation)

| Parameter    | Size     | Description                                       |
|--------------|----------|---------------------------------------------------|
| `BENEFICIARY`| 20 bytes | Beneficiary account ID (recipient of full balance)|
| `THRESHOLD`  | 4 bytes  | Inactivity threshold in seconds (uint32)          |

---

## How It Works

1. **Configuration**: Hook owner sets the beneficiary and threshold via hook parameters.
2. **Timer Reset**: Any outgoing payment or invoke from the hook account resets the inactivity timer.
3. **Threshold Check**: If no outgoing transactions occur for the configured threshold, any incoming transaction from a non-owner account triggers the distribution.
4. **Distribution**: The contract transfers the entire balance (minus a small reserve for fees) to the beneficiary.

---

## State Storage

- **Last Outgoing Time**: Stored as `"LASTCHC"` (4 bytes, uint32 seconds since epoch)

---

## Example Usage

### Set Hook Parameters (Install Time)

| Parameter    | Example Value (Hex)                        | Description            |
|--------------|--------------------------------------------|------------------------|
| `BENEFICIARY`| `AABBCCDDEEFF00112233445566778899AABBCCDD` | Beneficiary Account ID |
| `THRESHOLD`  | `00015180`                                 | 86400 seconds (1 day)  |

### Automatic Trigger

Any incoming payment or invoke from an external account (after threshold exceeded) will automatically forward the balance.

---

## Messages

### Success

- `"SBTC:: Success :: Outgoing payment from hook account accepted, timer reset"` – Timer reset
- `"SBTC:: Success :: Full balance forwarded to beneficiary (threshold exceeded)"` – Distribution completed

### Warnings & Errors

- `"SBTC:: Warning :: Beneficiary account not set during installation"` – Beneficiary not configured
- `"SBTC:: Error :: You must wait NNNNNNNN seconds before triggering."` – Threshold not yet reached
- `"SBTC:: Error :: Insufficient balance to send (need reserve for fees)"` – Not enough funds
- `"SBTC:: Error :: Failed to emit balance transfer transaction"` – Emit failed

### Complete
- `"SBDC:: Complete :: "This contract appears to have been completed, Farewell Adventurer <3"` – Balance below reserve

---

## Security & Compliance

- **Automatic Trigger**: Only activates after threshold and on incoming transactions from non-owners.
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