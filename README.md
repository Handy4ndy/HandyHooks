# HandyHooks

Welcome to **HandyHooks**, a collection of pre-built Xahau Hooks for the Xahau Network. This repository hosts Hooks designed for production use on Mainnet, alongside the educational **Xahau Hooks 101** series. All Hooks are written in C, compiled to WebAssembly (WASM) using the [Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop) starter template, and available for anyone to use, edit, and distribute. Each Hook includes its own `README` with installation instructions and detailed information.

## What is Xahau?

Xahau is a decentralized blockchain platform that enables secure, efficient, and transparent transactions. Its immutable ledger ensures data integrity, making it ideal for financial and automated applications.

## What are Hooks?

Hooks are compact, efficient C programs attached to Xahau accounts, allowing users to automate actions based on specific conditions or events. Examples include forwarding payments, enforcing transaction rules, or managing state. Hooks are executed on the Xahau Network, extending its functionality for both Testnet and Mainnet applications.

## About Hook Builder

[Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop) is a user-friendly, web-based tool for creating, testing, and deploying Hooks. It simplifies development with:
- A code editor for writing C Hooks.
- A simulated Testnet environment for testing.
- One-click deployment to Testnet or Mainnet accounts.

All Hooks in this repository are compiled using the Hooks Builder starter template, ensuring Mainnet compatibility.

## Tools
Use these online tools to work with HandyHooks—no local setup required:
- **[Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop)**: Write, compile, and deploy Hooks using the starter template.
- **[XRPLWin Xahau Testnet Tools](https://xahau-testnet.xrplwin.com/tools)**: Create and test transactions on the Testnet.
- **[XRPLWin Hook Management](https://xahau-testnet.xrplwin.com/account/YOUR_WALLET_RADDRESS_HERE/manage/hooks)**: Deploy and manage Hooks (replace `YOUR_WALLET_RADDRESS_HERE` with your account, e.g., `rTest123...`).
- **[Xahau Testnet Faucet](https://xahau-test.net/faucet)**: Fund Testnet accounts.
- **[Xahau Explorer](https://test.xahauexplorer.com/en)**: Verify transactions and Hook details.


## Repository Structure
The repository is organized into directories based on Hook purpose:
- **Xahau-Hooks-101**: Contains the **Xahau Hooks 101** series, a collection of educational Hooks for learning Xahau Hook development. See the [Xahau-Hooks-101 README](Xahau-Hooks-101/README.md) for details on these beginner-friendly examples.
- **Production Hooks**: Includes Hooks designed for real-world use, such as `ForwardSplit`. Each Hook has its own directory and `README` with specific instructions.

### Example Hook: ForwardSplit
- **Purpose**: Forwards incoming XAH payments to three predefined accounts, distributing a percentage of the payment based on specified parameters.
- **Hook Parameters**:
  - `DEST1`, `DEST2`, `DEST3`: Destination account addresses.
  - `PCT1`, `PCT2`, `PCT3`: Percentage splits (e.g., 50%, 30%, 20%).
- **Installation**:
  - Open `ForwardSplit.c` in Xahau Hooks Builder.
  - Compile with the starter template.
  - Deploy via Hooks Builder or XRPLWin Hook Management.
- **Details**: See the [ForwardSplit README](ForwardSplit/README.md) for full instructions and test cases.

## Testing
1. **Setup Accounts**:
   - Fund a Testnet account (e.g., `rTest123...`) using the Testnet Faucet.
2. **Deploy Hooks**:
   - Compile in Xahau Hooks Builder and deploy via Hooks Builder or XRPLWin Hook Management.
3. **Test Transactions**:
   - Send Payments (e.g., XAH or IOU) using XRPLWin Tools.
4. **Verify**:
   - Check results in Xahau Explorer or Hooks Builder logs (`TRACESTR`/`TRACEHEX`).

## Debugging Tips
- **Logs**: Use `TRACESTR` and `TRACEHEX` to track execution.
- **Xahau Explorer**: Verify `TransactionType`, `Amount`, and `HookHash`.
- **Common Issues**:
  - `Execution failure (no exit type specified)`: Caused by invalid `sfAmount` access. Use `uint8_t amount[48]` and check `otxn_field` returns.
  - Non-Payment transactions: Ensure Hooks are set for `ttPayment`.

## Contributing
Explore the Hooks, integrate them into your projects, or contribute new ones! Submit issues or PRs to enhance **HandyHooks**. For educational Hooks, check out the [Xahau Hooks 101](Xahau-Hooks-101/README.md) subfolder.