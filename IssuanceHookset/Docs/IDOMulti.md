# IDO Master Hook (IDOM)

## Overview

The IDO Master Hook (IDOM) is a sophisticated smart contract for Xahau that enables Initial DEX Offerings (IDOs). It facilitates token sales with multiple phases, dynamic multipliers, soft cap evaluation, and refund mechanisms. Users deposit XAH during active phases to receive IOU tokens at varying rates, with options for unwinding positions during eligible periods.

## Purpose

IDOs allow issuers to launch token sales with structured phases that incentivize early participation through higher multipliers. The hook manages the entire lifecycle: setup, deposits, issuance, soft cap evaluation, and refunds. It integrates with whitepaper validation to ensure user acknowledgment of terms and includes balance protection to prevent premature fund withdrawal.

## Hook Parameters

The hook requires several parameters set at installation time:

- **ADMIN** (20 bytes): Account ID of the administrator authorized to configure the IDO.
- **CURRENCY** (20 bytes): Currency code for the IOU tokens to be issued.
- **INTERVAL** (4 bytes): Ledger interval per phase (big-endian uint32).
- **SOFT_CAP** (8 bytes): Soft cap in XAH (big-endian uint64).
- **WP_LNK** (variable): Whitepaper/documentation link for validation.

## Admin Configuration

### Invoke Parameters
- **START** (4 bytes): Ledger offset to begin the IDO window (big-endian uint32).
- **WP_LNK** (variable): Whitepaper link stored in state for validation.

## Phases and Multipliers

The IDO operates in 5 phases with decreasing multipliers:

- **Phase 1**: 100x multiplier
- **Phase 2**: 75x multiplier
- **Phase 3**: 50x multiplier
- **Phase 4**: 25x multiplier
- **Phase 5**: Cooldown/unwind period (no new deposits)

Phases are determined by elapsed ledgers since the start, divided by the interval offset.

## User Actions

### Deposits
- Send XAH payments during active phases (1-4).
- Include 'WP_LNK' parameter matching the stored whitepaper link.
- Receive IOU tokens at the current phase's multiplier rate.

### Unwinding
- Send exact IOU amount back during eligible periods for proportional XAH refund.
- Available during Phase 5 (refund mode) or before window end (successful sales).

## Transaction Handling

### Invoke Transactions
- **Authorized Admin**: Can set START and WP_LNK to initialize the IDO window.
- **Unauthorized**: Rejected.

### Payment Transactions

#### Outgoing Payments
- **IOU Payments**: Always accepted (token issuance).
- **XAH Payments**: Checked against unlocked balance (locked during active IDO).
- **Other**: Accepted.

#### Incoming Payments

##### XAH Deposits (Active Phases)
- Validates WP_LNK parameter match.
- Checks window active and phase valid.
- Issues IOU tokens via Remit transaction.
- Updates global and user participation counters.

##### IOU Unwinds
- Validates issuer and amount.
- Checks refund mode or exact amount match.
- Emits XAH refund payment.
- Updates counters and removes user data.

## State Management

The hook maintains several state keys:

- **START**: Window start ledger.
- **END**: Window end ledger.
- **WP_LNK**: Stored whitepaper link.
- **SOFT_CAP**: Soft cap threshold.
- **XAH**: Total XAH raised.
- **IOU**: Total IOU issued.
- **EXEC**: Total execution count.
- **PHASE[1-5]**: Phase-specific execution counts.
- **REFUND**: Refund mode flag (0=success, 1=refund).
- **TOTAL_RAISED**: Preserved total for successful sales.
- **IDO_DATA** (foreign): User participation data (XAH deposited, IOU received).

## Soft Cap Evaluation

After Phase 4 ends:
- Compares total XAH raised against soft cap.
- If met: Sale successful, funds unlock after cooldown.
- If not met: Refund mode activated, users can unwind.

## Balance Protection

- Funds are locked during active IDO to prevent withdrawal.
- Locked amount = total XAH raised × 1,000,000 (drops).
- Unlocks after successful sale and cooldown period.

## Accepted Transactions

- Admin invokes with START/WP_LNK parameters.
- Outgoing IOU and other payments.
- Outgoing XAH with sufficient unlocked balance.
- Incoming XAH during active phases with valid WP_LNK.
- Incoming IOU for unwinding during eligible periods.

## Rejected Transactions

- Unauthorized invokes.
- XAH deposits without WP_LNK or during invalid phases.
- IOU unwinds with wrong issuer or invalid amounts.
- Payments outside active window (unless refund mode).
- Outgoing XAH exceeding unlocked balance.

## Error Messages

The hook provides detailed error messages for debugging:
- "IDO :: Error :: [specific issue]" for configuration/setup problems.
- "IDO :: Rejected :: [reason]" for invalid transactions.
- "IDO :: Unwind :: [issue]" for unwind-specific errors.
- "IDO :: Accepted :: [description]" for successful transactions.

## Security Considerations

- Admin authorization prevents unauthorized configuration.
- WP_LNK validation ensures user acknowledgment.
- Exact amount matching for unwinds prevents manipulation.
- Balance protection secures raised funds.
- State validation prevents race conditions.

## Integration

Designed to work with:
- Router Hook for transaction routing.
- Rewards Hook for post-IDO incentives.
- Compatible with Xahau's Remit transaction type for IOU issuance.

## Usage Example

1. **Setup**: Install hook with parameters, admin invokes with START.
2. **Deposit Phase**: Users send XAH with WP_LNK during phases 1-4.
3. **Evaluation**: After Phase 4, soft cap checked automatically.
4. **Unwind/Claim**: Users unwind IOU for refunds or hold for rewards.

## Author

@Handy_4ndy

## License

Part of the Xahau HandyHook Collection. See repository license for details.