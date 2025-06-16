# HandyHooks - Xahau Hooks 101 (Part 6)

`Xahau Hooks 101` is a beginner-friendly collection of Xahau Hooks designed to teach transaction processing on the Xahau Network. Written in C and compiled to WebAssembly (WASM) using the [Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop) starter template, these Hooks focus on controlling Payment transactions (`ttPayment`) and, where applicable, Invoke transactions (`ttINVOKE`) with logic based on transaction fees, amount restrictions, and state configurations. Ideal for learning Hooks development, testing on the Xahau Testnet, or deploying to Mainnet.

## Table of Contents
- [Overview](#overview)
- [Tools](#tools)
- [Hooks](#hooks)
  - [026 - Maximum Out](#026---maximum-out)
  - [027 - Maximum Out with Lock](#027---maximum-out-with-lock)
  - [028 - Fee Logger](#028---fee-logger)
  - [029 - Max Fee](#029---max-fee)
  - [030 - Fee Reflect](#030---fee-reflect)
- [Testing](#testing)
- [Debugging Tips](#debugging-tips)
- [Lessons Learned](#lessons-learned)
- [Contributing](#contributing)

## Overview
Xahau Hooks are WASM modules that add smart contract-like functionality to Xahau accounts. The `Xahau Hooks 101` series (Part 6) includes five Hooks, each demonstrating advanced transaction processing for Payment transactions and, in some cases, Invoke transactions:

1. **Maximum Out**: Restricts outgoing Payments to a maximum of 1 XAH when enabled via a state parameter (`MAX`), configurable through `ttINVOKE` transactions.
2. **Maximum Out with Lock**: Restricts outgoing Payments to a configurable maximum amount (`AMT`) when enabled (`MAX`), with a bypass option via a `PASS` parameter, set via `ttINVOKE`.
3. **Fee Logger**: Logs the transaction fee (`sfFee`) in drops for outgoing Payments using `otxn_field` and `XRP_AMOUNT_TO_DROPS`.
4. **Max Fee**: Limits the fee of outgoing Payments to a configurable maximum (`MF`) when fee checking is enabled (`FE`), set via `ttINVOKE`.
5. **Fee Reflect**: For incoming Payments, emits a Payment back to the sender with an amount equal to the transaction’s fee (`sfFee`) in drops.

All Hooks were compiled using the Xahau Hooks Builder starter template, ensuring compatibility with Testnet and Mainnet.

## Tools
Use these online tools to compile, deploy, test, and manage Hooks—no local installations required:

- **[Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop)**: Web-based IDE for writing, compiling, and deploying Hooks. Use the starter template to compile each `HandyHooks` file.
- **[XRPLWin Xahau Testnet Tools](https://xahau-testnet.xrplwin.com/tools)**: Suite of tools for interacting with the Xahau Testnet, including transaction creation and Hook testing.
- **[XRPLWin Hook Management](https://xahau-testnet.xrplwin.com/account/YOU_WALLET_RADDRESS_HERE/manage/hooks)**: Manage Hooks on your Testnet account (replace `YOU_WALLET_RADDRESS_HERE` with your account, e.g., `rTest123...`).
- **[Hooks Services Tools](https://hooks.services/tools)**: Convert decimal to hex and rAddress to Account ID.
- **[XRPL Hex Visualizer](https://transia-rnd.github.io/xrpl-hex-visualizer/)**: Convert decimal to uint64 for `ttINVOKE` parameters (Hooks 026, 027, 029).
- **[Xahau Testnet Faucet](https://xahau-test.net/faucet)**: Fund Testnet accounts for testing.
- **[Xahau Testnet Explorer](https://test.xahauexplorer.com/en)**: Check transactions and Hook details.
- **[Xahau Mainnet Explorer](https://xahau.xrplwin.com/)**: Check Mainnet transactions.
- **[Xahau Explorer Mainnet](https://xahauexplorer.com/en)**: Check Mainnet transactions.
- **[XRPLWin Xahau Mainnet](https://xahau.xrplwin.com/)**: Deploy and manage Mainnet Hooks.

## Hooks
Each Hook is installed on `ttPayment` (and `ttINVOKE` for Hooks 026, 027, and 029) and processes transactions according to its logic. These Hooks were compiled using the Xahau Hooks Builder starter template and are Mainnet-ready upon validation.

It's recommended that you verify the functionality of any Hook on Testnet before installing on Mainnet due to known security issues with smart contracts.

### 026 - Maximum Out

- **File**: `026_MaxOut.c`
- **Purpose**: Restricts outgoing Payments to a maximum of 1 XAH when enabled.
- **Logic**: For `ttINVOKE`, allows the hook owner to toggle `MAX` (0 or 1) in state. For `ttPAYMENT`, accepts incoming Payments and restricts outgoing Payments to ≤ 1 XAH if `MAX=1`.

- **Installation**:
  - Open `026_MaxOut.c` in Xahau Hooks Builder.
  - Compile using the starter template.
  - Deploy to a Testnet account for `ttPayment` and `ttINVOKE` via Hooks Builder or XRPLWin.

- **Test Cases**:
  - `ttINVOKE` (owner, set `MAX=1`): Accepted (`Maximum Out Hook: MAX state set successfully`).
  - `ttINVOKE` (non-owner): Rejected (`Maximum Out Hook: Error: Only hook owner can change settings`).
  - Outgoing Payment (> 1 XAH, `MAX=1`): Rejected (`Maximum Out Hook: Payment above maximum amount`).
  - Outgoing Payment (≤ 1 XAH, `MAX=1`): Accepted (`Maximum Out Hook: Outgoing payment accepted`).
  - Incoming Payment: Accepted (`Maximum Out Hook: Incoming payment accepted`).

- **Note**: Ensure the Hook account has sufficient XAH balance for transaction fees.

### 027 - Maximum Out with Lock

- **File**: `027_MaxOutLock.c`
- **Purpose**: Restricts outgoing Payments to a configurable maximum amount with a bypass option.
- **Logic**: For `ttINVOKE`, allows the hook owner to set `MAX` (0 or 1) and `AMT` (maximum amount in XAH, converted to drops). For `ttPAYMENT`, accepts incoming Payments and restricts outgoing Payments to ≤ `AMT` if `MAX=1`, unless a valid `PASS` parameter is provided.

- **Hook Parameters** (for `ttINVOKE`):
  - `MAX`: Toggle maximum amount check (1-byte, `00` or `01`).
  - `AMT`: Maximum amount in XAH (8-byte, uint64).
  - `PASS`: Bypass restriction (1-byte, `01`) for outgoing Payments.

- **Installation**:
  - Compile `027_MaxOutLock.c` in Hooks Builder for `ttPayment` and `ttINVOKE`.
  - Deploy to a Testnet account.
  - Send `ttINVOKE` transactions to set `MAX` and `AMT`.

- **Test Cases**:
  - `ttINVOKE` (owner, set `MAX=1`, `AMT=2`): Accepted (`MaxOut Hook: MAX/AMT state set successfully`).
  - `ttINVOKE` (non-owner): Rejected (`MaxOut Hook: Error: Only hook owner can change settings`).
  - Outgoing Payment (> 2 XAH, `MAX=1`, no `PASS`): Rejected (`MaxOut Hook: Payment above maximum amount and no valid PASS parameter`).
  - Outgoing Payment (> 2 XAH, `MAX=1`, with `PASS=1`): Accepted (`MaxOut Hook: PASS parameter provided, bypassing max amount check`).
  - Incoming Payment: Accepted (`MaxOut Hook: Incoming payment accepted`).

- **Note**: Use [XRPL Hex Visualizer](https://transia-rnd.github.io/xrpl-hex-visualizer/) for `AMT` conversions. Ensure sufficient XAH balance.

### 028 - Fee Logger

- **File**: `028_FeeLogger.c`
- **Purpose**: Logs the transaction fee (`sfFee`) in drops for outgoing Payments.
- **Logic**: For `ttPAYMENT`, logs `sfFee` in drops using `otxn_field` and `XRP_AMOUNT_TO_DROPS` if the transaction originates from the Hook account; skips incoming Payments.

- **Installation**:
  - Compile `028_FeeLogger.c` in Hooks Builder.
  - Deploy to a Testnet account for `ttPayment`.

- **Test Cases**:
  - Outgoing Payment: Accepted, fee logged (`Fee Logger Hook: Outgoing payment fee logged`).
  - Incoming Payment: Accepted, no logging (`Fee Logger Hook: Incoming payment, skipping`).
  - Non-Payment Transaction: Accepted (`Fee Logger Hook: Non-payment transaction`).

- **Note**: Ensure the Hook account has sufficient XAH balance for transaction fees.

### 029 - Max Fee

- **File**: `029_FeeManager.c`
- **Purpose**: Limits the fee of outgoing Payments to a configurable maximum.
- **Logic**: For `ttINVOKE`, allows the hook owner to set `MF` (maximum fee in drops) and `FE` (fee check enabled, 0 or 1). For `ttPAYMENT`, accepts incoming Payments and restricts outgoing Payments’ fees to ≤ `MF` if `FE=1`.

- **Hook Parameters** (for `ttINVOKE`):
  - `MF`: Maximum fee in drops (8-byte, uint64).
  - `FE`: Toggle fee check (1-byte, `00` or `01`).

- **Installation**:
  - Compile `029_FeeManager.c` in Hooks Builder for `ttPayment` and `ttINVOKE`.
  - Deploy to a Testnet account.
  - Send `ttINVOKE` transactions to set `MF` and `FE`.

- **Test Cases**:
  - `ttINVOKE` (owner, set `MF=1000`, `FE=1`): Accepted (`Max Fee Hook: MF/FE state set successfully`).
  - `ttINVOKE` (non-owner): Rejected (`Max Fee Hook: Error: Only hook owner can change settings`).
  - Outgoing Payment (fee > 1000 drops, `FE=1`): Rejected (`Max Fee Hook: Error: Fee exceeds maximum allowed`).
  - Outgoing Payment (fee ≤ 1000 drops, `FE=1`): Accepted (`Max Fee Hook: Outgoing payment fee within limit`).
  - Incoming Payment: Accepted (`Max Fee Hook: Incoming payment, skipping`).

- **Note**: Use [XRPL Hex Visualizer](https://transia-rnd.github.io/xrpl-hex-visualizer/) for `MF` conversions. Ensure sufficient XAH balance.

### 030 - Fee Reflect

- **File**: `030_FeeReturn.c`
- **Purpose**: Emits a Payment back to the sender of an incoming Payment, with the amount equal to the transaction’s fee.
- **Logic**: For `ttPAYMENT`, if incoming, emits a Payment to the origin account with an amount equal to `sfFee` in drops using `otxn_field` and `XRP_AMOUNT_TO_DROPS`. Outgoing Payments are accepted without action.

- **Installation**:
  - Compile `030_FeeReturn.c` in Hooks Builder.
  - Deploy to a Testnet account for `ttPayment`.

- **Test Cases**:
  - Incoming Payment: Accepted, emits Payment with fee amount (`Fee Reflect Hook: Incoming payment processed, fee reflected`).
  - Outgoing Payment: Accepted (`Fee Reflect Hook: Outgoing payment, skipping`).
  - Non-Payment Transaction: Accepted (`Fee Reflect Hook: Non-payment transaction`).

- **Note**: Ensure the Hook account has sufficient XAH balance for transaction fees and emitted Payments.

## Testing
1. **Setup Accounts**:
   - Create a Hook account (e.g., `rTest123...`) and a second account (e.g., `rTest456...`) using the Testnet Faucet.
2. **Deploy Hooks**:
   - Use Xahau Hooks Builder or XRPLWin Hook Management to deploy Hooks to `rTest123...` for `ttPayment` (and `ttINVOKE` for Hooks 026, 027, 029).
   - For Hooks 026, 027, and 029, send `ttINVOKE` transactions to configure state parameters.
3. **Send Transactions**:
   - Incoming Payment (e.g., 0.5, 2 XAH) from `rTest456...` to `rTest123...` via XRPLWin Tools.
   - Outgoing Payment (e.g., 0.5, 2 XAH) from `rTest123...` to `rTest456...` with varying fees or `PASS` parameters.
   - For Hooks 026, 027, and 029, `ttINVOKE` transactions to set parameters or test non-owner access.
4. **Verify**:
   - Check transaction status in Xahau Testnet Explorer.
   - View logs in Hooks Builder or XRPLWin Tools for `TRACESTR`/`TRACEHEX`/`TRACEVAR`.

## Debugging Tips
- **Logs**: Use `TRACESTR` (e.g., `Max Fee Hook: Outgoing payment fee within limit`), `TRACEHEX` (for accounts), and `TRACEVAR` (for state values or fees) to trace execution.
- **Xahau Explorer**: Verify `TransactionType`, `HookParameters`, `Amount`, and `Fee`.
- **Common Errors**:
  - `Execution failure (no exit type specified)`: Indicates invalid field access (e.g., `sfFee`, `sfAmount`). Use appropriate buffer sizes (e.g., 8 bytes for `sfFee`).
  - State errors (Hooks 026, 027, 029): Ensure state parameters are set via `ttINVOKE` before processing Payments.
  - Invalid parameters: Ensure `MAX`, `FE`, `MF`, `AMT`, and `PASS` are valid (e.g., `00` or `01` for 1-byte parameters).
  - Insufficient balance: Ensure the Hook account has enough XAH for fees and emitted Payments (Hook 030).
  - Emit failure (Hook 030): Verify `etxn_reserve` and `PREPARE_PAYMENT_SIMPLE` parameters.
- **Testnet Node**: Use `wss://testnet.xahau.network` via Hooks Builder or XRPLWin.

## Lessons Learned
- **Fee Handling**: Hooks 028, 029, and 030 demonstrate robust fee processing using `otxn_field` and `XRP_AMOUNT_TO_DROPS` for accurate fee extraction and manipulation.
- **State-Based Configuration**: Hooks 026, 027, and 029 use `state_set` and `state` with `ttINVOKE` for dynamic parameter control, enabling flexible transaction restrictions.
- **Bypass Mechanisms**: Hook 027’s `PASS` parameter shows how to implement conditional logic for overriding restrictions securely.
- **Transaction Emission**: Hook 030 illustrates emitting transactions with `emit` and `PREPARE_PAYMENT_SIMPLE`, useful for reactive Hooks.
- **Buffer Sizing**: Use precise buffer sizes (e.g., 8 bytes for `sfFee`, 48 bytes for `sfAmount`) to avoid field access errors.
- **Guard Macro**: Use `_g(1,1)` or `GUARD(1)` to prevent reentrancy and ensure safe execution.
- **Testing**: Test with varied XAH amounts, fees, parameters, and invalid `ttINVOKE` inputs to catch edge cases.

## Contributing
Fork this repo, add new Hooks, or enhance the guide! Submit issues or PRs to improve `HandyHooks`.