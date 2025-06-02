# HandyHooks - Xahau Hooks 101 (Part 3)

`Xahau Hooks 101` is a beginner-friendly collection of Xahau Hooks designed to teach transaction processing on the Xahau Network. Written in C and compiled to WebAssembly (WASM) using the [Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop) starter template, these Hooks handle Payment transactions (`ttPayment`) with advanced logic for emitting payments to hardcoded or configurable accounts. Ideal for learning Hooks development, testing on the Xahau Testnet, or deploying to Mainnet.

## Table of Contents
- [Overview](#overview)
- [Tools](#tools)
- [Hooks](#hooks)
  - [011 - Hard Emit](#011---hard-emit)
  - [012 - Hard Multi Emit](#012---hard-multi-emit)
  - [013 - Install Emit](#013---install-emit)
  - [014 - Install Multi Emit](#014---install-multi-emit)
  - [015 - Invoke Emit](#015---invoke-emit)
- [Testing](#testing)
- [Debugging Tips](#debugging-tips)
- [Lessons Learned](#lessons-learned)
- [Contributing](#contributing)

## Overview
Xahau Hooks are WASM modules that add smart contract-like functionality to Xahau accounts. The `Xahau Hooks 101` series (Part 3) includes five Hooks, each demonstrating advanced concepts for processing Payment transactions:

1. **Hard Emit**: Accepts outgoing Payments and incoming XAH Payments matching an exact amount, emitting a payment to a hardcoded account.
2. **Hard Multi Emit**: Accepts outgoing Payments and incoming XAH Payments matching an exact amount, emitting payments to two hardcoded accounts.
3. **Install Emit**: Accepts outgoing Payments and incoming XAH Payments matching an exact amount, emitting a payment to an account set via hook parameters.
4. **Install Multi Emit**: Accepts outgoing Payments and incoming XAH Payments matching an exact amount, emitting payments to two accounts set via hook parameters.
5. **Invoke Emit**: Accepts outgoing Payments and incoming XAH Payments matching an exact amount, emitting a payment to an account set via state, configurable through `ttINVOKE` transactions with a locking mechanism.

All Hooks were compiled using the Xahau Hooks Builder starter template, ensuring compatibility with Testnet and Mainnet.

## Tools
Use these online tools to compile, deploy, test, and manage Hooks—no local installations required:

- **[Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop)**: Web-based IDE for writing, compiling, and deploying Hooks. Use the starter template to compile each `HandyHooks` file.
- **[XRPLWin Xahau Testnet Tools](https://xahau-testnet.xrplwin.com/tools)**: Suite of tools for interacting with the Xahau Testnet, including transaction creation and Hook testing.
- **[XRPLWin Hook Management](https://xahau-testnet.xrplwin.com/account/YOU_WALLET_RADDRESS_HERE/manage/hooks)**: Manage Hooks on your Testnet account (replace `YOU_WALLET_RADDRESS_HERE` with your account, e.g., `rTest123...`).
- **[Xahau Testnet Faucet](https://xahau-test.net/faucet)**: Fund Testnet accounts for testing.
- **[Xahau Explorer](https://test.xahauexplorer.com/en)**: Check transactions and Hook details.
- **[Xahau Mainnet Explorer](https://xahau.xrplwin.com/)**: Check transactions and Hook details.
- **[Hooks Services Tools](https://hooks.services/tools)**: Convert decimal to hex (`AM`, `AO`) and rAddress to Account ID (`A1`, `A2`).
- **[XRPL Hex Visualizer](https://transia-rnd.github.io/xrpl-hex-visualizer/)**: Convert decimal to uint64 for `ttINVOKE` parameters (Hook 015).

## Hooks
Each Hook is installed on `ttPayment` (and `ttINVOKE` for Hook 015) and processes Payment transactions. These Hooks were compiled using the Xahau Hooks Builder starter template and are Mainnet-ready upon validation.

It's recommended that you verify the functionality of any Hook on Testnet before installing on Mainnet due to known security issues with smart contracts.

### 011 - Hard Emit
- **File**: `011_HardEmit.c`
- **Purpose**: Accepts outgoing Payments and incoming XAH Payments matching an exact amount (10 XAH), emitting a 5 XAH Payment to a hardcoded account.
- **Logic**: Accepts if Hook account matches originating account (outgoing) or if incoming XAH amount equals 10 XAH, then emits a 5 XAH Payment to a hardcoded account (`ftxn_acc`); rejects otherwise.
- **Configuration**: Edit the code to set:
  - `exact_amount_value` (line 16, default 10 XAH).
  - `amountOut` (line 19, default 5,000,000 drops = 5 XAH).
  - `ftxn_acc` (line 22, default hardcoded account ID).
- **Installation**:
  - Open `011_HardEmit.c` in Xahau Hooks Builder.
  - Compile using the starter template.
  - Deploy to a Testnet account via Hooks Builder or XRPLWin Hook Management.
- **Test Cases**:
  - Incoming XAH Payment (10 XAH): Accepted, emits 5 XAH to `ftxn_acc` (`Hard Emit: Payment received and forwarded successfully`).
  - Incoming XAH Payment (15 XAH): Rejected (`Hard Emit: Error: Payment amount does not match the required amount`).
  - Outgoing XAH Payment (50 XAH): Accepted (`Hard Emit: Outgoing payment transaction accepted`).
  - Incoming IOU Payment (100 USD): Rejected (`Hard Emit: Error: Non-XAH payment rejected`).
- **Note**: Ensure the Hook account has sufficient XAH balance (≥ 5 XAH + fees) for emission.

### 012 - Hard Multi Emit
- **File**: `012_HardMultiEmit.c`
- **Purpose**: Accepts outgoing Payments and incoming XAH Payments matching an exact amount (10 XAH), emitting 5 XAH Payments to two hardcoded accounts.
- **Logic**: Accepts if Hook account matches originating account (outgoing) or if incoming XAH amount equals 10 XAH, then emits 5 XAH Payments to `ftxn_acc1` and `ftxn_acc2`; rejects otherwise.
- **Configuration**: Edit the code to set:
  - `exact_amount_value` (line 16, default 10 XAH).
  - `amountOut` (line 19, default 5,000,000 drops = 5 XAH).
  - `ftxn_acc1` and `ftxn_acc2` (line 22, default hardcoded account IDs).
- **Installation**:
  - Load `012_HardMultiEmit.c` in Hooks Builder.
  - Compile and deploy via Hooks Builder or XRPLWin.
- **Test Cases**:
  - Incoming XAH Payment (10 XAH): Accepted, emits 5 XAH to both `ftxn_acc1` and `ftxn_acc2` (`Hard Multi Emit: Payment received and forwarded successfully`).
  - Incoming XAH Payment (15 XAH): Rejected (`Hard Multi Emit: Error: Payment amount does not match the required amount`).
  - Outgoing XAH Payment (50 XAH): Accepted (`Hard Multi Emit: Outgoing payment transaction accepted`).
  - Incoming IOU Payment (100 USD): Rejected (`Hard Multi Emit: Error: Non-XAH payment rejected`).
- **Note**: Ensure the Hook account has sufficient XAH balance (≥ 10 XAH + fees) for both emissions.

### 013 - Install Emit
- **File**: `013_InstallEmit.c`
- **Purpose**: Accepts outgoing Payments and incoming XAH Payments matching an exact amount, emitting a payment to an account set via hook parameters.
- **Logic**: Accepts if Hook account matches originating account (outgoing) or if incoming XAH amount equals the `AM` parameter, then emits a payment of `AO` drops to the `A1` account; rejects otherwise.
- **Hook Parameters**:
  - `AM`: Exact XAH amount (4-byte hex, e.g., `0a000000` for 10 XAH).
  - `AO`: Amount to emit in drops (4-byte hex, e.g., `404b4c00` for 5,000,000 drops = 5 XAH).
  - `A1`: Destination account ID (20-byte hex, e.g., `05A506F1996C9E045C1CE7EE7DF7E599C19A6D52`).
  - Use [Hooks Services Tools](https://hooks.services/tools) to convert values.
- **Installation**:
  - Compile `013_InstallEmit.c` in Hooks Builder.
  - Set hook parameters (`AM`, `AO`, `A1`) during deployment via Hooks Builder or XRPLWin.
- **Test Cases**:
  - Incoming XAH Payment (10 XAH, `AM=10`): Accepted, emits 5 XAH to `A1` (`Install Emit: Payment received and forwarded successfully`).
  - Incoming XAH Payment (15 XAH, `AM=10`): Rejected (`Install Emit: Error: Payment amount does not match the required amount`).
  - Outgoing XAH Payment (50 XAH): Accepted (`Install Emit: Outgoing payment transaction accepted`).
  - Incoming IOU Payment (100 USD): Rejected (`Install Emit: Error: Non-XAH payment rejected`).
- **Note**: Ensure the Hook account has sufficient XAH balance for emission and that `A1` differs from the Hook account.

### 014 - Install Multi Emit
- **File**: `014_InstallMultiEmit.c`
- **Purpose**: Accepts outgoing Payments and incoming XAH Payments matching an exact amount, emitting payments to two accounts set via hook parameters.
- **Logic**: Accepts if Hook account matches originating account (outgoing) or if incoming XAH amount equals the `AM` parameter, then emits payments of `AO` drops to `A1` and `A2` accounts; rejects otherwise.
- **Hook Parameters**:
  - `AM`: Exact XAH amount (4-byte hex, e.g., `0a000000` for 10 XAH).
  - `AO`: Amount to emit in drops (4-byte hex, e.g., `404b4c00` for 5,000,000 drops = 5 XAH).
  - `A1`, `A2`: Destination account IDs (20-byte hex, e.g., `05A506F1996C9E045C1CE7EE7DF7E599C19A6D52`, `4E4B7DA94851DB8A54EB7E65ED96A7249F8CCCC8`).
  - Use [Hooks Services Tools](https://hooks.services/tools) to convert values.
- **Installation**:
  - Compile `014_InstallMultiEmit.c` in Hooks Builder.
  - Set hook parameters (`AM`, `AO`, `A1`, `A2`) during deployment.
- **Test Cases**:
  - Incoming XAH Payment (10 XAH, `AM=10`): Accepted, emits 5 XAH to both `A1` and `A2` (`Install Multi Emit: Payment received and forwarded successfully`).
  - Incoming XAH Payment (15 XAH, `AM=10`): Rejected (`Install Multi Emit: Error: Payment amount does not match the required amount`).
  - Outgoing XAH Payment (50 XAH): Accepted (`Install Multi Emit: Outgoing payment transaction accepted`).
  - Incoming IOU Payment (100 USD): Rejected (`Install Multi Emit: Error: Non-XAH payment rejected`).
- **Note**: Ensure the Hook account has sufficient XAH balance (≥ 2 × `AO` + fees) and that `A1`, `A2` are unique and differ from the Hook account.

### 015 - Invoke Emit
- **File**: `015_InvokeEmit.c`
- **Purpose**: Accepts outgoing Payments and incoming XAH Payments matching an exact amount, emitting a payment to an account set via state, configurable through `ttINVOKE` transactions with a locking mechanism.
- **Logic**: 
  - For `ttINVOKE`: Allows the hook owner to set `AM` (exact amount), `AO` (emit amount), `A1` (destination account), or `LOCK` (passkey), rejecting unauthorized or invalid settings.
  - For `ttPAYMENT`: Accepts if Hook account matches originating account (outgoing) or if incoming XAH amount equals the `AM` state, then emits `AO` drops to the `A1` account; rejects otherwise.
- **Hook Parameters** (for `ttINVOKE`):
  - `AM`: Exact XAH amount (8-byte hex, e.g., `000000000000000A` for 10 XAH).
  - `AO`: Amount to emit in XAH (8-byte hex, e.g., `0000000000000005` for 5 XAH).
  - `A1`: Destination account ID (20-byte hex).
  - `LOCK`: Passkey to lock state changes (8-byte hex).
  - `PASS`: Passkey to unlock state changes (8-byte hex).
  - Use [XRPL Hex Visualizer](https://transia-rnd.github.io/xrpl-hex-visualizer/) and [Hooks Services Tools](https://hooks.services/tools).
- **Installation**:
  - Compile `015_InvokeEmit.c` in Hooks Builder for `ttPayment` and `ttINVOKE`.
  - Deploy to a Testnet account.
  - Send `ttINVOKE` transactions to set `AM`, `AO`, `A1`, and optionally `LOCK`.
- **Test Cases**:
  - `ttINVOKE` (owner, set `AM=10`, `AO=5`, `A1`): Accepted (`Invoke Emit: AM state set successfully`, etc.).
  - `ttINVOKE` (non-owner): Rejected (`Invoke Emit: Error: Only hook owner can change settings`).
  - Incoming XAH Payment (10 XAH, `AM=10`): Accepted, emits 5 XAH to `A1` (`Invoke Emit: Payment received and forwarded successfully`).
  - Incoming XAH Payment (15 XAH, `AM=10`): Rejected (`Invoke Emit: Error: Payment amount does not match AM`).
  - Outgoing XAH Payment (50 XAH): Accepted (`Invoke Emit: Outgoing payment transaction accepted`).
  - Incoming IOU Payment (100 USD): Rejected (`Invoke Emit: Error: Non-XAH payment rejected`).
- **Note**: Ensure the Hook account has sufficient XAH balance for emission. Set `LOCK` to secure state changes, requiring `PASS` for updates.

## Testing
1. **Setup Accounts**:
   - Create a Hook account (e.g., `rTest123...`) and a second account (e.g., `rTest456...`) using the Testnet Faucet.
   - Create additional accounts for emission targets (e.g., `rTest789...`, `rTestABC...`) for Hooks 011–014.
2. **Deploy Hooks**:
   - Use Xahau Hooks Builder or XRPLWin Hook Management to deploy Hooks to `rTest123...` for `ttPayment` (and `ttINVOKE` for Hook 015).
   - For Hooks 013–014, set hook parameters (`AM`, `AO`, `A1`, `A2`) during deployment.
   - For Hook 015, send `ttINVOKE` transactions to configure `AM`, `AO`, `A1`, and `LOCK`.
3. **Send Transactions**:
   - XAH Payment: 10, 15 XAH from `rTest456...` to `rTest123...` (incoming) via XRPLWin Tools.
   - IOU Payment: 100 USD from `rTest456...` to `rTest123...` (incoming).
   - Outgoing Payment: 50 XAH from `rTest123...` to `rTest456...`.
   - For Hook 015, `ttINVOKE` transactions to set parameters or test locked state.
4. **Verify**:
   - Check transaction status and emitted transactions in Xahau Explorer.
   - View logs in Hooks Builder or XRPLWin Tools for `TRACESTR`/`TRACEHEX`/`TRACEVAR`.

## Debugging Tips
- **Logs**: Use `TRACESTR` (e.g., `Invoke Emit: Payment received and forwarded successfully`), `TRACEHEX` (for accounts), and `TRACEVAR` (for amounts) to trace execution.
- **Xahau Explorer**: Verify `TransactionType`, `Amount`, `HookHash`, and emitted transactions (`HookEmissions`).
- **Common Errors**:
  - `Execution failure (no exit type specified)`: Indicates invalid `sfAmount` access. Use a 48-byte buffer and check `otxn_field` return values.
  - Emission failure: Ensure sufficient XAH balance (e.g., ≥ 5 XAH for Hook 011, ≥ 10 XAH for Hook 012).
  - Hook parameter errors (Hooks 013–014): Validate `AM`, `AO`, `A1`, `A2` formats using Hooks Services Tools.
  - State errors (Hook 015): Ensure `AM`, `AO`, `A1` are set via `ttINVOKE` before processing payments.
  - Lock errors (Hook 015): Provide correct `PASS` parameter to unlock state changes.
- **Testnet Node**: Use `wss://testnet.xahau.network` via Hooks Builder or XRPLWin.

## Lessons Learned
- **Emission to Hardcoded Accounts**: Hooks 011–012 use hardcoded accounts (`ftxn_acc`, `ftxn_acc1`, `ftxn_acc2`), requiring code edits for changes (consider hook parameters for flexibility).
- **Hook Parameters**: Hooks 013–014 use `hook_param` for dynamic configuration (`AM`, `AO`, `A1`, `A2`), improving flexibility over hardcoded values.
- **State-Based Configuration**: Hook 015 uses state storage (`state_set`, `state`) and `ttINVOKE` for secure, dynamic updates, with a `LOCK`/`PASS` mechanism for access control.
- **Buffer Sizing**: Use `uint8_t amount[48]` for `sfAmount` to handle XAH (8 bytes) and IOUs (48 bytes).
- **Amount Conversion**: Use integer arithmetic (e.g., `otxn_drops == exact_amount * 1000000`) to avoid floating-point precision issues.
- **Emission Handling**: Reserve slots with `etxn_reserve` and verify emission with `emit` return value (32 bytes).
- **Testing**: Test with varied XAH amounts, IOUs, and invalid parameters to catch edge cases.

## Contributing
Fork this repo, add new Hooks, or enhance the guide! Submit issues or PRs to improve `HandyHooks`.