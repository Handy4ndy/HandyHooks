# HandyHooks - Xahau Hooks 101 (Part 5)

`Xahau Hooks 101` is a beginner-friendly collection of Xahau Hooks designed to teach transaction processing on the Xahau Network. Written in C and compiled to WebAssembly (WASM) using the [Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop) starter template, these Hooks focus on controlling Payment transactions (`ttPayment`) with logic based on hook parameters, memos, and amount restrictions. Ideal for learning Hooks development, testing on the Xahau Testnet, or deploying to Mainnet.

## Table of Contents
- [Overview](#overview)
- [Tools](#tools)
- [Hooks](#hooks)
  - [021 - Require Hook Parameter](#021---require-hook-parameter)
  - [022 - Reject Hook Parameter](#022---reject-hook-parameter)
  - [023 - Require Memo](#023---require-memo)
  - [024 - Reject Memo](#024---reject-memo)
  - [025 - Spam Prevent](#025---spam-prevent)
- [Testing](#testing)
- [Debugging Tips](#debugging-tips)
- [Lessons Learned](#lessons-learned)
- [Contributing](#contributing)

## Overview
Xahau Hooks are WASM modules that add smart contract-like functionality to Xahau accounts. The `Xahau Hooks 101` series (Part 4) includes five Hooks, each demonstrating advanced transaction filtering for Payment transactions:

1. **Require Hook Parameter**: Accepts outgoing Payments and incoming Payments with hook parameters; rejects incoming Payments without hook parameters.
2. **Reject Hook Parameter**: Accepts outgoing Payments and incoming Payments without hook parameters; rejects incoming Payments with hook parameters.
3. **Require Memo**: Accepts outgoing Payments and incoming Payments with memos; rejects incoming Payments without memos.
4. **Reject Memo**: Accepts outgoing Payments and incoming Payments without memos; rejects incoming Payments with memos.
5. **Spam Prevent**: Accepts outgoing Payments and restricts incoming Payments based on configurable state parameters: minimum amount (`MIN`), hook parameters (`PAR`), and memos (`MEM`), set via `ttINVOKE` transactions.

All Hooks were compiled using the Xahau Hooks Builder starter template, ensuring compatibility with Testnet and Mainnet.

## Tools
Use these online tools to compile, deploy, test, and manage Hooks—no local installations required:

- **[Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop)**: Web-based IDE for writing, compiling, and deploying Hooks. Use the starter template to compile each `HandyHooks` file.
- **[XRPLWin Xahau Testnet Tools](https://xahau-testnet.xrplwin.com/tools)**: Suite of tools for interacting with the Xahau Testnet, including transaction creation and Hook testing.
- **[XRPLWin Hook Management](https://xahau-testnet.xrplwin.com/account/YOU_WALLET_RADDRESS_HERE/manage/hooks)**: Manage Hooks on your Testnet account (replace `YOU_WALLET_RADDRESS_HERE` with your account, e.g., `rTest123...`).
- **[Hooks Services Tools](https://hooks.services/tools)**: Convert decimal to hex and rAddress to Account ID.
- **[XRPL Hex Visualizer](https://transia-rnd.github.io/xrpl-hex-visualizer/)**: Convert decimal to uint64 for `ttINVOKE` parameters (Hook 025).
- **[Xahau Testnet Faucet](https://xahau-test.net/faucet)**: Fund Testnet accounts for testing.
- **[Xahau Testnet Explorer](https://test.xahauexplorer.com/en)**: Check transactions and Hook details.
- **[Xahau Mainnet Explorer](https://xahau.xrplwin.com/)**: Check transactions and Hook details.
- **[Xahau Explorer Mainnet](https://xahauexplorer.com/en)** : Check Mainnet transactions.
- **[XRPLWin Xahau Mainnet](https://xahau.xrplwin.com/)** : Deploy and manage mainnet Hooks.

## Hooks
Each Hook is installed on `ttPayment` (and `ttINVOKE` for Hook 025) and processes Payment transactions. These Hooks were compiled using the Xahau Hooks Builder starter template and are Mainnet-ready upon validation.

It's recommended that you verify the functionality of any Hook on Testnet before installing on Mainnet due to known security issues with smart contracts.

### 021 - Require Hook Parameter

- **File**: `021_RequireParams.c`
- **Purpose**: Rejects incoming Payments without hook parameters.
- **Logic**: Rejects if the incoming Payment has `sfHookParameters`..

- **Installation**:
  - Open `021_RequireParams.c` in Xahau Hooks Builder.
  - Compile using the starter template.
  - Deploy to a Testnet account via Hooks Builder or XRPLWin Hook Management.

- **Test Cases**:
  - Incoming Payment with hook parameters: Accepted (`Require Hook Param: Incoming payment with hook parameters accepted`).
  - Incoming Payment without hook parameters: Rejected (`Require Hook Param: Incoming payment without hook parameters rejected`).
  
- **Note**: Ensure the Hook account has sufficient XAH balance for transaction fees.

### 022 - Reject Hook Parameter

- **File**: `022_RejectParams.c`
- **Purpose**: Rejects incoming Payments with hook parameters.
- **Logic**: Accepts if the incoming Payment lacks `sfHookParameters`.

- **Installation**:
  - Open `022_RejectParams.c` in Xahau Hooks Builder.
  - Compile and deploy via Hooks Builder or XRPLWin.

- **Test Cases**:
  - Incoming Payment without hook parameters: Accepted (`Reject Hook Param: Incoming payment without hook parameters accepted`).
  - Incoming Payment with hook parameters: Rejected (`Reject Hook Param: Incoming payment with hook parameters rejected`).

- **Note**: Ensure the Hook account has sufficient XAH balance for transaction fees.

### 023 - Require Memo

- **File**: `023_RequireMemo.c`
- **Purpose**: Requires incoming Payments to have memos.
- **Logic**: Accepts only if the incoming Payment has `sfMemos`.

- **Installation**:
  - Compile `023_RequireMemo.c` in Hooks Builder.
  - Deploy to a Testnet account via Hooks Builder or XRPLWin.

- **Test Cases**:
  - Incoming Payment with memos: Accepted (`Require Memo: Incoming payment with memos accepted`).
  - Incoming Payment without memos: Rejected (`Require Memo: Incoming payment without memos rejected`).
  - Outgoing Payment: Accepted (`Require Memo: Outgoing payment transaction accepted`).
  
- **Note**: Ensure the Hook account has sufficient XAH balance for transaction fees.

### 024 - Reject Memo

- **File**: `024_RejectMemo.c`
- **Purpose**: Rejects Incoming Payments with memos.
- **Logic**: Accepts if the Hook account matches the originating account (outgoing) or if the incoming Payment lacks `sfMemos`.

- **Installation**:
  - Compile `024_RejectMemo.c` in Hooks Builder.
  - Deploy to a Testnet account via Hooks Builder or XRPLWin.

- **Test Cases**:
  - Incoming Payment without memos: Accepted (`Reject Memo: Incoming payment without memos accepted`).
  - Incoming Payment with memos: Rejected (`Reject Memo: Incoming payment with memos rejected`).
  - Outgoing Payment: Accepted (`Reject Memo: Outgoing payment transaction accepted`).

- **Note**: Ensure the Hook account has sufficient XAH balance for transaction fees.

### 025 - Spam Prevent

- **File**: `025_SpamPrevent.c`
- **Purpose**: Restricts incoming Payments based on state-configurable parameters: minimum amount (`MIN`, 1 XAH), hook parameters (`PAR`), and memos (`MEM`), set via `ttINVOKE` transactions.

- **Logic**:
  - For `ttINVOKE`: Allows the hook owner to toggle `MIN`, `PAR`, and `MEM` (0 or 1) in state; rejects non-owner or invalid settings.
  - For `ttPAYMENT`: Accepts outgoing Payments or incoming Payments passing checks for `MIN` (≥ 1 XAH if true), `PAR` (no hook parameters if true), and `MEM` (no memos if true); rejects otherwise.

- **Hook Parameters** (for `ttINVOKE`):
  - `MIN`: Toggle minimum amount check (1-byte, `00` or `01`).
  - `PAR`: Toggle hook parameter check (1-byte, `00` or `01`).
  - `MEM`: Toggle memo check (1-byte, `00` or `01`).
  - Use [XRPL Hex Visualizer](https://transia-rnd.github.io/xrpl-hex-visualizer/) for conversions.

- **Installation**:
  - Compile `025_SpamPrevent.c` in Hooks Builder for `ttPayment` and `ttINVOKE`.
  - Deploy to a Testnet account.
  - Send `ttINVOKE` transactions to set `MIN`, `PAR`, `MEM`.

- **Test Cases**:
  - `ttINVOKE` (owner, set `MIN=1`): Accepted (`Spam Prevent Hook: MIN state set successfully`).
  - `ttINVOKE` (non-owner): Rejected (`Spam Prevent Hook: Error: Only hook owner can change settings`).
  - Incoming Payment (< 1 XAH, `MIN=1`): Rejected (`Spam Prevent Hook: Payment below minimum amount`).
  - Incoming Payment (with parameters, `PAR=1`): Rejected (`Spam Prevent Hook: Payment with hook parameter Blocked`).
  - Incoming Payment (with memos, `MEM=1`): Rejected (`Spam Prevent Hook: Payment with memo Blocked`).
  - Incoming Payment (≥ 1 XAH, no parameters/memos, `MIN=1`, `PAR=1`, `MEM=1`): Accepted (`Spam Prevent Hook: Incoming payment accepted`).

- **Note**: Ensure the Hook account has sufficient XAH balance for transaction fees. Set state parameters before testing incoming Payments.

## Testing
1. **Setup Accounts**:
   - Create a Hook account (e.g., `rTest123...`) and a second account (e.g., `rTest456...`) using the Testnet Faucet.
2. **Deploy Hooks**:
   - Use Xahau Hooks Builder or XRPLWin Hook Management to deploy Hooks to `rTest123...` for `ttPayment` (and `ttINVOKE` for Hook 025).
   - For Hook 025, send `ttINVOKE` transactions to configure `MIN`, `PAR`, `MEM`.
3. **Send Transactions**:
   - Incoming XAH Payment (e.g., 0.5, 2 XAH) from `rTest456...` to `rTest123...` with/without hook parameters or memos via XRPLWin Tools.
   - Outgoing Payment (e.g., 2 XAH) from `rTest123...` to `rTest456...`.
   - For Hook 025, `ttINVOKE` transactions to set parameters or test non-owner access.
4. **Verify**:
   - Check transaction status in Xahau Testnet Explorer.
   - View logs in Hooks Builder or XRPLWin Tools for `TRACESTR`/`TRACEHEX`/`TRACEVAR`.

## Debugging Tips
- **Logs**: Use `TRACESTR` (e.g., `Spam Prevent Hook: Incoming payment accepted`), `TRACEHEX` (for accounts), and `TRACEVAR` (for state values) to trace execution.
- **Xahau Explorer**: Verify `TransactionType`, `HookParameters`, `Memos`, and `Amount`.
- **Common Errors**:
  - `Execution failure (no exit type specified)`: Indicates invalid field access (e.g., `sfHookParameters`, `sfMemos`). Use large buffers (e.g., 2056 bytes for `sfMemos`).
  - State errors (Hook 025): Ensure `MIN`, `PAR`, `MEM` are set via `ttINVOKE` before processing Payments.
  - Invalid parameters (Hook 025): Ensure `MIN`, `PAR`, `MEM` are `00` or `01`.
  - Insufficient balance: Ensure the Hook account has enough XAH for fees.
- **Testnet Node**: Use `wss://testnet.xahau.network` via Hooks Builder or XRPLWin.

## Lessons Learned
- **Hook Parameters and Memos**: Hooks 021–024 demonstrate filtering based on `sfHookParameters` and `sfMemos`, useful for enforcing or restricting metadata in transactions.
- **State-Based Configuration**: Hook 025 uses state storage (`state_set`, `state`) and `ttINVOKE` for dynamic, secure parameter toggling, ideal for flexible transaction filtering.
- **Buffer Sizing**: Use large buffers (e.g., `uint8_t param_buf[2056]`) for `sfHookParameters` and `sfMemos` to handle variable-length data.
- **Amount Checks**: Convert `sfAmount` to drops (`AMOUNT_TO_DROPS`) and use integer arithmetic for precise comparisons (e.g., `xah_amount < 1` in Hook 025).
- **Guard Macro**: Use `_g(1,1)` or `GUARD(1)` to prevent reentrancy and ensure safe execution.
- **Testing**: Test with varied XAH amounts, hook parameters, memos, and invalid `ttINVOKE` inputs to catch edge cases.

## Contributing
Fork this repo, add new Hooks, or enhance the guide! Submit issues or PRs to improve `HandyHooks`.