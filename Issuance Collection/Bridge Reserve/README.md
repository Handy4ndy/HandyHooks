# Bridge Reserve Hook (BRH) - Xahau HandyHook Collection

## Overview
A production-ready Xahau hook that enables cross-chain token bridging through a burn-to-mint mechanism with reserve backing. Features automatic token burning, reserve minting, and cross-chain transfer integrity.

This hook provides secure cross-chain token bridging with the following features:

- **Payment-Only Processing**: Only processes `ttPAYMENT` transactions - all other transaction types pass through unchanged
- **Burn-to-Mint Mechanism**: Tokens sent to issuer are automatically burned, equivalent amounts minted to reserve
- **Cross-Chain Backing**: Reserve account maintains 1:1 backing for cross-chain transfers
- **Currency Filtering**: Only processes specified currency to prevent unauthorized bridging
- **Issuer Validation**: Only processes tokens issued by the hook account itself
- **Pass-Through Design**: Non-matching transactions pass through without blocking the hook chain
- **Development Contribution**: A hardcoded development contribution of 0.05 XAH is automatically charged per successful bridge operation to support ongoing development of the HandyHooks collection

## Hook Parameters

Configure these parameters when installing the hook:

| Parameter | Size     | Description                                      |
|-----------|----------|--------------------------------------------------|
| `IOU`     | 20 bytes | Currency code to filter and bridge               |
| `R_ACC`   | 20 bytes | Reserve account for cross-chain transfer backing |

## Bridge Mechanism

The hook implements a sophisticated burn-to-mint system:

### Burn Process
1. User sends tokens to the hook account (issuer)
2. Tokens are automatically burned by the ledger (issuer receives own tokens)
3. Hook validates currency and issuer match configuration

### Mint Process
1. Hook extracts amount from burned tokens using slot API
2. Equivalent amount is minted to the reserve account
3. Reserve maintains 1:1 backing for cross-chain integrity

### Cross-Chain Flow
1. **Source Chain**: User burns tokens → Reserve gets backing
2. **Bridge Monitoring**: Off-ledger systems detect burn event
3. **Destination Chain**: Equivalent tokens minted based on reserve backing

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
              "HookParameterName": "525F414343",
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

To bridge tokens, users send a payment to the hook account:

```json
{
  "TransactionType": "Payment",
  "Account": "rUserAccount...",
  "Destination": "rIssuerAccount...",
  "Amount": {
    "currency": "XPN",
    "value": "100",
    "issuer": "rIssuerAccount..."
  }
}
```

## Bridge Operation Logic

For every successful bridge operation:

1. **Token Burn**: User tokens automatically burned by ledger
2. **Reserve Mint**: Equivalent amount → Reserve account for backing
3. **Development Contribution**: 0.05 XAH → Development account

**Example**: If user sends 100 XPN tokens:
- User tokens: **100 XPN burned**
- Reserve receives: **100 XPN minted** (backing for cross-chain transfer)
- Development contribution: **0.05 XAH** (separate native payment)

## Development Contribution

This hook includes a hardcoded development contribution of **0.05 XAH** that is automatically charged for each successful bridge operation. This contribution:

- Supports ongoing development and maintenance of the HandyHooks collection
- Is only charged when bridge operation completes successfully
- Is sent to a predetermined development account
- Does not affect your token amounts or reserve calculations

The development contribution is charged as a second transaction emission alongside the reserve minting.

## Technical Implementation

### Payment Validation
- Only processes IOU payments (48-byte amount format)
- Validates destination is hook account (issuer)
- Validates currency matches configured parameter
- Validates issuer is hook account (only bridge own tokens)

### Amount Processing
- Uses slot API to extract amount from burned tokens
- Converts to XFL format for reserve minting
- Maintains exact 1:1 burn-to-mint ratio

### Transaction Flow
1. Validates transaction type (must be ttPAYMENT)
2. Validates payment destination (must be hook account)
3. Validates amount format (must be IOU, not native XAH)
4. Validates currency and issuer match configuration
5. Extracts amount using slot API
6. Builds and emits reserve mint transaction
7. Emits development contribution payment
8. Returns success

### Security Features
- **Transaction Type Filtering**: Only processes payment transactions
- **Destination Validation**: Only processes payments to hook account
- **Currency Filtering**: Only processes configured currency
- **Issuer Validation**: Only processes tokens issued by hook account
- **Amount Validation**: Rejects native XAH and invalid formats
- **Outgoing Transaction Detection**: Ignores hook account sending payments

## Pass-Through Messages

The hook allows these transactions to pass through unchanged:

- `"Non-PAYMENT transaction passed through."` - Any non-payment transaction type
- `"Outgoing transaction passed through."` - Hook account sending payments
- `"Not destined to this account; passed through."` - Payments to other accounts
- `"Native XAH payment; passed through."` - Native XAH payments
- `"Invalid amount format; passed through."` - Malformed amount data
- `"Non-matching currency; passed through."` - Wrong currency payments
- `"Not our issued currency; passed through."` - Tokens from other issuers

## Error Messages

| Error Message | Cause |
|---------------|-------|
| `"Failed to get hook account."` | Hook account retrieval failed |
| `"Failed to get origin account."` | Origin account extraction failed |
| `"Misconfigured. Currency not set as Hook Parameter."` | Missing IOU parameter |
| `"Misconfigured. Reserve account not set as Hook Parameter."` | Missing R_ACC parameter |
| `"bridge: slot otxn failed"` | Transaction slot access failed |
| `"bridge: slot amount failed"` | Amount slot access failed |
| `"Failed to serialize reserve amount."` | Reserve amount serialization failed |
| `"Failed to emit reserve transaction."` | Reserve mint transaction emission failed |

## Success Message

`"Bridge: tokens burned and reserve minted successfully."`

## State Management

The Bridge Reserve hook is **stateless** - it does not store any persistent data. All operations are atomic and based solely on the incoming payment transaction parameters and hook configuration.

## Cross-Chain Integration

### Monitoring Systems
- Bridge events can be monitored via transaction logs
- Off-ledger systems (like Evernode) can detect burn events
- Reserve minting provides backing verification

### Reserve Account
- Maintains 1:1 backing for all bridged tokens
- Can be monitored for reserve health
- Provides transparency for cross-chain operations

## Debugging

The hook includes trace logging for development:
- `TRACESTR("bridge_reserve: called")` - Logged when hook executes
- Development contribution emission failures logged as warnings (non-blocking)
- View trace output in Xahau Explorer transaction details or Hooks Builder logs

## Tools & Resources

- **[Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop)**: Compile and deploy hooks
- **[Xahau Hooks Technical](https://xrpl-hooks.readme.io/reference/hook-api-conventions)**: Detailed Hooks references
- **[Address to Account ID Converter](https://hooks.services/tools/raddress-to-accountid)**: Convert addresses to 20-byte account IDs
- **[Currency Code Converter](https://transia-rnd.github.io/xrpl-hex-visualizer/)**: Convert currency codes to hex
- **[XahauExplorer](https://xahau-testnet.xrplwin.com/)**: View transactions and hook execution logs