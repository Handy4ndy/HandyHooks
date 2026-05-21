# IOU Balance Adjustment (IBA) Hook

## Overview

The **IOU Balance Adjustment (IBA)** hook enables IOU holders to claim rewards on their holdings. This is a configurable rewards distribution system where:
- Interest rates, claim intervals, and reward issuers are managed by admins via invoke transactions
- Users can claim rewards on their IOU balances with automatic trustline and timing constraints
- All rewards are distributed via Remit transactions emitted by the hook
- Claim timing uses Unix timestamps for accurate interval enforcement
- User state is tracked in hierarchical namespaces for unlimited scalability

## Installation

### Required Hook Parameters (at install time)

Deploy the hook with these required parameters:

```json
{
  "HookParameters": [
    {
      "HookParameterName": "43555252454E4359",  // "CURRENCY" in hex
      "HookParameterValue": "000000000000000000000000494F550000000000"  // IOU code
    },
    {
      "HookParameterName": "41444d494e",  // "ADMIN" in hex
      "HookParameterValue": "9809EFB6952078CD84FD7C1B240E66D1C608F810"  // Admin account ID
    },
    {
      "HookParameterName": "495353554552",  // "ISSUER" in hex
      "HookParameterValue": "DEEA27132ACF95D4603E600931B55569A99555E2"  // Issuer account ID
    }
  ]
}
```

### Optional Hook Parameters (can be set at install or via admin commands later)

```json
{
  "HookParameterName": "494e54455245535",  // "INTEREST" in hex
  "HookParameterValue": "00000004"  // Interest rate: 4 = 0.04% (divide by 10000 for percentage)
},
{
  "HookParameterName": "494e544552564c",  // "INTERVAL" in hex
  "HookParameterValue": "0000000A"  // Claim interval: 10 seconds (adjust as needed)
},
{
  "HookParameterName": "4d4158434c41494d",  // "MAXCLAIM" in hex
  "HookParameterValue": "0000000A"  // Max lifetime claims: 10 (unlimited if not set)
}
```

**Parameter Format:** All numeric values are 4 bytes, big-endian unsigned integers.

### Hook Trigger Setting

**Important:** When deploying the hook via SetHook transaction, set the hook trigger to **Invoke** (transaction type 99). The hook will ONLY be triggered by Invoke transactions sent to the hook account. This is where both admin configuration commands and user claim requests are processed.

## Admin Configuration

Admin can update hook configuration at any time by sending an invoke transaction from the ADMIN account to the hook account.

### Example: Update Interest Rate

```json
{
  "Account": "rNiucYuwvETo3UDgEdvFfj8ysgLYSRbfdG",  // ADMIN account
  "Destination": "rMKCWoDDmMgybMKiQKikbvngbozhFdYuJr",  // Hook account
  "TransactionType": "Invoke",
  "Fee": "4052",
  "HookParameters": [
    {
      "HookParameter": {
        "HookParameterName": "494e54455245535",  // "INTEREST"
        "HookParameterValue": "00000008"  // New rate: 8 = 0.08%
      }
    }
  ]
}
```

### Configurable Parameters

| Parameter | Hex Name | Size | Purpose | Example |
|-----------|----------|------|---------|---------|
| INTEREST | 494e54455245535 | 4 bytes | Daily interest rate (0.0001% per unit) | 1000 = 10% |
| INTERVAL | 494e544552564c | 4 bytes | Seconds between claims | 86400 = 1 day |
| MAXCLAIM | 4d4558434c41494d | 4 bytes | Lifetime claim limit (0 = unlimited) | 100 = max 100 claims |
| CURRENCY | 43555252454e4359 | 20 bytes | Reward currency code | 000000000000000000000000494f550000000000 |
| ISSUER | 495353555552 | 20 bytes | Reward issuer account | Account ID (20 bytes) |

## User Claiming Rewards

Users claim rewards by sending a simple invoke transaction to the hook account with NO parameters:

```json
{
  "Account": "rsxsDmJjYG1Yy2w64n1AGHK6eQKvM3Rr78",  // Claimant account
  "Destination": "rMKCWoDDmMgybMKiQKikbvngbozhFdYuJr",  // Hook account
  "TransactionType": "Invoke",
  "Fee": "4040",
  "HookParameters": [],
  "Memos": []
}
```

### Claim Validation (in order of execution)

The hook validates claims in this sequence for optimal performance:

