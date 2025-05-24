# HandyHooks - Xahau Hooks 101 (Part 2)

`Xahau Hooks 101` is a beginner-friendly collection of Xahau Hooks designed to teach transaction processing on the Xahau Network. Written in C and compiled to WebAssembly (WASM) using the [Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop) starter template, these Hooks handle Payment transactions (`ttPayment`) with advanced logic for filtering by amount thresholds or emitting transactions. Ideal for learning Hooks development, testing on the Xahau Testnet, or deploying to Mainnet.

## Table of Contents
- [Overview](#overview)
- [Tools](#tools)
- [Hooks](#hooks)
  - [006 - Accept Minimum](#006---accept-minimum)
  - [007 - Accept Maximum](#007---accept-maximum)
  - [008 - Accept Exact](#008---accept-exact)
  - [009 - Accept Multiple](#009---accept-multiple)
  - [010 - Accept Emit](#010---accept-emit)
- [Testing](#testing)
- [Debugging Tips](#debugging-tips)
- [Lessons Learned](#lessons-learned)
- [Contributing](#contributing)

## Overview
Xahau Hooks are WASM modules that add smart contract-like functionality to Xahau accounts. The `Xahau Hooks 101` series includes five Hooks, each demonstrating a core concept for processing Payment transactions:

1. **Accept Minimum**: Accepts outgoing Payments and incoming XAH Payments above a minimum threshold.
2. **Accept Maximum**: Accepts outgoing Payments and incoming XAH Payments below a maximum threshold.
3. **Accept Exact**: Accepts outgoing Payments and incoming XAH Payments matching an exact amount.
4. **Accept Multiple**: Accepts outgoing Payments and incoming XAH Payments that are multiples of a specified amount.
5. **Accept Emit**: Accepts outgoing Payments and incoming XAH Payments matching an exact amount, emitting a 5 XAH Payment back to the sender.

All Hooks were compiled using the Xahau Hooks Builder starter template, ensuring compatibility with Testnet and Mainnet.

## Tools
Use these online tools to compile, deploy, test, and manage Hooks—no local installations required:

- **[Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop)**: Web-based IDE for writing, compiling, and deploying Hooks. Use the starter template to compile each `HandyHooks` file.
- **[XRPLWin Xahau Testnet Tools](https://xahau-testnet.xrplwin.com/tools)**: Suite of tools for interacting with the Xahau Testnet, including transaction creation and Hook testing.
- **[XRPLWin Hook Management](https://xahau-testnet.xrplwin.com/account/YOU_WALLET_RADDRESS_HERE/manage/hooks)**: Manage Hooks on your Testnet account (replace `YOU_WALLET_RADDRESS_HERE` with your account, e.g., `rTest123...`).
- **[Xahau Testnet Faucet](https://xahau-test.net/faucet)**: Fund Testnet accounts for testing.
- **[Xahau Explorer](https://test.xahauexplorer.com/en)**: Check transactions and Hook details.
- **[Xahau Mainnet Explorer](https://xahau.xrplwin.com/)**: Check transactions and Hook details.

## Hooks
Each Hook is installed on `ttPayment` and processes Payment transactions. These Hooks were compiled using the Xahau Hooks Builder starter template and are Mainnet-ready upon validation.

It's recommended that you verify the functionality of any Hook on Testnet before installing on Mainnet due to known security issues with smart contracts.

### 006 - Accept Minimum
- **File**: `006_AcceptMin.c`
- **Purpose**: Accepts outgoing Payments and incoming XAH Payments above a minimum threshold (10 XAH).
- **Logic**: Accepts if Hook account matches originating account (outgoing) or if incoming XAH amount is ≥ 10 XAH, rejects otherwise.
- **Hook Parameters**: None (future: configurable `min_amount_value`).
- **Installation**:
  - Open `006_AcceptMin.c` in Xahau Hooks Builder.
  - Compile using the starter template.
  - Deploy to a Testnet account via Hooks Builder or XRPLWin Hook Management.
- **Test Cases**:
  - Incoming XAH Payment (15 XAH): Accepted (`Accept Minimum: Incoming XAH payment accepted`).
  - Incoming XAH Payment (5 XAH): Rejected (`Accept Minimum: Error: Payment amount is less than the minimum required`).
  - Outgoing XAH Payment (50 XAH): Accepted (`Accept Minimum: Outgoing payment transaction accepted`).
  - Incoming IOU Payment (100 USD): Rejected (`Accept Minimum: Error: Non-XAH payment rejected`).

### 007 - Accept Maximum
- **File**: `007_AcceptMax.c`
- **Purpose**: Accepts outgoing Payments and incoming XAH Payments below a maximum threshold (10 XAH).
- **Logic**: Accepts if Hook account matches originating account (outgoing) or if incoming XAH amount is ≤ 10 XAH, rejects otherwise.
- **Hook Parameters**: None (future: configurable `max_amount_value`).
- **Installation**:
  - Load `007_AcceptMax.c` in Hooks Builder.
  - Compile and deploy via Hooks Builder or XRPLWin.
- **Test Cases**:
  - Incoming XAH Payment (5 XAH): Accepted (`Accept Maximum: Incoming XAH payment accepted`).
  - Incoming XAH Payment (15 XAH): Rejected (`Accept Maximum: Error: Payment amount is greater than the maximum allowed`).
  - Outgoing XAH Payment (50 XAH): Accepted (`Accept Maximum: Outgoing payment transaction accepted`).
  - Incoming IOU Payment (100 USD): Rejected (`Accept Maximum: Error: Non-XAH payment rejected`).

### 008 - Accept Exact
- **File**: `008_AcceptExact.c`
- **Purpose**: Accepts outgoing Payments and incoming XAH Payments matching an exact amount (10 XAH).
- **Logic**: Accepts if Hook account matches originating account (outgoing) or if incoming XAH amount equals 10 XAH, rejects otherwise.
- **Hook Parameters**: None (future: configurable `exact_amount_value`).
- **Installation**:
  - Compile `008_AcceptExact.c` in Hooks Builder and deploy.
- **Test Cases**:
  - Incoming XAH Payment (10 XAH): Accepted (`Accept Exact: Incoming XAH payment accepted`).
  - Incoming XAH Payment (15 XAH): Rejected (`Accept Exact: Error: Payment amount doesn't match the exact_amount_value`).
  - Outgoing XAH Payment (50 XAH): Accepted (`Accept Exact: Outgoing payment transaction accepted`).
  - Incoming IOU Payment (100 USD): Rejected (`Accept Exact: Error: Non-XAH payment rejected`).

### 009 - Accept Multiple
- **File**: `009_AcceptMulti.c`
- **Purpose**: Accepts outgoing Payments and incoming XAH Payments that are exact multiples of a specified amount (10 XAH).
- **Logic**: Accepts if Hook account matches originating account (outgoing) or if incoming XAH amount is a multiple of 10 XAH, rejects otherwise.
- **Hook Parameters**: None (future: configurable `cost_xah`).
- **Installation**:
  - Compile `009_AcceptMulti.c` in Hooks Builder and deploy via Hooks Builder or XRPLWin Hook Management.
- **Test Cases**:
  - Incoming XAH Payment (20 XAH): Accepted (`Accept Multiple: Incoming XAH payment accepted`).
  - Incoming XAH Payment (15 XAH): Rejected (`Accept Multiple: Error: Payment amount must be an exact multiple of COST!`).
  - Outgoing XAH Payment (50 XAH): Accepted (`Accept Multiple: Outgoing payment transaction accepted`).
  - Incoming IOU Payment (100 USD): Rejected (`Accept Multiple: Error: Non-XAH payment rejected`).

### 010 - Accept Emit
- **File**: `010_AcceptEmit.c`
- **Purpose**: Accepts outgoing Payments and incoming XAH Payments matching an exact amount (10 XAH), emitting a 5 XAH Payment back to the sender.
- **Logic**: Accepts if Hook account matches originating account (outgoing) or if incoming XAH amount equals 10 XAH, then emits a 5 XAH Payment to the sender; rejects otherwise.
- **Hook Parameters**: None (future: configurable `exact_amount_value`, `amountOut`).
- **Installation**:
  - Compile `010_AcceptEmit.c` in Hooks Builder and deploy via Hooks Builder or XRPLWin Hook Management.
- **Test Cases**:
  - Incoming XAH Payment (10 XAH): Accepted, emits 5 XAH back (`Accept Emit: Payment received and forwarded Successfully`).
  - Incoming XAH Payment (15 XAH): Rejected (`Accept Emit: Error: Payment amount doesn't match the exact_amount_value`).
  - Outgoing XAH Payment (50 XAH): Accepted (`Accept Emit: Outgoing payment transaction accepted`).
  - Incoming IOU Payment (100 USD): Rejected (`Accept Emit: Error: Non-XAH payment rejected`).
- **Note**: Ensure sufficient XAH balance in the Hook account to emit the 5 XAH Payment. Failed emissions trigger `Accept Emit: Error: Failed to emit transactions`.

## Testing
1. **Setup Accounts**:
   - Create a Hook account (e.g., `rTest123...`) and a second account (e.g., `rTest456...`) using the Testnet Faucet.
2. **Deploy Hooks**:
   - Use Xahau Hooks Builder or XRPLWin Hook Management to deploy each Hook to `rTest123...` for `ttPayment`.
3. **Send Transactions**:
   - XAH Payment: 5, 10, 15, or 20 XAH from `rTest456...` to `rTest123...` (incoming) via XRPLWin Tools.
   - IOU Payment: 100 USD from `rTest456...` to `rTest123...` (incoming).
   - Outgoing Payment: 50 XAH from `rTest123...` to `rTest456...`.
4. **Verify**:
   - Check transaction status and emitted transactions (for Hook 010) in Xahau Explorer.
   - View logs in Hooks Builder or XRPLWin Tools for `TRACESTR`/`TRACEHEX`.

## Debugging Tips
- **Logs**: Use `TRACESTR` for messages (e.g., `Accept Minimum: Incoming XAH payment accepted`) and `TRACEHEX` for account/amount data.
- **Xahau Explorer**: Check `TransactionType`, `Amount`, and `HookHash` to verify Hook behavior.
- **Common Errors**:
  - `Execution failure (no exit type specified)`: Indicates invalid `sfAmount` access. Use a 48-byte buffer and check `otxn_field` return values.
  - Non-Payment transactions: Ensure Hooks are set for `ttPayment` in Hooks Builder or XRPLWin.
  - Emission failure (Hook 010): Ensure Hook account has sufficient XAH balance for emitting 5 XAH.
- **Testnet Node**: Use `wss://testnet.xahau.network` via Hooks Builder or XRPLWin.

## Lessons Learned
- **Buffer Sizing**: Use `uint8_t amount[48]` for `sfAmount` to handle XAH (8 bytes) and IOUs (48 bytes).
- **Amount Conversion**: Convert drops to XAH using `(double)otxn_drops / 1000000.0` for accurate comparisons.
- **Emission Handling**: Reserve emission slots with `etxn_reserve` and verify emission success with `emit` return value (Hook 010).
- **Error Handling**: Check `otxn_field` and `amount_len` to avoid errors with non-XAH Payments.
- **Testing**: Test with varied XAH amounts (e.g., 5, 10, 15, 20 XAH) and IOUs to catch edge cases.

## Contributing
Fork this repo, add new Hooks, or enhance the guide! Submit issues or PRs to improve `HandyHooks`.