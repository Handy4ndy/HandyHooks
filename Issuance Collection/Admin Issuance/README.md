# Admin Issuance Hook 

A production-ready Xahau hook that enables controlled, dynamic token issuance through invoke transactions. Features whitelisted access control, flexible destination targeting, and automatic 5% treasury allocation.

## Overview

This hook provides secure token issuance capabilities with the following features:

- **Invoke-Only Processing**: Only processes `ttINVOKE` (type 99) transactions - all other transaction types pass through unchanged
- **Whitelist Access Control**: Only pre-configured whitelisted accounts can trigger token issuance
- **Dynamic Destination**: Each issuance can specify a different destination account via transaction parameters
- **Automatic Treasury Allocation**: 5% of every issuance automatically goes to a configured treasury account
- **Dual Transaction Emission**: Emits two separate transactions (main issuance + treasury allocation)
- **Pass-Through Design**: Non-matching transactions pass through without blocking the hook chain
- **Development Contribution**: A hardcoded development contribution of 0.05 XAH is automatically charged per successful issuance to support ongoing development of the HandyHooks collection

## Hook Parameters

Configure these parameters when installing the hook:

| Parameter | Size     | Description |
|-----------|----------|-------------|
| `IOU`     | 20 bytes | Currency code to be issued by this hook |
| `W_ACC`   | 20 bytes | Whitelisted account ID that can trigger issuance |
| `T_ACC`   | 20 bytes | Treasury account ID that receives 5% of all issuances |

## Transaction Parameters

When invoking the hook, include these parameters in the invoke transaction:

| Parameter | Size     | Format            | Description |
|-----------|----------|-------------------|-------------|
| `AMT`     | 8 bytes  | Big-endian uint64 | Amount of tokens to issue |
| `DEST`    | 20 bytes | Account ID        | Destination account to receive the tokens |

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
          },
          {
            "HookParameter": {
              "HookParameterName": "545F414343",
              "HookParameterValue": "58BF50228721CF7E6B3AE45ACEFEEB71974CA571"
            }
          }
        ]
      }
    }
  ]
}
```

## Usage Example

To issue tokens, the whitelisted account sends an invoke transaction:

```json
{
  "TransactionType": "Invoke",
  "Account": "rWhitelistedAccount...",
  "Destination": "rIssuerAccount...",
  "HookParameters": [
    {
      "HookParameter": {
        "HookParameterName": "44455354",
        "HookParameterValue": "58BF50228721CF7E6B3AE45ACEFEEB71974CA571  "
      }
    },
    {
      "HookParameter": {
        "HookParameterName": "414D54",
        "HookParameterValue": "000000000000000A"
      }
    }
  ],
}

```

## Token Distribution Logic

For every successful invoke transaction:

1. **Main Issuance**: `AMT` tokens → `DEST` account (specified in transaction)
2. **Treasury Allocation**: `AMT ÷ 20` tokens → `T_ACC` account (minimum 1 token guaranteed)

**Example**: If `AMT = 100` tokens:
- Destination receives: **100 tokens**
- Treasury receives: **5 tokens** (100 ÷ 20)
- **Total minted: 105 tokens**

## Development Contribution

This hook includes a hardcoded development contribution of **0.05 XAH** that is automatically charged for each successful token issuance. This contribution:

- Supports ongoing development and maintenance of the HandyHooks collection
- Is only charged when issuance completes successfully (both main issuance and treasury allocation)
- Is sent to a predetermined development account
- Does not affect your token amounts or treasury allocation calculations

The development contribution is charged as a third transaction emission alongside the main issuance and treasury transactions.

## Technical Implementation

### Amount Processing
- Amount parameter converted from 8-byte big-endian buffer to uint64
- Treasury amount calculated using integer division: `issued_amount / 20`
- Minimum treasury amount enforced: 1 token (when division results in 0)
- Both amounts converted to XFL format for transaction serialization

### Transaction Flow
1. Validates transaction type (must be ttINVOKE)
2. Retrieves and validates hook account and origin account
3. Loads hook parameters (IOU, W_ACC, T_ACC)
4. Validates invoking account against whitelist
5. Extracts transaction parameters (AMT, DEST)
6. Calculates main and treasury amounts
7. Builds and emits main issuance transaction
8. Builds and emits treasury transaction
9. Returns success

### Security Features
- **Transaction Type Filtering**: Only processes invoke transactions
- **Whitelist Validation**: Only configured account can trigger issuance
- **Outgoing Transaction Detection**: Ignores hook account invoking itself
- **Parameter Validation**: All required parameters must be present and correct size
- **Amount Validation**: Issued amount must be positive (> 0)

## Pass-Through Messages

The hook allows these transactions to pass through unchanged:

- `"Non-INVOKE transaction passed through."` - Any non-invoke transaction type
- `"Outgoing invoke transaction passed through."` - Hook account invoking itself
- `"Invoke from non-whitelisted account passed through."` - Invoke from unauthorized account

## Error Messages

| Error Message | Cause |
|---------------|-------|
| `"Failed to get hook account."` | Hook account retrieval failed |
| `"Failed to get origin account."` | Origin account extraction failed |
| `"Misconfigured. Currency not set as Hook Parameter."` | Missing IOU parameter |
| `"Misconfigured. Whitelist account not set as Hook Parameter."` | Missing W_ACC parameter |
| `"Misconfigured. Treasury account not set as Hook Parameter."` | Missing T_ACC parameter |
| `"Misconfigured. Amount to issue not passed as otxn parameter."` | Missing AMT transaction parameter |
| `"Misconfigured. Destination account not passed as otxn parameter."` | Missing DEST transaction parameter |
| `"Invalid main amount - must be positive."` | Amount parameter is zero |
| `"Wrong AMT - < xlf 8b req amount, 20b currency, 20b issuer >"` | Amount serialization failed |
| `"Failed To Emit main transaction."` | Main transaction emission failed |
| `"Failed to serialize treasury amount."` | Treasury amount serialization failed |
| `"Failed to emit treasury transaction."` | Treasury transaction emission failed |

## Success Message

`"Admin Issuance: tokens issued to destination + 5% to treasury successfully."`

## Debugging

The hook includes trace logging for development:
- `TRACESTR("Admin Issuance: called")` - Logged when hook executes
- View trace output in Xahau Explorer transaction details or Hooks Builder logs

## Tools & Resources

- **[Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop)**: Compile and deploy hooks
- **[Xahau Hooks Technical](https://xrpl-hooks.readme.io/reference/hook-api-conventions)**: Detailed Hooks references
- **[Address to Account ID Converter](https://hooks.services/tools/raddress-to-accountid)**: Convert addresses to 20-byte account IDs
- **[Amount to Uint64](https://transia-rnd.github.io/xrpl-hex-visualizer/)**: Convert integert amount to Uint64
- **[XahauExplorer](https://xahau-testnet.xrplwin.com/)**: View transactions and hook execution logs

