# SafeGuard Hook 

## Overview

The **SafeGuard Hook** is a powerful and flexible on-chain security and compliance tool for Xahau accounts. It allows the hook owner to enforce payment limits, blacklist accounts, and apply outgoing payment caps, all configurable via on-chain transactions. This hook is ideal for custodial accounts, community treasuries, or any scenario where automated risk controls and compliance are required.

---

## Key Features

- **Minimum Payment Enforcement:** Block incoming payments below a configurable minimum amount.
- **Maximum Payment Enforcement:** Block outgoing payments above a configurable maximum amount.
- **Outgoing Payment Cap:** Prevent outgoing payments that exceed 80% of the account’s balance.
- **Blacklist:** Block payments from or to specific accounts, with on-chain management.
- **Development Contribution:** Automatically emits a small developer contribution payment for each successful payment transaction.
- **On-chain Configuration:** All settings are managed by the hook owner via Invoke transactions.
- **Hierarchical Namespace Storage:** Blacklist entries are stored in scalable, account-specific namespaces.

---

## Configuration Parameters (Invoke Transactions, Owner Only)

| Parameter         | Size   | Description                                           |
|-------------------|--------|-------------------------------------------------------|
| `MIN`             | 1 byte | Enable/disable minimum payment check (0 or 1)         |
| `MINAMT`          | 8 bytes| Set minimum allowed incoming payment (in drops)       |
| `MAX`             | 1 byte | Enable/disable maximum payment check (0 or 1)         |
| `MAXAMT`          | 8 bytes| Set maximum allowed outgoing payment (in drops)       |
| `CAP`             | 1 byte | Enable/disable outgoing payment cap (0 or 1)          |
| `BLACKLIST`       | 1 byte | Enable/disable blacklist enforcement (0 or 1)         |
| `ADD_BLACKLIST`   | 20 bytes| Add account to blacklist (account ID)                |
| `REMOVE_BLACKLIST`| 20 bytes| Remove account from blacklist (account ID)           |

---

## How It Works

### 1. **Configuration (Invoke by Owner Only)**
- The hook owner can enable/disable features and set limits by sending an Invoke transaction with the appropriate parameter(s).
- Blacklist entries can be added or removed by providing the target account ID.

### 2. **Payment Processing**
- For every payment transaction (`ttPAYMENT`):
  - **Blacklist Check:** If enabled, the hook checks if the sender or recipient is blacklisted and blocks the transaction if so.
  - **Minimum Amount Check:** If enabled and the payment is incoming, the hook blocks payments below the configured minimum.
  - **Maximum Amount Check:** If enabled and the payment is outgoing, the hook blocks payments above the configured maximum.
  - **Outgoing Cap Check:** If enabled and the payment is outgoing, the hook blocks payments that are 80% or more of the account’s balance.
  - **Development Contribution:** For every successful payment (after all checks pass), the hook emits a 0.05 XAH developer contribution to a predefined account as a second transaction emission.

### 3. **Blacklist Storage**
- Blacklist status is stored in a namespace unique to each account.
- The presence of a `BLACKLISTED` key with value `0x01` in an account’s namespace means the account is blacklisted.

---

## Example Use Cases

- **Community Treasury:** Prevent accidental or malicious draining of funds.
- **Custodial Wallets:** Enforce compliance and risk controls for user withdrawals.
- **Charity or DAO Accounts:** Block suspicious accounts and enforce spending policies.
- **Personal Security:** Add an extra layer of outgoing payment protection.

---

## Example Configuration Transactions

### Enable Minimum Payment and Set to 10 XAH

```json
{
  "TransactionType": "Invoke",
  "Account": "rHookOwner...",
  "HookParameters": [
    { "HookParameter": 
        { "HookParameterName": "4D494E", 
          "HookParameterValue": "01" 
        }
    }
  ]
}

```
```json
{
  "TransactionType": "Invoke",
  "Account": "rHookOwner...",
  "HookParameters": [
    { "HookParameter": 
        { "HookParameterName": "4D494E414D54",      
          "HookParameterValue": "000000000000000A" 
        }
    } 
  ]
}
```



### Add Account to Blacklist

```json
{
  "TransactionType": "Invoke",
  "Account": "rHookOwner...",
  "HookParameters": [
    { "HookParameter": 
        { "HookParameterName": "4144445F424C41434B4C495354",  // ADD_BLACKLIST
          "HookParameterValue": "AABBCCDDEEFF00112233445566778899AABBCCDD" 
        } 
    }
  ]
}
```

### Remove Account from Blacklist

```json
{
  "TransactionType": "Invoke",
  "Account": "rHookOwner...",
  "HookParameters": [
    { "HookParameter": 
        { "HookParameterName": "52454D4F56455F424C41434B4C495354", // REMOVE_BLACKLIST
          "HookParameterValue": "AABBCCDDEEFF00112233445566778899AABBCCDD" 
        } 
    }
  ]
}
```

---

## Security & Compliance

- **Owner-Only Configuration:** Only the hook owner can change settings or manage the blacklist.
- **On-Chain Transparency:** All settings and blacklist changes are visible on-chain.
- **Automatic Enforcement:** No manual intervention required for payment checks.

---

## Development Contribution

This hook includes a hardcoded development contribution of **0.05 XAH** that is automatically charged for each successful payment operation. This contribution:

- Supports ongoing development and maintenance of the HandyHooks collection
- Is only charged when a payment operation completes successfully (i.e., after all checks pass)
- Is sent to a predetermined development account
- Does not affect your token amounts or reserve calculations

The development contribution is charged as a second transaction emission, alongside the processed payment.

---

## Tools & Resources

- **Xahau Hooks Builder** — https://hooks-builder.xrpl.org/develop
- **Hooks API Reference** — https://xrpl-hooks.readme.io/reference/hook-api-conventions
- **Address → AccountID** — https://hooks.services/tools/raddress-to-accountid
- **Explorer / Logs** — https://xahau-testnet.xrplwin.com/

---

## Technical Notes

- **Namespace Structure:** Blacklist entries are stored in a 32-byte namespace (first 20 bytes = account ID, rest zero).
- **Key Structure:** The key `"BLACKLISTED"` (padded to 32 bytes) is used for blacklist status.
- **Development Contribution:** Each successful payment processed by the hook emits a 0.05 XAH developer contribution to a predefined account, as a second transaction emission.
- **Limits:** Minimum and maximum amounts are set in drops (1 XAH = 1,000,000 drops).

---

## Summary

The SafeGuard Hook is a robust, configurable, and transparent solution for on-chain payment controls and blacklist management. It empowers account owners to enforce security policies and compliance directly at the protocol level, with all actions and settings visible and auditable on-chain.

---