1. **INVOKE type check** - Must be an invoke transaction (type 99)
2. **Account validation** - Accepts outgoing transactions from hook account
3. **Configuration check** - Verifies CURRENCY, ADMIN, ISSUER parameters set
4. **Install-time initialization** - Sets initial values from hook parameters (first time only)
5. **User namespace generation** - Creates unique state key from claimant account ID
6. **Timing validation** - **Checks if INTERVAL has elapsed since last claim** (fails early if not, with seconds remaining)
7. **Max claims check** - Verifies lifetime claim limit not exceeded (if set)
8. **Load configurations** - Retrieves INTEREST, ISSUER, CURRENCY from state
9. **Trustline validation** - Verifies claimant has trustline for reward currency
10. **Balance retrieval** - Loads claimant's IOU balance
11. **Amount calculation** - Computes: `balance × (interest_rate / 10000)`
12. **Emit transaction** - Creates Remit transaction with calculated reward
13. **Update state** - Records claim timestamp and increments claim counter

## Example Reward Remit Transaction

When a user claims successfully, the hook emits a Remit transaction:

```json
{
  "Account": "rMKCWoDDmMgybMKiQKikbvngbozhFdYuJr",  // Hook account
  "TransactionType": "Remit",
  "Destination": "rsxsDmJjYG1Yy2w64n1AGHK6eQKvM3Rr78",  // Claimant
  "Amounts": [
    {
      "AmountEntry": {
        "Amount": {
          "currency": "IOU",
          "issuer": "rMKCWoDDmMgybMKiQKikbvngbozhFdYuJr",
          "value": "0.04030509688461348"  // Calculated reward
        }
      }
    }
  ],
  "Flags": 2147483648,  // tfCanonical
  "SigningPubKey": "000000000000000000000000000000000000000000000000000000000000000000",
  "Fee": "10",
  "FirstLedgerSequence": 8984100,
  "LastLedgerSequence": 8984104,
  "Sequence": 0
}
```

## Error Messages and Debugging

### Timing Error
**Message:** `"Too soon - wait 3542 secs before next."`
- User tried to claim before INTERVAL elapsed
- Response shows exact seconds remaining

### Configuration Errors
- `"CURRENCY not configured - admin must set currency first."`
- `"INTEREST not configured - admin must set interest rate first."`
- `"INTERVAL not configured - admin must set claim interval first."`
- `"ISSUER not configured - admin must set issuer first."`

### Validation Errors
- `"Claimant account does not have required trustline."`
- `"Maximum lifetime claims reached."`
- `"Invalid interest rate - must be positive."`
- `"Failed to serialize claim amount."`

## Trace Output

The hook outputs trace messages for debugging:

```
IBA :: IOU Balance Adjustment :: Called.
IBA :: User claim request processing.
IBA :: Interval requirement passed.
IBA :: Eligibility checks passed.
IBA :: Trustline validated.
IBA :: Claim amount calculated.
IBA :: Claim transaction emitted.
IBA :: User state updated.
IBA :: Success :: Tokens claimed successfully.
```

## Technical Details

### State Management

User state is stored in hierarchical namespaces derived from their account ID:
- **Namespace:** First 20 bytes = claimant account ID, last 12 bytes = 0 padding
- **State Key:** "CLAIM_DATA" (10 bytes) + zero padding
- **State Value:** 8 bytes = [last_claim_timestamp:4 bytes] + [total_claims:4 bytes]

This provides unlimited scalability without namespace congestion.

### Timestamp Handling

The hook converts Ripple Epoch timestamps to Unix Epoch:
- **Ripple Epoch:** Jan 1, 2000 00:00:00 UTC
- **Unix Epoch:** Jan 1, 1970 00:00:00 UTC
- **Conversion:** Add 946684800 seconds to Ripple timestamp

Example INTERVAL values:
- 60 = 1 minute
- 3600 = 1 hour
- 86400 = 1 day
- 604800 = 1 week
- 2592000 = 30 days

### Interest Rate Calculation

Interest rates are stored as big-endian uint32 values and divided by 10000 for percentage:
- `1000` = 0.1%
- `100` = 0.01%
- `10000` = 1.0%
- `100000` = 10.0%

Claim amount formula:
```
claim_amount = balance × (interest_rate / 10000)
```

## Hook Builder Deployment Steps

Follow these steps to deploy the IBA hook to your testnet account using the Xahau Hooks Builder:

### 1. Prepare Your Code

