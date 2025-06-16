# HandyHooks - Xahau Hooks 101 (Part 4)

`Xahau Hooks 101` is a beginner-friendly collection of Xahau Hooks designed to teach transaction processing on the Xahau Network. Written in C and compiled to WebAssembly (WASM) using the [Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop) starter template, these Hooks focus on advanced transaction handling for Payment (`ttPayment`) and Invoke (`ttINVOKE`) transactions, including multi-account payment forwarding and URI token minting. Ideal for learning Hooks development, testing on the Xahau Testnet, or deploying to Mainnet.

## Table of Contents
- [Overview](#overview)
- [Tools](#tools)
- [Hooks](#hooks)
  - [016 - Invoke Multi Emit](#016---invoke-multi-emit)
  - [017 - Invoke URI Emit](#017---invoke-uri-emit)
- [Testing](#testing)
- [Debugging Tips](#debugging-tips)
- [Lessons Learned](#lessons-learned)
- [Contributing](#contributing)
- [Work in Progress](#work-in-progress)

## Overview
Xahau Hooks are WASM modules that add smart contract-like functionality to Xahau accounts. The `Xahau Hooks 101` series (Part 7) includes two Hooks, each demonstrating advanced transaction processing:

1. **Invoke Multi Emit**: Limits incoming Payments to an exact amount and forwards a specified amount to multiple destination accounts, configured via hook parameters and state.
2. **Invoke URI Emit**: Mints a URI token for incoming Payments by emitting a Remit transaction, with URI and length configured via `ttINVOKE` transactions.

Both Hooks were compiled using the Xahau Hooks Builder starter template, ensuring compatibility with Testnet and Mainnet. Hooks 018, 019, and 020 are works in progress and will be updated soon.

## Tools
Use these online tools to compile, deploy, test, and manage Hooks—no local installations required:

- **[Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop)**: Web-based IDE for writing, compiling, and deploying Hooks. Use the starter template to compile each `HandyHooks` file.
- **[XRPLWin Xahau Testnet Tools](https://xahau-testnet.xrplwin.com/tools)**: Suite of tools for interacting with the Xahau Testnet, including transaction creation and Hook testing.
- **[XRPLWin Hook Management](https://xahau-testnet.xrplwin.com/account/YOU_WALLET_RADDRESS_HERE/manage/hooks)**: Manage Hooks on your Testnet account (replace `YOU_WALLET_RADDRESS_HERE` with your account, e.g., `rTest123...`).
- **[Hooks Services Tools](https://hooks.services/tools)**: Convert decimal to hex, flip hex, and rAddress to Account ID.
- **[XRPL Hex Visualizer](https://transia-rnd.github.io/xrpl-hex-visualizer/)**: Convert decimal to uint64 for `ttINVOKE` parameters.
- **[Xahau Testnet Faucet](https://xahau-test.net/faucet)**: Fund Testnet accounts for testing.
- **[Xahau Testnet Explorer](https://test.xahauexplorer.com/en)**: Check transactions and Hook details.
- **[Xahau Mainnet Explorer](https://xahau.xrplwin.com/)**: Check Mainnet transactions.
- **[Xahau Explorer Mainnet](https://xahauexplorer.com/en)**: Check Mainnet transactions.
- **[XRPLWin Xahau Mainnet](https://xahau.xrplwin.com/)**: Deploy and manage Mainnet Hooks.

## Hooks
Each Hook is installed on `ttPayment` (and `ttINVOKE` for Hook 017) and processes transactions according to its logic. These Hooks were compiled using the Xahau Hooks Builder starter template and are Mainnet-ready upon validation.

It's recommended that you verify the functionality of any Hook on Testnet before installing on Mainnet due to known security issues with smart contracts.

### 016 - Invoke Multi Emit

- **File**: `016_InvokeMultiEmit.c`
- **Purpose**: Limits incoming Payments to an exact amount and forwards a specified amount to multiple destination accounts.
- **Logic**: For `ttINVOKE`, allows the hook owner to set `AM` (exact incoming amount in XAH), `AO` (outgoing amount in XAH), and `A1` (destination account) with `NUM` (account index). For `ttPAYMENT`, accepts outgoing Payments and, for incoming Payments matching `AM`, emits Payments of `AO` drops to each destination account stored in state.

- **Hook Parameters** (for `ttINVOKE` or `ttPAYMENT`):
  - `AM`: Exact incoming amount in XAH (8-byte, uint64, e.g., `000000000000000A` for 10 XAH).
  - `AO`: Outgoing amount in XAH (8-byte, uint64, e.g., `0000000000000005` for 5 XAH).
  - `A1`: Destination account (20-byte, Account ID, e.g., `05A506F1996C9E045C1CE7EE7DF7E599C19A6D52`).
  - `NUM`: Number of destination accounts (8-byte, uint64, e.g., `0000000000000001`).

- **Installation**:
  - Open `016_InvokeMultiEmit.c` in Xahau Hooks Builder.
  - Compile using the starter template.
  - Deploy to a Testnet account for `ttPayment`.
  - Send `ttINVOKE` transactions to set `AM`, `AO`, `A1`, and `NUM`.

- **Test Cases**:
  - `ttINVOKE` (owner, set `AM=10`, `AO=5`, `A1`, `NUM=1`): Accepted (`Invoke Multi Emit: AM/AO/A1 state set successfully`).
  - `ttINVOKE` (non-owner): Rejected (`Invoke Multi Emit: Error: Only hook owner can change settings`).
  - Incoming Payment (10 XAH): Accepted, emits 5 XAH to each destination account (`Invoke Multi Emit: Payment received and forwarded to all accounts successfully`).
  - Incoming Payment (≠ 10 XAH): Rejected (`Invoke Multi Emit: Error: Payment amount doesn't match the exact_amount_value`).
  - Outgoing Payment: Accepted (`Invoke Multi Emit: Outgoing payment accepted`).

- **Note**: Use [Hooks Services Tools](https://hooks.services/tools) for decimal-to-hex conversions and rAddress to Account ID. Ensure sufficient XAH balance for transaction fees and emitted Payments.

### 017 - Invoke URI Emit

- **File**: `017_InvokeUriRemit.c`
- **Purpose**: Mints a URI token for incoming Payments by emitting a Remit transaction.
- **Logic**: For `ttINVOKE`, allows the hook owner to set `URIL` (URI length, uint64), `URI` (URI string, up to 256 bytes), `NUM` (URI index), `COUNT` (number of URIs), and `DEL` (delete URI state). For `ttPAYMENT`, accepts outgoing Payments and, for incoming Payments, emits a Remit transaction with the latest URI if available.

- **Hook Parameters** (for `ttINVOKE`):
  - `URIL`: URI length (8-byte, uint64, e.g., `000000000000004E` for 78 bytes).
  - `URI`: URI string (hex-encoded, e.g., `697066733A2F2F...` for an IPFS URI).
  - `NUM`: URI index (8-byte, uint64, e.g., `0000000000000001`).
  - `COUNT`: Number of URIs (8-byte, uint64).
  - `DEL`: URI index to delete (8-byte, uint64).

- **Installation**:
  - Follow the guide at [Automated NFT Airdrop Hook](https://xpert.page/handy_4ndy/blog/automated-nft-airdrop-hook) for detailed setup instructions.
  - Open `017_InvokeUriRemit.c` in Xahau Hooks Builder.
  - Compile using the starter template.
  - Deploy to a Testnet account for `ttPayment` and `ttINVOKE`.
  - Send `ttINVOKE` transactions to set `URIL`, `URI`, `NUM`, and `COUNT`.

- **Test Cases**:
  - `ttINVOKE` (owner, set `URIL=78`, `URI`, `NUM=1`, `COUNT=1`): Accepted (`Success: Set the URIL/URI/COUNT state`).
  - `ttINVOKE` (non-owner): Rejected (`Error: Only the owner of this hook can change its settings!`).
  - `ttINVOKE` (owner, `DEL=1`): Accepted, deletes URI state (`Success: Deleted the state`).
  - Incoming Payment (XAH): Accepted, emits Remit transaction with URI (`Success: Tx emitted success`).
  - Incoming Payment (no URIs in state): Rejected (`Error: This hook has no more URI tokens to mint`).
  - Outgoing Payment: Accepted (`Success: Outgoing Payment Transaction`).

- **Note**: Ensure `URIL` is set before adding `URI`. Use [Hooks Services Tools](https://hooks.services/tools) for hex conversions. Ensure sufficient XAH balance for transaction fees and emitted transactions.

## Testing
1. **Setup Accounts**:
   - Create a Hook account (e.g., `rTest123...`) and a second account (e.g., `rTest456...`) using the Testnet Faucet.
2. **Deploy Hooks**:
   - Use Xahau Hooks Builder or XRPLWin Hook Management to deploy Hooks to `rTest123...` for `ttPayment` (and `ttINVOKE` for Hook 017).
   - For Hook 016, send `ttINVOKE` transactions to set `AM`, `AO`, `A1`, and `NUM`.
   - For Hook 017, send `ttINVOKE` transactions to set `URIL`, `URI`, `NUM`, and `COUNT`.
3. **Send Transactions**:
   - Incoming Payment (e.g., exact amount for Hook 016, any XAH for Hook 017) from `rTest456...` to `rTest123...` via XRPLWin Tools.
   - Outgoing Payment from `rTest123...` to `rTest456...`.
   - For Hook 017, `ttINVOKE` transactions to configure or delete URIs, or test non-owner access.
4. **Verify**:
   - Check transaction status in Xahau Testnet Explorer.
   - View logs in Hooks Builder or XRPLWin Tools for `TRACESTR`/`TRACEHEX`/`TRACEVAR`.

## Debugging Tips
- **Logs**: Use `TRACESTR` (e.g., `Invoke Multi Emit: Payment received and forwarded`), `TRACEHEX` (for accounts or URIs), and `TRACEVAR` (for amounts or counts) to trace execution.
- **Xahau Explorer**: Verify `TransactionType`, `HookParameters`, `Amount`, and emitted transactions.
- **Common Errors**:
  - `Execution failure (no exit type specified)`: Indicates invalid field access (e.g., `sfAmount`). Use appropriate buffer sizes (e.g., 48 bytes for `sfAmount`, 256 bytes for `URI`).
  - State errors: Ensure `AM`, `AO`, `COUNT` (Hook 016) or `URIL`, `URI`, `COUNT` (Hook 017) are set via `ttINVOKE` before processing Payments.
  - Invalid parameters: Ensure `AM`, `AO`, `URIL`, `NUM`, `COUNT` are valid uint64 values and `A1` is a valid Account ID.
  - Emit failure: Verify `etxn_reserve` and transaction preparation (`PREPARE_PAYMENT_SIMPLE` or `PREPARE_REMIT_TXN`).
  - Insufficient balance: Ensure the Hook account has enough XAH for fees and emitted transactions.
- **Testnet Node**: Use `wss://testnet.xahau.network` via Hooks Builder or XRPLWin.

## Lessons Learned
- **Multi-Account Forwarding**: Hook 016 demonstrates emitting multiple Payments using a loop and state-stored accounts, useful for batch processing.
- **URI Token Minting**: Hook 017 shows how to emit Remit transactions for NFT-like functionality, with dynamic URI management via state.
- **State Management**: Both Hooks use `state_set` and `state` with unique keys for persistent configuration, enabling complex logic.
- **Buffer Sizing**: Use large buffers (e.g., 256 bytes for `URI`, 48 bytes for `sfAmount`) to handle variable-length data.
- **Transaction Emission**: Proper use of `etxn_reserve`, `PREPARE_PAYMENT_SIMPLE`, and `PREPARE_REMIT_TXN` ensures reliable transaction emission.
- **Guard Macro**: Use `_g(1,1)` to prevent reentrancy and ensure safe execution.
- **Testing**: Test with varied amounts, invalid parameters, and edge cases (e.g., zero `COUNT` or missing `URIL`) to catch errors.

## Contributing
Fork this repo, add new Hooks, or enhance the guide! Submit issues or PRs to improve `HandyHooks`.

## Work in Progress
Hooks 018, 019, and 020 are under development and will be included in future updates with additional functionality.