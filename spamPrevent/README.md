# HandyHooks - Spam Prevent Hook

Welcome to **HandyHooks**, a collection of pre-built Xahau Hooks for production use on the Xahau Network, tailored for e-commerce platforms and beyond. This repository hosts Hooks designed for real-world applications, alongside the educational **Xahau Hooks 101** series. Written in C and compiled to WebAssembly (WASM) using the [Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop) starter template, all Hooks are Mainnet-ready and freely available for use, modification, and distribution. Each Hook includes a dedicated `README` with installation instructions and details.

## What is Xahau?

Xahau is a decentralized blockchain platform that delivers secure, efficient, and transparent transactions through its immutable ledger. It’s ideal for e-commerce, financial automation, and asset management.

## What are Hooks?

Hooks are small, efficient C programs attached to Xahau accounts, enabling automated actions like payment processing, spam filtering, or state management. Executed on the Xahau Network, Hooks support both Testnet and Mainnet, making them perfect for a variety of use cases.

## About Hook Builder

[Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop) is a web-based tool that simplifies Hook development, offering:
- A code editor for C Hooks.
- Testnet simulation for testing.
- One-click deployment to Testnet or Mainnet.

All Hooks in this repository are compiled using the Hooks Builder starter template, ensuring Mainnet compatibility.

## Tools

Use these online tools to develop, test, and deploy Hooks—no local setup required:
- **[Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop)**: Write, compile, and deploy Hooks using the starter template.
- **[XRPLWin Xahau Testnet Tools](https://xahau-testnet.xrplwin.com/tools)**: Create and test transactions.
- **[XRPLWin Hook Management](https://xahau-testnet.xrplwin.com/account/YOUR_WALLET_RADDRESS_HERE/manage/hooks)**: Deploy and manage Hooks (replace `YOUR_WALLET_RADDRESS_HERE` with your account, e.g., `rTest123...`).
- **[Xahau Testnet Faucet](https://xahau-test.net/faucet)**: Fund Testnet accounts.
- **[Xahau Explorer](https://test.xahauexplorer.com/en)**: Verify transactions and Hook details.
- **[XRPL Hex Visualizer](https://transia-rnd.github.io/xrpl-hex-visualizer/)**: Convert parameters.
- **[Xahau Explorer Mainnet](https://xahauexplorer.com/en)** : Check Mainnet transactions.
- **[XRPLWin Xahau Mainnet](https://xahau.xrplwin.com/)** : Deploy and manage mainnet Hooks.

### Example Hook: Spam Prevent Hook
- **File**: `spamPrevent.c`

- **Testnet**: `B7D5ECD3DA42CF90A689BD659617436565C4D5E0ED2E0865737E35387EC9855A`
- **Mainnet**: `277A521F154C34C81B99CE6CE0F000DBC284673C07CC7B37D4056A64BCB2F07D`

- **Purpose**: Prevents spam transactions by filtering incoming payments based on configurable settings, suitable for increasing account security.

- **Original Author**: Handy Andy ([@Handy4n...](https://x.com/Handy4n...)).

- **Logic**: Installed on an account, the Hook:
  - Accepts all outgoing payments 
  - Filters incoming payments with optional checks:
    - `MIN`: Rejects payments below 1 XAH . ('MIN_AMOUNT_VALUE' defined on line 38)
    - `PAR`: Rejects payments with hook parameters.
    - `MEM`: Rejects payments with memos.
  - Allows the hook owner to toggle settings via `ttINVOKE` transactions.

- **Hook Parameters**:
  - `MIN`: Toggle to reject payments below 1 XAH (00 or 01).
  - `PAR`: Toggle to reject payments with hook parameters (00 or 01).
  - `MEM`: Toggle to reject payments with memos (00 or 01).

- **Installation**:
  - Open `spamPrevent.c` in Xahau Hooks Builder.
  - Compile with the starter template.
  - Deploy via Hooks Builder or XRPLWin Hook Management.
  - Set `HookOn` for `ttPayment` and `ttInvoke` (e.g., `0xfffffffffffffffffffffffffffffffffffffff7ffffffffffffffffffbffffe`).
  - Example `SetHook` transaction:
    ```javascript
    const prepared = {
      "TransactionType": "SetHook",
      "Account": "your_account_address",
      "Flags": 0,
      "Hooks": [{
        "Hook": {
          "HookHash": "YOUR_COMPILED_HOOK_HASH",
          "HookNamespace": "0204DAD98F5ADB07B8CB073BFBCC1B331A2589ED7744C644577A024E0D98A220",
          "HookOn": "0xfffffffffffffffffffffffffffffffffffffff7ffffffffffffffffffbffffe"
        }
      }],
      ...networkInfo.txValues
    };
    ```
  
- **Test Cases**:
  - Outgoing XAH Payment: Accepted.
  - Incoming XAH Payment below 1 XAH with `MIN=01`: Rejected.
  - Incoming Payment with parameters with `PAR=01`: Rejected.
  - Incoming Payment with memo with `MEM=01`: Rejected.
  - Non-XAH Payment: Accepted (bypasses `MIN`).

- **Note**: Test on Testnet before Mainnet. Designed as a template, with potential for future enhancements (e.g., configurable minimum amount, account blacklist).

## Testing

1. **Setup Accounts**:
   - Fund a Testnet account (e.g., `rTest123...`) using the Testnet Faucet.
2. **Deploy Hooks**:
   - Compile in Xahau Hooks Builder and deploy via Hooks Builder or XRPLWin Hook Management.
3. **Test Transactions**:
   - Send Payments or `ttINVOKE` transactions.
4. **Verify**:
   - Check results in Xahau Explorer or Hooks Builder logs (`TRACESTR`/`TRACEHEX`).

## Debugging Tips

- **Logs**: Use `TRACESTR` and `TRACEVAR` to track execution.
- **Xahau Explorer**: Verify `TransactionType`, `Amount`, `HookHash`.
- **Common Issues**:
  - `Execution failure`: Check `otxn_field` returns for `sfAmount` or `sfHookParameters`.
  - State errors: Ensure `ttINVOKE` parameters are correctly formatted.
- **State Tracking**: Monitor state with XRPLWin Hook Testnet.

## Contributing
Integrate this Hook into your projects or contribute new ones to **HandyHooks**! Submit issues or PRs to enhance the collection. For educational Hooks, see [Xahau Hooks 101](Xahau-Hooks-101/README.md).

## Acknowledgments
- Special thanks to Dazzling (@dazzlingxrpl) and Mayukha Vadari (@msvadari) for insights on Hooks and XRPL evolution.