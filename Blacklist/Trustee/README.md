# Blacklist Trustee Hook

![Enforcer](https://img.shields.io/badge/Enforcer-Trustee-orange)

> Lightweight enforcement hook that queries a Provider for authoritative blacklist decisions and enforces policy at the account level.

## Abstract

The Blacklist Trustee Hook is the enforcement component of the Provider/Trustee pattern. It consults an authoritative Provider to decide whether transactions should be allowed and performs policy enforcement at the account level. Trustees are deployed on accounts that require compliance controls, automated risk checks, or operational policy enforcement.

## Motivation

While the Provider holds policy, Trustees are responsible for operational enforcement and resilience. This split enables organizations to centralize blacklist policy while distributing enforcement across hot wallets, custodial namespaces, or service accounts. Trustees make it practical to apply a single compliance policy consistently across many operational accounts without duplicating policy logic.

### Example Compliance & Operational Use-Cases

- Exchange Enforcement: Trustees on hot/cold wallets prevent withdrawals from accounts flagged by the Provider.
- Custodial Controls: Trustees on custodial accounts ensure outgoing transfers comply with governance rules.
- Compliance Holds: Trustees reject or hold transactions while off-ledger KYC/AML checks complete.
- Incident Response: Trustees immediately block transactions from accounts flagged after security incidents.
- Fee-based Enforcement: Trustees can optionally collect small service fees for processing enforcement logic and logging.

## Overview

Trustees enforce blacklist decisions by querying a Provider hook. The Trustee remains lightweight — no per-account blacklist state is needed except an optional local cache for emergency toggles.

## Key Features

- Queries a central Provider via `state_foreign()`
- Simple two-parameter configuration: a 1-byte `BLACKLIST` flag and a 20-byte `PROVIDER` account
- Fast enforcement for PAYMENT transactions
- Optional service fee emission after a payment is allowed (requires native balance)

## Hook Parameters / Admin Commands

Trustee administrative commands (send as Invoke from hook owner):

| Command | Size | Description |
|---------|------|-------------|
| `PROVIDER` | 20 bytes | Set provider account ID to query |
| `BLACKLIST` | 1 byte  | Enable (1) or disable (0) blacklist checking on this trustee |

Hex HookParameter names used in examples:

- `PROVIDER` -> `50524F5649444552`
- `BLACKLIST` -> `424C41434B4C495354`

## Storage Keys

- `PROVIDER` — 20-byte provider account ID stored in local hook state
- `BLKLST` — 1-byte local cached flag (`0x01`) used by the trustee to enable/disable blacklist checking

## Installation Example

```json
{
  "Account": "rTrusteeAccount...", 
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

Set provider account (invoke from hook owner):

```json
{ 
  "TransactionType": "Invoke", 
  "Account": "rTrusteeOwner...", 
  "HookParameters": [
    {
      "HookParameter": {
        "HookParameterName": "50524F5649444552",  
        "HookParameterValue": "58BF50228721CF7E6B3AE45ACEFEEB71974CA571" 
      }
    }
  ] 
}
```

Toggle blacklist checking on/off (invoke from hook owner):

```json
{ 
  "TransactionType": "Invoke", 
  "Account": "rTrusteeOwner...", 
  "HookParameters": [
    {
      "HookParameter": {
        "HookParameterName": "424C41434B4C495354",  
        "HookParameterValue": "01"  // 01 = enable, 00 = disable
      }
    }
  ] 
}
```

## Behavior & Transaction Flow

1. Trustee reads `PROVIDER` from local state to determine which provider to query.
2. On PAYMENT transactions, Trustee builds a 32-byte namespace for the payment source and calls `state_foreign()` against the provider account (only if the local `BLKLST` flag is enabled).
3. If provider returns a single byte `0x01`, Trustee rejects the payment (rollback).
4. If provider returns no entry or a non-`0x01` value, the payment proceeds; the trustee may then emit a small service fee for the processed payment.

## Integration (How Trustee queries Provider)

Example C flow used in `blacklistTrustee.c`:

```c
uint8_t provider_acc[20]; // loaded from local PROVIDER key
uint8_t account_namespace[32]; // source account id padded to 32 bytes
uint8_t blacklist_key[32] = "BLACKLISTED"; // padded
uint8_t status[1];
int64_t got = state_foreign(SBUF(status), SBUF(blacklist_key), SBUF(account_namespace), SBUF(provider_acc));
if (got == 1 && status[0] == 0x01) {
  // Reject payment (rollback)
} else {
  // Payment allowed — trustee may emit service fee after successful processing
}
```

## Service Fee

- Trustee may reserve and emit a small native fee using `etxn_reserve()`/`emit()` for successful payments that have passed the provider blacklist check. Service fees are collected only after the transaction has been allowed — not when a payment is rejected due to blacklist status. 

- Ensure trustee accounts maintain an adequate native balance to fund fee emissions.

## Pass-Through & Messages

- `"Payment transaction blocked: Account is blacklisted"`
- `"Payment allowed"`

## Error & Debugging

- Use `TRACESTR()` and `TRACEVAR()`; check explorer traces for etxn results.
- Common errors: missing PROVIDER configuration, insufficient native balance for etxn, failed state_foreign() calls.

## Tools & Resources

- **Xahau Hooks Builder** — https://hooks-builder.xrpl.org/develop
- **Hooks API Reference** — https://xrpl-hooks.readme.io/reference/hook-api-conventions
- **Address → AccountID** — https://hooks.services/tools/raddress-to-accountid
- **Explorer / Logs** — https://xahau-testnet.xrplwin.com/

## Notes & Recommendations

- Point all trustees to a single provider for centralized policy, or to distinct providers for segmented policy domains.
- Ensure trustees have a small native reserve for fee emissions if using service fees.

## Acknowledgments

- Special thanks to @AverageJohnXRP for inspiring the Provider/Trustee design used in this project.

---

*Built with ❤️ for the Xahau ecosystem by @Handy_4ndy*

> **Disclaimer**: Trustee operators are responsible for ensuring they have native balance and permissions to emit service fee transactions. This tool is provided for compliance and security purposes. Users are responsible for ensuring compliance with applicable laws and regulations in their jurisdiction.

