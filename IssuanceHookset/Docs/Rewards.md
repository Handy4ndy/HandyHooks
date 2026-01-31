# Issuance Rewards Hook (IRH)

## Overview

The Issuance Rewards Hook (IRH) enables IOU token holders to claim periodic rewards on their holdings after the completion of an Initial DEX Offering (IDO). It provides configurable interest rates, claim intervals, and lifetime limits, with integration for IDO participants to receive bonus rewards.

## Purpose

After an IDO concludes successfully, token holders can earn ongoing rewards on their IOU balances. The hook enforces timing constraints, trustline requirements, and configurable limits to ensure fair and controlled reward distribution. It integrates with the IDO hook to provide bonus rates for original participants.

## Hook Parameters

The hook requires several parameters set at installation time:

- **CURRENCY** (20 bytes): Currency code for the IOU tokens being rewarded.
- **ADMIN** (20 bytes): Account ID of the administrator authorized to configure rewards.
- **INT_RATE** (4 bytes): Initial daily interest rate (big-endian uint32, e.g., 1000 = 10%).
- **SET_INTERVAL** (4 bytes): Initial claim interval in ledgers (big-endian uint32).
- **SET_MAX_CLAIMS** (4 bytes): Initial lifetime claim limit per user (big-endian uint32). (Optional)

## Admin Configuration

### Invoke Parameters (Install-time or Runtime)
- **INT_RATE** (4 bytes): Set daily interest rate (big-endian uint32, e.g., 1000 = 10%).
- **SET_INTERVAL** (4 bytes): Set claim interval in ledgers (big-endian uint32).
- **SET_MAX_CLAIMS** (4 bytes): Set lifetime claim limit per user (big-endian uint32).

## User Actions

### Claiming Rewards
- Send invoke transaction with 'R_CLAIM' parameter containing the claimant's account ID.
- Hook validates trustline, timing, and limits.
- Calculates rewards based on current IOU balance and configured rate.
- IDO participants receive +5% bonus on their base rate.
- Emits Remit transaction to deliver reward tokens.

## Reward Calculation

- **Base Rate**: IOU Balance × Interest Rate / 10000
- **Bonus Rate**: For IDO participants, base rate + 5%
- Rewards are calculated in real-time based on current balance
- Uses XFL floating-point arithmetic for precision

## State Management

The hook maintains several state keys:

- **INT_RATE**: Configured interest rate.
- **CLAIM_INT**: Claim interval in ledgers.
- **MAX_CLM**: Maximum lifetime claims per user.

User-specific state is stored in hierarchical namespaces:
- **CLAIM_DATA**: Per-user claim tracking (last claim ledger, total claims).
- Uses account-derived namespaces for unlimited scalability.

## Integration with IDO Hook

- Queries foreign state from IDO hook to detect participation.
- Participants receive bonus rewards (+5% interest).
- Ensures rewards are only available after IDO completion.

## Constraints and Validation

### Timing
- Claims must wait for configured interval since last claim.
- Enforced per user with ledger-based timestamps.

### Trustlines
- Users must have established trustline for the reward currency.
- Balance must be positive (IOU holdings).

### Limits
- Optional lifetime claim limit per user.
- Configurable by admin, default unlimited.

## Transaction Handling

### Invoke Transactions

#### Admin Configuration
- From authorized admin account only.
- Can set interest rate, interval, and max claims.
- Parameters can be set at install or updated via invoke.

#### User Claims
- From any account with valid trustline.
- Requires 'R_CLAIM' parameter with claimant account ID.
- Validates all constraints before processing.

#### Other Invokes
- Outgoing invokes from hook account: Passed through.
- Invalid invokes: Rejected with detailed messages.

## Accepted Transactions

- Admin invokes with configuration parameters.
- Outgoing invokes.
- User invokes with valid 'R_CLAIM' parameter and all constraints met.

## Rejected Transactions

- Non-invoke transactions.
- Unauthorized admin configuration attempts.
- Invalid 'R_CLAIM' parameters.
- Missing trustlines.
- Timing violations (too soon since last claim).
- Lifetime claim limit exceeded.
- Missing configuration (interest rate/interval not set).

## Error Messages

The hook provides detailed error messages for debugging:
- "IRH :: Error :: [specific issue]" for configuration/setup problems.
- "IRH :: Success :: [description]" for successful operations.

## Security Considerations

- Admin authorization prevents unauthorized configuration changes.
- Trustline validation ensures only legitimate holders can claim.
- Timing and limit enforcement prevents abuse.
- Hierarchical namespaces prevent state collisions.
- Real-time balance checks prevent stale claims.

## Usage Example

1. **Setup**: Install hook with parameters, admin configures rates.
2. **Post-IDO**: After IDO completion, users can start claiming.
3. **Claim**: User sends invoke with 'R_CLAIM' parameter.
4. **Validation**: Hook checks all constraints and calculates rewards.
5. **Distribution**: Reward tokens emitted to claimant.

## Performance Notes

- Uses foreign state queries for IDO integration.
- Hierarchical namespaces ensure scalability.
- Efficient validation order minimizes processing overhead.

## Author

@Handy_4ndy

## License

Part of the Xahau HandyHook Collection. See repository license for details.