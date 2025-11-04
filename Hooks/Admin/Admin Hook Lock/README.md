# Admin Hook Locker – Detailed Explanation

## Overview

The **Admin Hook Locker** is a critical security tool for Xahau accounts that prevents unauthorized or accidental modifications to your hook configuration. It allows a designated admin account to toggle a lock that blocks all `SetHook` transactions, protecting your hook chain from unwanted changes while allowing all other transaction types to flow through normally. This hook is ideal for accounts running production hooks, DAOs, community treasuries, or any scenario where hook configuration stability is essential.

---

## ⚠️ CRITICAL WARNING: PERMANENT LOCK RISK

**When the Admin Hook Locker is ENABLED, all `SetHook` transactions are permanently blocked until the lock is explicitly disabled by the admin account.**

### Important Considerations:

- **No Hook Modifications:** While locked, you **cannot** add, remove, or modify any hooks on your account—including disabling the Admin Hook Locker itself.
- **Lock Requires Admin Access:** Only the designated admin account can disable the lock. If the admin account becomes inaccessible or compromised, your hooks are frozen indefinitely.
- **Permanent Until Disabled:** The lock persists across all transactions and hook cycles until an explicit toggle from the admin account disables it.
- **Multi-Hop Consequence:** If using multiple hooks, enabling this lock freezes your entire hook chain—no modifications possible until disabled.
- **Recovery Dependency:** There is **no recovery mechanism** if the admin account is lost or unavailable. Plan accordingly and maintain secure access to your admin account.

### Best Practices:

1. **Test in Testnet First:** Always test the toggle mechanism on testnet before enabling in production.
2. **Secure Admin Account:** Use a multi-signature address, hardware wallet, or secure key management for the admin account.
3. **Document Admin Identity:** Keep clear records of which account is your admin to avoid losing access.
4. **Plan Unlock Strategy:** Have a documented process for disabling the lock in case of emergency.
5. **Communicate Changes:** If using a shared or DAO account, communicate lock status to all stakeholders.

---

## Key Features

- **SetHook Blocking:** Prevent all hook configuration changes when the lock is enabled.
- **Admin-Only Control:** Only the designated admin account can toggle the lock on/off.
- **Non-Admin Rejection:** Invoke transactions from non-admin accounts are blocked.
- **Transaction Passthrough:** All other transaction types (payments, transfers, etc.) pass through unaffected.
- **On-chain Configuration:** Admin account is set during hook installation and managed at the protocol level.
- **Simple Toggle Mechanism:** Enable/disable the lock with a single Invoke parameter.
- **Persistent State:** Lock state is stored on-chain and survives hook updates.
- **Early Exit Optimization:** Non-relevant transactions pass through immediately, saving resources.

---

## Configuration Parameters (Installation Only)

| Parameter | Size   | Description                                      |
|-----------|--------|--------------------------------------------------|
| `ADMIN`   | 20 bytes| Admin account ID with exclusive toggle control  |

---

## Admin Controls (Invoke Transactions, Admin Only)

| Parameter | Size   | Description                                      |
|-----------|--------|--------------------------------------------------|
| `LOCK`    | 1 byte | Any value to toggle lock on/off (1 = enable, 0 = disable) |

---

## How It Works

### 1. **Installation & Setup**
- The hook owner installs the Admin Hook Locker and specifies the `ADMIN` parameter (20-byte account ID) during installation.
- This admin account is granted exclusive control over the lock state.

### 2. **Admin Toggle Control (Invoke by Admin Only)**
- The admin account sends an Invoke transaction with the `LOCK` parameter to toggle the lock state.
- The lock state is toggled between enabled (blocks SetHook) and disabled (allows SetHook).
- Non-admin Invoke attempts are rejected with an error.

### 3. **Transaction Processing**
- **For `ttINVOKE` transactions:**
  - If sender is admin and `LOCK` parameter is present: Toggle lock state and accept.
  - If sender is not admin: Reject the transaction.
  - If `LOCK` parameter is not present: Accept and pass through.
  
- **For `ttHOOK_SET` transactions:**
  - If lock is enabled: Reject the transaction (blocks hook modifications).
  - If lock is disabled: Accept the transaction (allows hook modifications).
  
- **For all other transaction types:** Accept and pass through immediately (payments, transfers, etc.) — early exit for resource optimization.

