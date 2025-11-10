# Native XAH-to-Token Issuance Hook

A Xahau hook that automatically issues custom tokens in response to incoming XAH payments with automatic treasury allocation.

## Overview

This hook enables automatic token issuance where:
- **Processes ttPAYMENT transactions** - responds to incoming XAH payments
- **Multiplier-based issuance**: Issues tokens = XAH received × multiplier
- **Automatic treasury allocation**: Mints additional 10% to treasury account
- **Configurable currency**: Set custom token currency at installation
- **Pass-through compatible**: Non-XAH payments and outgoing transactions pass through

## Setup

### 1. Configure Hook Parameters

**Required Hook Parameters:**
- `M` - Multiplier value (8 bytes, big-endian uint64) - determines tokens per XAH
- `T_ACC` - Treasury account ID (20 bytes) - receives 10% of all issued tokens
- `C` - Currency code (20 bytes, optional) - defaults to XPN if not provided

### 2. Install the Hook

Install this hook on the issuer account, configured to trigger on **Payment** transactions.

## Usage

### Automatic Token Issuance

To receive tokens:

1. **Send XAH Payment** to the hook account
2. **The hook will automatically:**
   - Calculate tokens = XAH amount × multiplier
   - Issue calculated tokens to the sender
   - Mint additional 10% to treasury account

## Example Scenarios

### Installation Parameters:
- `M`: `"000000000000000A"` (10x multiplier)
- `T_ACC`: `"1234567890ABCDEF1234567890ABCDEF12345678"` (treasury account)
- `C`: `"00000000000000000000000058504E000000000000000000000000"` (XPN, optional)

### Payment Example:
- **User sends**: 5 XAH to hook account
- **User receives**: 50 XPN tokens (5 × 10 multiplier)
- **Treasury receives**: 5 XPN tokens (10% of 50)
- **Total minted**: 55 XPN tokens

## Key Features

### ✅ **Automatic Response**
- No invoke transactions needed
- Simply send XAH to receive tokens
- Immediate token issuance

### ✅ **Multiplier-Based Calculation**
- Configurable multiplier at installation
- Consistent exchange rate: XAH → Tokens
- Integer-based calculation (XAH amount × multiplier)

### ✅ **Treasury Allocation**
- 10% of issued tokens automatically go to treasury
- Ensures sustainable token economics
- Minimum 1 token guarantee for treasury

### ✅ **Smart Transaction Filtering**
- Only processes incoming XAH payments
- Ignores outgoing transactions (hook sending)
- Rejects non-XAH payments (IOUs pass through)
- Hook chain compatible

### ✅ **Configurable Currency**
- Custom token currency via `C` parameter
- Defaults to XPN if not specified
- 20-byte currency identifier support

## Token Distribution Flow

For every incoming XAH payment:

1. **Main Issuance**: `(XAH × Multiplier)` tokens → Sender
2. **Treasury Allocation**: `(XAH × Multiplier × 10%)` tokens → Treasury account

**Example with 2 XAH and 5x multiplier:**
- Sender receives: **10 tokens**
- Treasury receives: **1 token**
- Total minted: **11 tokens**

## Installation Parameter Examples

### 1:1 Ratio (1 XAH = 1 Token):
```json
{
  "M": "0000000000000001",
  "T_ACC": "your_treasury_account_id_20_bytes",
  "C": "your_currency_code_20_bytes_optional"
}
```

### 100:1 Ratio (1 XAH = 100 Tokens):
```json
{
  "M": "0000000000000064",
  "T_ACC": "your_treasury_account_id_20_bytes"
}
```

## Pass-Through Messages

- `"nativeIssue: Non-PAYMENT transaction passed through."` - Non-payment transactions
- `"nativeIssue: Outgoing transaction passed through."` - Hook's own outgoing payments

## Error Messages

- `"nativeIssue: Failed to get hook account."` - Hook account retrieval failed
- `"nativeIssue: Failed to get origin account."` - Transaction sender retrieval failed
- `"nativeIssue: Non-XAH payment rejected."` - IOU or malformed payment
- `"nativeIssue: Invalid or zero XAH amount."` - Zero or negative XAH amount
- `"nativeIssue: Treasury account not set as Hook Parameter."` - Missing `T_ACC` parameter
- `"nativeIssue: Multiplier (M) parameter must be 8 bytes."` - Invalid multiplier format
- `"nativeIssue: Multiplier (M) must be positive."` - Zero multiplier value
- `"nativeIssue: Currency (C) parameter must be 20 bytes."` - Invalid currency format
- `"nativeIssue: Could not generate trustline keylet."` - Trustline validation failed
- `"nativeIssue: Issued amount is zero."` - Calculation resulted in zero tokens
- `"nativeIssue: Failed to serialize issued amount."` - Token amount serialization error
- `"nativeIssue: Failed to serialize treasury amount."` - Treasury amount serialization error
- `"nativeIssue: Failed to emit main token transaction."` - Main issuance failed
- `"nativeIssue: Failed to emit treasury transaction."` - Treasury transaction failed

## Success Message

`"nativeIssue: Tokens issued to sender + 10% to treasury successfully."`

## Comparison with Invoke Issuer

| Feature | Native Issuer | Invoke Issuer |
|---------|---------------|---------------|
| Trigger | XAH Payment | Invoke Transaction |
| Access | Anyone | Whitelisted Only |
| Destination | Payment Sender | Dynamic (Parameter) |
| Rate | Fixed Multiplier | Dynamic Amount |
| Treasury | 10% Auto | 10% Auto |
| Use Case | Public Exchange | Controlled Issuance |