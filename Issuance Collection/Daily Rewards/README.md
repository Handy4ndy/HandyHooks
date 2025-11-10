# IOU Daily Rewards Hook (DRH) - Xahau HandyHook Collection

## Overview

A production-ready Xahau hook that enables automated daily token rewards through invoke transactions. Features configurable claim amounts, timing intervals, lifetime limits, and user-specific state tracking.
This hook provides secure daily rewards distribution with the following features:

- **Invoke-Only Processing**: Only processes `ttINVOKE` (type 99) transactions - all other transaction types pass through unchanged
- **Admin Configuration**: Whitelisted admin can configure reward amounts, intervals, and limits
- **Daily Claim System**: Users can claim configured daily rewards with timing constraints
- **User State Tracking**: Individual claim history and limits using hierarchical namespaces
- **Trustline Validation**: Ensures claimants have required trustlines before processing
- **Flexible Timing**: Configurable claim intervals (default 24 hours)
- **Lifetime Limits**: Optional maximum claims per user
- **Pass-Through Design**: Non-matching transactions pass through without blocking the hook chain
- **Service Fee**: A hardcoded service fee of 0.05 XAH is automatically charged per successful claim to support ongoing development of the HandyHooks collection

## Hook Parameters

Configure these parameters when installing the hook:

| Parameter | Size     | Description                                              |
|-----------|----------|----------------------------------------------------------|
| `IOU`     | 20 bytes | Currency code to be distributed as daily rewards         |
| `W_ACC`   | 20 bytes | Whitelisted admin account ID that can configure the Hook |

## Admin Configuration Commands

The whitelisted admin account can configure the system using these invoke transaction parameters:

| Parameter | Size     | Format            | Description |
|-----------|----------|-------------------|-------------|
| `SET_TREASURY` | 20 bytes | Account ID | Configure treasury account (kept for future use) |
| `SET_DAILY` | 8 bytes | Big-endian uint64 | Set daily claim amount |
| `SET_INTERVAL` | 4 bytes | Big-endian uint32 | Set claim interval in ledgers (default: 17280 = ~24 hours) |
| `SET_MAX_CLAIMS` | 4 bytes | Big-endian uint32 | Set maximum lifetime claims per user (0 = unlimited) |

## Daily Claim Parameters

Users can claim daily rewards using this invoke transaction parameter:

