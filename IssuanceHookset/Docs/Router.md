# IDO Router Hook

## Overview

The IDO Router Hook is a critical component of the Xahau HandyHook Collection, designed to manage the execution flow in a hook chain that includes both IDO (Initial Dex Offering) and Rewards hooks. It acts as a smart router, determining whether to execute the IDO hook, the Rewards hook, or skip them based on transaction type, parameters, and current state.

## Purpose

In a hook chain setup, multiple hooks can be installed on an account. The Router Hook ensures that only the appropriate hook logic runs for each transaction, preventing conflicts and optimizing performance. It uses hardcoded hashes to identify and control the execution of the IDO and Rewards hooks.

## Hardcoded Configuration

The Router Hook uses predefined byte arrays for hook hashes and namespace to ensure compatibility and security:

- **IDO_HOOK_HASH**: The SHA-256 hash of the IDO hook binary. Used to skip or allow execution of the IDO hook.
- **REWARDS_HOOK_HASH**: The SHA-256 hash of the Rewards hook binary. Used to skip or allow execution of the Rewards hook.
- **IDO_NAMESPACE**: A 32-byte namespace used for storing and retrieving IDO-related state data from foreign accounts.

These values are hardcoded to prevent runtime configuration errors and ensure deterministic behavior.

## Functionality

### Transaction Routing Logic

The Router Hook analyzes incoming transactions and routes them as follows:

#### Outgoing Transactions
- **Invoke (ttINVOKE)**: Skips the IDO hook, allowing the Rewards hook to process admin configurations or claims.
- **Payment (ttPAYMENT)**:
  - XAH payments: Skips the Rewards hook, allowing IDO logic to handle outgoing XAH (e.g., refunds).
  - Non-XAH payments: Skips the IDO hook, allowing Rewards hook to process.

#### Incoming Transactions

##### Invoke Transactions
- **START Parameter**: Runs IDO hook (for initialization), skips Rewards hook.
- **Rewards Admin Parameters** (INT_RATE, SET_INTERVAL, SET_MAX_CLAIMS): Skips IDO hook, runs Rewards hook.
- **R_CLAIM Parameter**: Skips IDO hook, runs Rewards hook for user claims.
- **Invalid Invoke**: Rejected if no recognized parameters.

##### Payment Transactions

The Router checks the IDO window status and refund mode:

- **Refund Mode Active**: Only accepts incoming IOU payments (for unwinding), rejects invokes and outgoing payments.
- **IDO Ended**: Skips IDO hook for all transactions.
- **Window Not Active**: Skips IDO hook unless refund mode.

For active windows or refund mode:

- **XAH Payments**:
  - Must include WP_LNK parameter (whitepaper acknowledgment) or have existing raised funds.
  - Runs IDO hook, skips Rewards hook.
- **IOU Payments**:
  - Must have participation data in user state.
  - Runs IDO hook, skips Rewards hook.

## Accepted Transactions

- Outgoing invokes and payments.
- Incoming invokes with START, rewards admin params, or R_CLAIM.
- Incoming XAH payments with WP_LNK or during active IDO with raised funds.
- Incoming IOU payments from participants during active IDO or refund mode.

## Rejected Transactions

- Incoming payments outside active window (unless refund mode).
- XAH deposits without WP_LNK acknowledgment and no raised funds.
- IOU deposits without participation data.
- Invalid invokes without required parameters.
- Transactions during refund mode that aren't incoming IOU payments.

## Installation and Setup

1. **Hook Chain Order**: Install the Router Hook as the first hook in the chain, followed by the IDO hook and Rewards hook.
2. **Parameters**: No runtime parameters required; all configuration is hardcoded.
3. **Dependencies**: Requires the IDO and Rewards hooks to be installed with matching hashes.

## State Dependencies

The Router queries the following state keys from the IDO namespace:
- START: IDO window start ledger.
- END: IDO window end ledger.
- REFUND: Refund mode flag.
- XAH: Total raised XAH (for permissive XAH deposits).
- IDO_DATA: User participation data (per account namespace).

## Error Handling

The Router Hook provides detailed error messages for debugging:
- "Router: Skip failed" - Hook skip operation failed.
- "Router: Cannot read sender account" - Unable to read transaction sender.
- Various routing-specific messages for invalid transactions.

## Security Considerations

- Hardcoded hashes prevent tampering with hook execution.
- Strict parameter validation ensures only intended transactions are processed.
- State queries use foreign namespaces to access IDO data securely.

## Compatibility

- Designed for Xahau network.
- Compatible with IDO and Rewards hooks from the HandyHook Collection.
- Uses standard Hook API functions for maximum compatibility.

## Author

@Handy_4ndy

## License

Part of the Xahau HandyHook Collection. See repository license for details.