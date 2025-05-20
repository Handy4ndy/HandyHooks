# HandyHooks - Xahau Hooks 101

`Xahau Hooks 101` is a beginner-friendly collection of Xahau Hooks designed to teach transaction processing on the Xahau Network. Written in C and compiled to WebAssembly (WASM) using the [Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop) starter template, these Hooks handle Payment transactions (`ttPayment`) to accept or reject based on direction (incoming/outgoing) or amount type (XAH). Ideal for learning Hooks development, testing on the Xahau Testnet, or deploying to Mainnet.

## Table of Contents
- [Overview](#overview)
- [Tools](#tools)
- [Hooks](#hooks)
  - [001 - Accept All](#001---accept-all)
  - [002 - Reject All](#002---reject-all)
  - [003 - Accept In](#003---accept-in)
  - [004 - Accept Out](#004---accept-out)
  - [005 - Accept XAH](#005---accept-xah)
- [Testing](#testing)
- [Debugging Tips](#debugging-tips)
- [Lessons Learned](#lessons-learned)
- [Contributing](#contributing)

## Overview
Xahau Hooks are WASM modules that add smart contract-like functionality to Xahau accounts. The `Xahau Hooks 101` series currently includes five Hooks, each demonstrating a core concept:

1. **Accept All**: Accepts all Payment transactions.
2. **Reject All**: Rejects all Payment transactions.
3. **Accept In**: Accepts incoming Payments, rejects outgoing.
4. **Accept Out**: Accepts outgoing Payments, rejects incoming.
5. **Accept XAH**: Accepts outgoing Payments and incoming XAH Payments.

All Hooks were compiled using the Xahau Hooks Builder starter template, ensuring compatibility with Testnet and Mainnet.

## Tools
Use these online tools to compile, deploy, test, and manage Hooks—no with local installations required:

- **[Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop)**: Web-based IDE for writing, compiling, and deploying Hooks. Use the starter template to compile each `HandyHooks` file.
- **[XRPLWin Xahau Testnet Tools](https://xahau-testnet.xrplwin.com/tools)**: Suite of tools for interacting with the Xahau Testnet, including transaction creation and Hook testing.
- **[XRPLWin Hook Management](https://xahau-testnet.xrplwin.com/account/YOU_WALLET_RADDRESS_HERE/manage/hooks)**: Manage Hooks on your Testnet account (replace `YOU_WALLET_RADDRESS_HERE` with your account, e.g., `rTest123...`).
- **[Xahau Testnet Faucet](https://xahau-test.net/faucet)**: Fund Testnet accounts for testing.
- **[Xahau Explorer](https://test.xahauexplorer.com/)**: Verify transactions, Hook installation, and `HookHash`.

## Hooks

Each Hook is installed on `ttPayment` and processes Payment transactions. These Hooks were compiled using the Xahau Hooks Builder starter template and upon validation are Mainnet-ready. 

It's recommended that you verify the functionality of any hook on testnet before installing them on mainnet due to known security issues with smart contracts.

### 001 - Accept All
- **File**: `001_AcceptAll.c`
- **Purpose**: Accepts all Payment transactions (incoming and outgoing).
- **Logic**: Retrieves Hook and originating accounts, accepts both with `accept`.
- **Hook Parameters**: None.
- **Installation**:
  - Open `001_AcceptAll.c` in Xahau Hooks Builder.
  - Compile using the starter template.
  - Deploy to a Testnet account via Hooks Builder or XRPLWin Hook Management.
- **Test Cases**:
  - Incoming XAH Payment (50 XAH): Accepted (`Accept All: Incoming payment transaction accepted`).
  - Outgoing XAH Payment (50 XAH): Accepted (`Accept All: Outgoing payment transaction accepted`).
  - Incoming IOU Payment (100 USD): Accepted.

### 002 - Reject All
- **File**: `002_RejectAll.c`
- **Purpose**: Rejects all Payment transactions.
- **Logic**: Rejects both incoming and outgoing with `rollback`.
- **Hook Parameters**: None.
- **Installation**:
  - Load `002_RejectAll.c` in Hooks Builder.
  - Compile and deploy via Hooks Builder or XRPLWin.
- **Test Cases**:
  - Incoming XAH Payment: Rejected (`Reject All: Incoming payment transaction rejected`).
  - Outgoing XAH Payment: Rejected (`Reject All: Outgoing payment transaction rejected`).

### 003 - Accept In
- **File**: `003_AcceptIn.c`
- **Purpose**: Accepts incoming Payments, rejects outgoing.
- **Logic**: Accepts if Hook account differs from originating account, rejects otherwise.
- **Hook Parameters**: None.
- **Installation**:
  - Compile `003_AcceptIn.c` in Hooks Builder and deploy via Hooks Builder or XRPLWin.
- **Test Cases**:
  - Incoming XAH Payment: Accepted (`Accept In: Incoming payment transaction accepted`).
  - Outgoing XAH Payment: Rejected (`Accept In: Outgoing payment transaction rejected`).

### 004 - Accept Out
- **File**: `004_AcceptOut.c`
- **Purpose**: Accepts outgoing Payments, rejects incoming.
- **Logic**: Accepts if Hook account matches originating account, rejects otherwise.
- **Hook Parameters**: None.
- **Installation**:
  - Compile `004_AcceptOut.c` in Hooks Builder and deploy.
- **Test Cases**:
  - Incoming XAH Payment: Rejected (`Accept OUT: Incoming payment transaction rejected`).
  - Outgoing XAH Payment: Accepted (`Accept OUT: Outgoing payment transaction accepted`).

### 005 - Accept XAH
- **File**: `005_AcceptXah.c`
- **Purpose**: Accepts outgoing Payments and incoming XAH Payments, rejects non-XAH incoming Payments.
- **Logic**: Accepts outgoing transactions; for incoming, checks if `sfAmount` is 8 bytes (XAH).
- **Hook Parameters**: None (future: `MIN_XAH` for minimum amount).
- **Installation**:
  - Compile `005_AcceptXah.c` in Hooks Builder and deploy via Hooks Builder or XRPLWin Hook Management.
- **Test Cases**:
  - Outgoing XAH Payment: Accepted (`Accept XAH: Outgoing payment transaction accepted`).
  - Incoming XAH Payment (50 XAH): Accepted (`Accept XAH: Incoming XAH payment accepted`).
  - Incoming IOU Payment (100 USD): Rejected (`Accept XAH: Non-XAH Payment rejected`).
- **Note**: Early versions failed with `HookError: Execution failure (no exit type specified)` due to a small `sfAmount` buffer (8 bytes). Fixed with a 48-byte buffer and error handling.

## Testing
1. **Setup Accounts**:
   - Create a Hook account (e.g., `rTest123...`) and a second account (e.g., `rTest456...`) using the Testnet Faucet.
2. **Deploy Hooks**:
   - Use Xahau Hooks Builder or XRPLWin Hook Management to deploy each Hook to `rTest123...` for `ttPayment`.
3. **Send Transactions**:
   - XAH Payment: 50 XAH from `rTest456...` to `rTest123...` (incoming) via XRPLWin Tools.
   - IOU Payment: 100 USD from `rTest456...` to `rTest123...` (incoming).
   - Outgoing Payment: 50 XAH from `rTest123...` to `rTest456...`.
4. **Verify**:
   - Check transaction status in Xahau Explorer.
   - View logs in Hooks Builder or XRPLWin Tools for `TRACESTR`/`TRACEHEX`.

## Debugging Tips
- **Logs**: Use `TRACESTR` for messages and `TRACEHEX` for account/amount data.
- **Xahau Explorer**: Check `TransactionType`, `Amount`, and `HookHash`.
- **Common Errors**:
  - `Execution failure (no exit type specified)`: Indicates invalid `sfAmount` access. Use a 48-byte buffer and check `otxn_field` return values.
  - Non-Payment transactions: Ensure Hooks are set for `ttPayment` in Hooks Builder or XRPLWin.
- **Testnet Node**: Use `wss://testnet.xahau.network` via Hooks Builder or XRPLWin.

## Lessons Learned
- **Buffer Sizing**: Use `uint8_t amount[48]` for `sfAmount` to handle XAH (8 bytes) and IOUs (48 bytes).
- **Error Handling**: Check `otxn_field` for errors (e.g., `< 0`).
- **Online Tools**: Hooks Builder and XRPLWin simplify compilation, deployment, and management, ensuring Mainnet compatibility.
- **Testing**: Test with XAH, IOU, and outgoing Payments to catch edge cases.

## Contributing
Fork this repo, add new Hooks, or enhance the guide! Submit issues or PRs to improve `HandyHooks`.