| Parameter | Size     | Format            | Description                                             |
|-----------|----------|-------------------|---------------------------------------------------------|
| `R_CLAIM` | 20 bytes | Account ID        | Claim daily rewards (usually claimant's own account ID) |

## Installation Example

```json
{
  "Account": "rIssuerAccount...",
  "TransactionType": "SetHook",
  "Hooks": [
    {
      "Hook": {
        "CreateCode": "0061736D01000000...",
        "Flags": 1,
        "HookApiVersion": 0,
        "HookNamespace": "4FF9961269BF7630D32E15276569C94470174A5DA79FA567C0F62251AA9A36B9",
        "HookOn": "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFFFFFFFFFFFFFBFFFFF",
        "HookParameters": [
          {
            "HookParameter": {
              "HookParameterName": "494F55",
              "HookParameterValue": "00000000000000000000000058504E0000000000"
            }
          },
          {
            "HookParameter": {
              "HookParameterName": "575F414343",
              "HookParameterValue": "F734DAE9FB86A7EA543BBFFECBF432F50D2B6423"
            }
          }
        ]
      }
    }
  ]
}
```

## Usage Examples

### Admin Configuration

Set daily reward amount to 10 tokens:
```json
{
  "TransactionType": "Invoke",
  "Account": "rAdminAccount...",
  "Destination": "rIssuerAccount...",
  "HookParameters": [
    {
      "HookParameter": {
        "HookParameterName": "5345545F4441494C59",
        "HookParameterValue": "000000000000000A"
      }
    }
  ]
}
```

Set claim interval to 12 hours (8640 ledgers):
```json
{
  "TransactionType": "Invoke",
  "Account": "rAdminAccount...",
  "Destination": "rIssuerAccount...",
  "HookParameters": [
    {
      "HookParameter": {
        "HookParameterName": "5345545F494E54455256414C",
        "HookParameterValue": "000021C0"
      }
    }
  ]
}
```

### User Daily Claim

```json
{
  "TransactionType": "Invoke",
  "Account": "rUserAccount...",
  "Destination": "rIssuerAccount...",
  "HookParameters": [
    {
      "HookParameter": {
        "HookParameterName": "525F434C41494D",
        "HookParameterValue": "58BF50228721CF7E6B3AE45ACEFEEB71974CA571"
      }
    }
  ]
}
```

## Reward Distribution Logic

For every successful daily claim:

1. **User Reward**: Full configured daily amount → Claimant account
2. **Service Fee**: 0.05 XAH → Development account

**Example**: If daily amount = 10 tokens:
- User receives: **10 tokens**
- Service fee: **0.05 XAH** (separate native payment)
- **Total tokens distributed: 10 tokens**

## Service Fee

This hook includes a hardcoded service fee of **0.05 XAH** that is automatically charged for each successful daily claim. This fee:

- Supports ongoing development and maintenance of the HandyHooks collection
- Is only charged when claim completes successfully
- Is sent to a predetermined development account
- Does not affect your token reward amounts

The service fee is charged as a second transaction emission alongside the main reward distribution.

## Technical Implementation

### User State Management
- Each user has individual state tracking using hierarchical namespaces
- State includes: last claim ledger sequence, total lifetime claims
- Namespace derived from user account ID for unlimited scalability
- State stored on hook account using `state_foreign` operations

### Timing Constraints
- Default interval: 17280 ledgers (~24 hours on Xahau)
- Configurable via `SET_INTERVAL` admin command
- Prevents claims until sufficient ledgers have elapsed

### Amount Processing
- Daily amount stored as 8-byte big-endian uint64 in hook state
- Converted to XFL format for transaction serialization
- Admin configurable via `SET_DAILY` command

### Transaction Flow
1. Validates transaction type (must be ttINVOKE)
2. Determines admin configuration vs user claim
3. For admin: Updates configuration in hook state
4. For claims: Validates timing, limits, and trustlines
5. Emits reward transaction to claimant
6. Emits service fee payment
7. Updates user state with new claim data

### Security Features
- **Transaction Type Filtering**: Only processes invoke transactions
- **Admin Access Control**: Only whitelisted account can configure system
- **Trustline Validation**: Validates claimant has required trustline before processing
- **Timing Enforcement**: Prevents claims before interval expires
- **Lifetime Limits**: Optional maximum claims per user
- **State Isolation**: User-specific namespaces prevent state conflicts

## Pass-Through Messages

The hook allows these transactions to pass through unchanged:

- `"Non-INVOKE transaction passed through."` - Any non-invoke transaction type
- `"Outgoing invoke transaction passed through."` - Hook account invoking itself
- `"Admin configuration: No valid parameters provided."` - Admin invoke without config parameters
- `"Invoke from non-whitelisted account passed through."` - Non-admin without claim parameter

## Admin Configuration Messages

| Success Message | Action |
|----------------|--------|
| `"Treasury account configured successfully."` | Treasury account set |
| `"Daily claim amount configured successfully."` | Daily reward amount set |
| `"Claim interval configured successfully."` | Claim timing interval set |
| `"Max claims limit configured successfully."` | Lifetime claim limit set |

## Error Messages

| Error Message | Cause |
|---------------|-------|
| `"Failed to get hook account."` | Hook account retrieval failed |
| `"Failed to get origin account."` | Origin account extraction failed |
| `"Misconfigured. Currency not set as Hook Parameter."` | Missing IOU parameter |
| `"Misconfigured. Whitelist account not set as Hook Parameter."` | Missing W_ACC parameter |
| `"Daily claim: DAILY_AMT not configured - admin must use SET_DAILY first."` | Daily amount not configured |
| `"Daily claim: Invalid daily amount - must be positive."` | Daily amount is zero |
| `"Daily claim: Claimant account does not have required trustline."` | Missing trustline |
| `"Daily claim: Too soon - wait more ledgers before next claim."` | Claim interval not elapsed |
| `"Daily claim: Maximum lifetime claims reached."` | User hit claim limit |
| `"Daily claim: Failed to serialize claim amount."` | Amount serialization failed |
| `"Daily claim: Failed to emit claim transaction."` | Claim transaction emission failed |
| `"Daily claim: Failed to update user state."` | User state update failed |

## Success Messages

- `"Daily claim: Tokens claimed successfully."` - Successful daily claim

## State Storage

### Hook State (Global Configuration)
- `TREASURY`: Treasury account (20 bytes)
- `DAILY_AM`: Daily claim amount (8 bytes)
- `INTERVAL`: Claim interval in ledgers (4 bytes)
- `MAX_CLM`: Maximum lifetime claims (4 bytes)

### User State (Per-User Tracking)
- **Namespace**: Derived from user account ID (first 20 bytes + 12 zero bytes)
- **Key**: `CLAIM_DATA` (padded to 32 bytes)
- **Value**: 8 bytes containing:
  - Bytes 0-3: Last claim ledger sequence (big-endian uint32)
  - Bytes 4-7: Total lifetime claims (big-endian uint32)

## Debugging

The hook includes trace logging for development:
- View trace output in Xahau Explorer transaction details or Hooks Builder logs
- Service fee emission failures logged as warnings (non-blocking)

## Tools & Resources

- **[Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop)**: Compile and deploy hooks
- **[Xahau Hooks Technical](https://xrpl-hooks.readme.io/reference/hook-api-conventions)**: Detailed Hooks references
- **[Address to Account ID Converter](https://hooks.services/tools/raddress-to-accountid)**: Convert addresses to 20-byte account IDs
- **[Amount to Uint64](https://transia-rnd.github.io/xrpl-hex-visualizer/)**: Convert integer amount to Uint64
- **[XahauExplorer](https://xahau-testnet.xrplwin.com/)**: View transactions and hook execution logs