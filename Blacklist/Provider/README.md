# Blacklist Provider Hook - Xahau HandyHook Collection

![Compliance](https://img.shields.io/badge/Compliance-Ready-green)
![Security](https://img.shields.io/badge/Security-Enhanced-blue)
![Enterprise](https://img.shields.io/badge/Enterprise-Grade-purple)

> Centralized blacklist storage and query provider for the Xahau Hooks ecosystem.

## Abstract

The Blacklist Provider Hook is a purpose-built compliance service for Xahau deployments. It provides a single authoritative, on-ledger registry of accounts that are restricted from sending or receiving funds for regulatory, risk, or operational reasons. Consumer hooks (Trustees) query the Provider to determine whether a transaction should be allowed, enabling consistent, auditable enforcement across many accounts and services.

## Motivation

Organizations operating on Xahau — exchanges, custodians, financial institutions, and regulated businesses — need reliable mechanisms to enforce sanctions, respond to regulatory takedowns, and implement internal risk controls. The Provider/Trustee pattern separates policy (Provider) from enforcement (Trustee), allowing:

- Centralized policy management with distributed enforcement points
- Clear audit trails for regulatory review (state changes are recorded on-ledger)
- Reduced operational overhead compared to per-account lists maintained off-ledger
- Rapid propagation of policy changes to all trustees without redeploying enforcement logic

### Example Compliance Use-Cases

- Sanctions & Watchlists: Maintain OFAC-style or internal watchlists and ensure payments from listed accounts are blocked.
- Regulatory Holds: Implement temporary holds while KYC/AML reviews complete.
- Exchange Risk Controls: Prevent hot-wallets or flagged accounts from participating in withdrawals or trading.
- Custodial Controls: Block outgoing transfers from accounts flagged by internal governance.
- Incident Response: Quickly add accounts involved in fraud or security incidents to prevent further loss.
- Audit & Reporting: Provide a single on-ledger source of truth for compliance reporting and forensic analysis.

## Overview

The Provider stores per-account blacklist flags in account-specific namespaces so consumer hooks can query a single authoritative source.

## Key Features

- Owner-only administrative commands (Invoke)
- Per-account namespace storage for unlimited scalability
- Simple API: ADD_BLACKLIST / REMOVE_BLACKLIST / CHECK_BLACKLIST
- Fast cross-hook queries via `state_foreign()`

## Hook Parameters / Admin Commands

Provider administrative commands (send as Invoke from hook owner):

| Command | Size | Description |
|---------|------|-------------|
| `ADD_BLACKLIST` | 20 bytes | Add account ID to blacklist |
| `REMOVE_BLACKLIST` | 20 bytes | Remove account ID from blacklist |
| `CHECK_BLACKLIST` | 20 bytes | Query blacklist status of account |

Hex HookParameter names used in examples:

- `ADD_BLACKLIST` -> `4144445F424C41434B4C495354`
- `REMOVE_BLACKLIST` -> `52454D4F56455F424C41434B4C495354`
- `CHECK_BLACKLIST` -> `434845434B5F424C41434B4C495354`

## Storage Keys

- Namespace: 32 bytes = 20-byte account ID + 12 zero bytes
- Key: `BLACKLISTED` padded to 32 bytes
- Value: single byte `0x01` = blacklisted; absence = not blacklisted

Internal key helpers in code: `blacklist_key_data[] = "BLACKLISTED"` (padded)

## Installation Example

```json
{
  "Account": "rBlacklistProviderAccount...",
  "TransactionType": "SetHook",
  "Hooks": [
    {
      "Hook": {
        "CreateCode": "0061736D01000000...",
        "Flags": 1,
        "HookApiVersion": 0,
        "HookNamespace": "4FF9961269BF7630D32E15276569C94470174A5DA79FA567C0F62251AA9A36B9",
        "HookOn": "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFFFFFFFFFFFFFBFFFFE"
      }
    }
  ]
}
```

## Invocation Examples

Add an account to the blacklist (invoke from hook owner):

```json
{ 
  "TransactionType": "Invoke", 
  "Account": "rHookOwner...", 
  "HookParameters": [
    {
      "HookParameter": {
        "HookParameterName": "4144445F424C41434B4C495354",  // ADD_BLACKLIST
        "HookParameterValue": "58BF50228721CF7E6B3AE45ACEFEEB71974CA571" 
      }
    }
  ] 
}
```

Remove an account from the Blacklist (invoke from hook owner): 

```json
{ 
  "TransactionType": "Invoke", 
  "Account": "rHookOwner...", 
  "HookParameters": [
    {
      "HookParameter": {
        "HookParameterName": "52454D4F56455F424C41434B4C495354",  // REMOVE_BLACKLIST
        "HookParameterValue": "58BF50228721CF7E6B3AE45ACEFEEB71974CA571" 
      }
    }
  ] 
}
```

Check an account's status (invoke from hook owner):

```json
{ 
  "TransactionType": "Invoke", 
  "Account": "rHookOwner...", 
  "HookParameters": [
    {
      "HookParameter": {
        "HookParameterName": "434845434B5F424C41434B4C495354", // CHECK_BLACKLIST
        "HookParameterValue": "58BF50228721CF7E6B3AE45ACEFEEB71974CA571"
      }
    }
  ] 
}
```

## Behavior & Transaction Flow

1. Administrative invokes (ttINVOKE) must originate from hook owner. Provider sets/removes per-account state via `state_foreign_set()`.
2. For payments and other transactions, consumer hooks query the provider via `state_foreign()` (provider itself is passive for payments).

## Integration (How to query the provider)

Consumer hooks should build a 32-byte namespace (20-byte account ID + 12 zeros), prepare the `BLACKLISTED` key padded to 32 bytes, then call:

```c
uint8_t blacklist_status[1];
int64_t res = state_foreign(SBUF(blacklist_status), SBUF(blacklist_key_data), 
                            SBUF(account_namespace), SBUF(blacklist_hook_account));

if (res == 1 && blacklist_status[0] == 0x01) {
    // Account is blacklisted
}
```

## Pass-Through & Messages

- `"Account added to blacklist successfully"`
- `"Account removed from blacklist successfully"`
- `"Account is blacklisted"` / `"Account is not blacklisted"`

## Error & Debugging

- Trace helpers: `TRACESTR()` and `TRACEVAR()` are used in the code for diagnostics
- Common provider errors: failed state writes or missing parameters

## Tools & Resources

- **Xahau Hooks Builder** — https://hooks-builder.xrpl.org/develop
- **Hooks API Reference** — https://xrpl-hooks.readme.io/reference/hook-api-conventions
- **Address → AccountID** — https://hooks.services/tools/raddress-to-accountid
- **Explorer / Logs** — https://xahau-testnet.xrplwin.com/

## Notes & Recommendations

- Keep provider admin keys tightly controlled.
- Use provider-only administrative channels and audit all invoke transactions.

## Acknowledgments

Special thanks to @AverageJohnXRP for inspiring the Provider/Trustee design used in this project.

---

*Built with ❤️ for the Xahau ecosystem by @Handy_4ndy*

> **Disclaimer**: This tool is provided for compliance and security purposes. Users are responsible for ensuring compliance with applicable laws and regulations in their jurisdiction.