- Go to **[Xahau Hooks Builder - Developer](https://builder.xahau.network/develop)**
- Copy the hook code from `IOUBalanceAdjustment.c` (or the compact `IBA_wasm.c` version)
- Delete the starter template code and paste the IBA hook code

### 2. Compile to WASM

- Click the **"Compile to WASM"** button
- Wait for compilation to complete
- Verify no errors appear in the compilation output
- The compiled bytecode will be displayed (begins with `61...` in hex)

### 3. Deploy the Hook

- Go to **[Xahau Hooks Builder - Deploy](https://builder.xahau.network/deploy)**
- Select or create a funded testnet account (this will be your hook account)
- Click **"Set Hook"**
- Set the Invoke on transactions field to **ttINVOKE**

### 4. Configure Hook Parameters

Before confirming deployment, add the required hook parameters:

**Click "Add Parameter"** and enter each of these three required parameters:

| Parameter | Hex Name | Value | Purpose |
|-----------|----------|-------|----------|
| CURRENCY | `43555252454E4359` | `000000000000000000000000494F550000000000` | IOU currency code |
| ADMIN | `41444d494e` | Your admin account ID (20 bytes hex) | Admin-only configuration account |
| ISSUER | `495353554552` | Issuer account ID (20 bytes hex) | Account distributing rewards |

**Optional parameters** (can be set later via admin commands):

| Parameter | Hex Name | Example Value | Purpose |
|-----------|----------|----------------|----------|
| INTEREST | `494e54455245535` | `00000004` | Interest rate (0.04% with value 4) |
| INTERVAL | `494e544552564c` | `0000000A` | Claim interval in seconds (10 = 10 seconds for testing) |
| MAXCLAIM | `4d4158434c41494d` | `0000000A` | Max lifetime claims (10 = limit to 10 claims) |

### 5. Confirm and Deploy

- Verify the HookHash matches: `51050DEA8AF27E4FF6C9E0DCBC914492316815F3737FBCCFBAE511B0D8CC4B98`
- Click **"Confirm"** to submit the SetHook transaction
- Wait for the transaction to be confirmed on the ledger
- The hook is now active on your account!

### 6. Test the Hook

- Go to **[Xahau Hooks Builder - Test](https://builder.xahau.network/test)**
- Create a test user account with some IOU balance
- Send an Invoke transaction from the test account to the hook account
- Verify the hook emits a Remit transaction with the reward
- Check the debug output (TRACESTR messages) to confirm all validations passed

## Testing Checklist

- [ ] Compile hook without errors
- [ ] Deploy to test account with all required parameters
- [ ] Admin: Set INTEREST rate via invoke
- [ ] Admin: Set INTERVAL via invoke
- [ ] Admin: Set MAXCLAIM limit via invoke
- [ ] User: Send claim invoke (first claim)
- [ ] Verify: Remit transaction emitted with correct amount
- [ ] User: Attempt claim again (should fail: "Too soon - wait X secs")
- [ ] Wait for INTERVAL seconds
- [ ] User: Send claim invoke again (should succeed)
- [ ] Verify: User state updated correctly
- [ ] Verify: Trace messages appear in debug output

## Files

- `IOUBalanceAdjustment.c` - Fully documented reference implementation
- `hook/IBA.c` - Clean, optimized version for WASM compilation
- `hook/IBA.wasm` - Precompiled wasm to aid with mainnet installation.

---

## Hook Hashes

### ⚠️ IMPORTANT: Test Before Mainnet Deployment

**We hold no responsibility for misconfigured distributions, user complaints, data loss, or any issues arising from incorrect hook configuration or usage.** Please thoroughly test this hook on testnet before deploying to mainnet.

### Xahau Testnet

```
51050DEA8AF27E4FF6C9E0DCBC914492316815F3737FBCCFBAE511B0D8CC4B98
```

**Deploy on Testnet First:**
- Use the [Xahau Hooks Builder - Deploy](https://builder.xahau.network/deploy) to test the hook
- Verify all functionality works as expected
- Test admin configuration updates
- Test user claim transactions and reward emissions
- Monitor trace output and verify correct behavior

### Xahau Mainnet

```
51050DEA8AF27E4FF6C9E0DCBC914492316815F3737FBCCFBAE511B0D8CC4B98
```

**Deploy on Mainnet Only After Testnet Validation:**

1. **Deploy from Hash**: Go to **[XRPLWin Hook Deployment Tool](https://xahau.xrplwin.com/tools/hook/from-hash)**
   - Paste the hook hash above
   - Select your account and configure hook parameters carefully
   - Submit the SetHook transaction

2. **Manage Hook Configuration**: Go to **[XRPLWin Account Hook Manager](https://xahau.xrplwin.com/account/<INSERT_rADDRESS>/manage/hooks)**
   - Replace `<INSERT_rADDRESS>` with your account address
   - Manage hook parameters (INTEREST, INTERVAL, MAXCLAIM, etc.)
   - Update configurations as needed

---

**Author:** @Handy_4ndy  
**Ledger:** Xahau  
**License:** MIT