### 4. **Lock State Storage**
- Lock state is stored in a single on-chain key: `SETHOOK_LOCK_KEY` (`0x5345544800000000` = "SETH").
- State value `0x01` = Lock enabled (SetHook blocked).
- State value `0x00` or unset = Lock disabled (SetHook allowed).

---

## Example Use Cases

- **Production Hook Protection:** Prevent accidental hook modifications on live accounts.
- **Multi-Signature Schemes:** Admin account is a multisig address, requiring group consensus to unlock.
- **DAO/Treasury Accounts:** Protect community hooks from unauthorized changes.
- **Temporary Freeze:** Quickly lock your hook configuration during security incidents.
- **Development Workflows:** Lock production hooks while maintaining development hook flexibility.

---

## Example Configuration Transactions

### Enable/Disable Hook Lock (Toggle)

```json
{
  "TransactionType": "Invoke",
  "Account": "rAdminAccount...",
  "HookParameters": [
    { "HookParameter": 
        { "HookParameterName": "4C4F434B",  // LOCK
          "HookParameterValue": "01" 
        }
    }
  ]
}
```

**Note:** The value of `LOCK` can be any 1-byte value (e.g., `01`, `FF`, etc.). Any value triggers the toggle.

### Install Hook with Admin Account

```json
{
  "TransactionType": "SetHook",
  "Account": "rHookOwner...",
  "Hooks": [
    {
      "Hook": {
        "CreateCode": "...",
        "HookParameters": [
          { "HookParameter": 
              { "HookParameterName": "41444d494e",  // ADMIN
                "HookParameterValue": "AABBCCDDEEFF00112233445566778899AABBCCDD" 
              }
          }
        ]
      }
    }
  ]
}
```

---

## Security & Compliance

- **Admin-Only Configuration:** Only the designated admin account can toggle the lock.
- **On-Chain Transparency:** Lock state changes are visible and auditable on-chain.
- **Immutable Admin Account:** The admin account is set at installation and cannot be changed without removing and reinstalling the hook.
- **Fail-Safe Design:** When lock is enabled, all SetHook attempts are rejected; no exceptions.
- **Non-Admin Protection:** Invoke transactions from non-admin accounts are rejected to prevent unauthorized lock toggling.
- **Resource Optimization:** Non-relevant transactions bypass all state checks via early exit.

---

## Transaction Flow Diagram

```
Incoming Transaction
       │
       ├─ NOT ttINVOKE AND NOT ttHOOK_SET?
       │  └─ Early Exit → Accept (resource optimization)
       │
       ├─ ttINVOKE?
       │  ├─ Admin + LOCK param?       → Toggle lock state → Accept
       │  ├─ Admin, no LOCK?           → Pass through → Accept
       │  └─ Non-admin?                → Reject
       │
       └─ ttHOOK_SET?
          ├─ Lock enabled?             → Reject (CRITICAL)
          └─ Lock disabled?            → Accept
```

---

## Tools & Resources

- **Xahau Hooks Builder** — https://hooks-builder.xrpl.org/develop
- **Hooks API Reference** — https://xrpl-hooks.readme.io/reference/hook-api-conventions
- **Address → AccountID** — https://hooks.services/tools/raddress-to-accountid
- **Explorer / Logs** — https://xahau-testnet.xrplwin.com/

---

## Technical Notes

- **State Key:** `SETHOOK_LOCK_KEY` = `0x5345544800000000ULL` (8 bytes, hex value of "SETH").
- **State Values:** `0x01` = locked, `0x00` or unset = unlocked.
- **Admin Account:** 20-byte account ID set during installation; immutable after deployment.
- **Transaction Type Constants:** `ttINVOKE` and `ttHOOK_SET` are used to identify transaction types.
- **Buffer Comparison:** Admin account is compared to transaction originator using 20-byte buffer equality check.
- **Early Exit:** Non-relevant transaction types skip all state lookups and checks for optimal resource usage.

---

## Summary

The Admin Hook Locker is a powerful but critical security mechanism that protects your hook configuration from unwanted modifications. By restricting `SetHook` transactions to periods when the lock is disabled, it provides effective control over hook stability. However, **enabling the lock is a permanent action until explicitly disabled by the admin account**—ensure your admin account remains secure and accessible at all times. Combined with other hooks in the HandyHooks collection, it forms a comprehensive security framework for Xahau accounts.

---