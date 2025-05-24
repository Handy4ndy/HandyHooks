# Xahau Hooks 101

**Xahau Hooks 101** is a growing collection of short, beginner-friendly Xahau Hook examples written in C, designed to explore the Xahau Network’s smart contract-like features. Each Hook is a concise, digestible snippet compiled to WebAssembly (WASM) using the [Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop) starter template, suitable for testing on the Xahau Testnet or deploying to Mainnet. This project is a learning journey to better understand C and Xahau Hooks, creating a reference resource for future use.

## Overview
Xahau Hooks are lightweight WASM modules that add custom logic to Xahau accounts, such as validating transactions or managing data. This project aims to build a series of simple Hooks to demonstrate key features, starting with basic transaction handling and gradually exploring advanced concepts like state management and namespaces. As a work in progress, it reflects an ongoing effort to master C programming and Hook development.

## Project Goal
The goal is to create a set of short, easy-to-understand Hook examples that:
- Showcase Xahau Hook capabilities in small, focused snippets.
- Serve as a learning tool for C and Xahau development.
- Provide a reusable reference for future projects.
- Use online tools for accessibility and Mainnet compatibility.

The project will evolve over time, adding new Hooks as skills and understanding grow.

## Tools
No local setup needed! Use these online tools to write, compile, test, and deploy Hooks:
- **[Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop)**: Web-based IDE for coding, compiling, and deploying Hooks. Uses the starter template for Testnet and Mainnet compatibility.
- **[XRPLWin Xahau Testnet Tools](https://xahau-testnet.xrplwin.com/tools)**: Create and test transactions on the Testnet.
- **[XRPLWin Hook Management](https://xahau-testnet.xrplwin.com/account/YOU_WALLET_RADDRESS_HERE/manage/hooks)**: Deploy and manage Hooks (replace `YOU_WALLET_RADDRESS_HERE` with your account, e.g., `rTest123...`).
- **[Xahau Testnet Faucet](https://xahau-test.net/faucet)**: Fund Testnet accounts.
- **[Xahau Explorer](https://test.xahauexplorer.com/en)**: Check transactions and Hook details.
- **[Xahau Mainnet Explorer](https://xahau.xrplwin.com/)**: Check transactions and Hook details.


## Current Status
The project is in its early stages, with a small set of Hooks already developed in the `001-005` `006-010` `011-015` folder (see the separate `README.md` there for details). These initial Hooks focus on basic transaction processing, serving as a foundation for learning C and Xahau Hooks.

## Future Plans
As the project grows, new Hooks will explore:
- Transaction validation (e.g., filtering by amount or account).
- State management (e.g., storing data with `state_set`/`state_get`).
- Namespaces for organized data.
- Other Xahau features, keeping each example short and clear.

The focus is on gradual learning, with no fixed roadmap to allow flexibility.

## Testing
1. **Setup Accounts**:
   - Fund a Testnet account (e.g., `rTest123...`) using the Testnet Faucet.
2. **Deploy Hooks**:
   - Compile Hooks in Xahau Hooks Builder (starter template) and deploy via Hooks Builder or XRPLWin Hook Management.
3. **Test Transactions**:
   - Send Payments (e.g., XAH or IOU) using XRPLWin Tools.
4. **Verify**:
   - Check results in Xahau Explorer or Hooks Builder logs (`TRACESTR`/`TRACEHEX`).

## Debugging Tips
- **Logs**: Use `TRACESTR` and `TRACEHEX` to track execution.
- **Xahau Explorer**: Verify `TransactionType` and `HookHash`.
- **Common Issues**:
  - Buffer errors: Use `uint8_t amount[48]` for `sfAmount`.
  - Invalid fields: Check `otxn_field` return values (e.g., `< 0`).
- **Testnet Node**: `wss://testnet.xahau.network`.

## Contributing
This is a learning project, but contributions are welcome! Fork the repo, suggest simple Hook ideas, or share feedback. Submit issues or PRs to help grow `Xahau Hooks 101`.