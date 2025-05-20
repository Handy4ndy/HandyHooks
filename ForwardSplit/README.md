# HandyHooks - ForwardSplit.c

Welcome to **HandyHooks**, a collection of pre-built Xahau Hooks for production use on the Xahau Network, tailored for e-commerce platforms. This repository hosts Hooks designed for real-world applications, alongside the educational **Xahau Hooks 101** series. Written in C and compiled to WebAssembly (WASM) using the [Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop) starter template, all Hooks are Mainnet-ready and freely available for use, modification, and distribution. Each Hook includes a dedicated `README` with installation instructions and details.

## What is Xahau?

Xahau is a decentralized blockchain platform that delivers secure, efficient, and transparent transactions through its immutable ledger. It’s ideal for e-commerce, financial automation, and asset management.

## What are Hooks?

Hooks are small, efficient C programs attached to Xahau accounts, enabling automated actions like payment processing, token minting, or state management. Executed on the Xahau Network, Hooks support both Testnet and Mainnet, making them perfect for e-commerce solutions.

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
- **[Xahau Explorer](https://xahauexplorer.com/en)**: Verify transactions and Hook details.
- **[XRPL Hex Visualizer](https://transia-rnd.github.io/xrpl-hex-visualizer/)**: Convert parameters (e.g., account IDs, percentages).
- **[R-Address to AccountID](https://hooks.services/tools/raddress-to-accountid)**: Convert r-addresses to HEX AccountID.
- **[Decimal to Hex/FlipHex](https://hooks.services/tools/decimal-to-hex-to-fliphex)**: Convert percentages to HEX_FLIPPED.

## Example Hook: ForwardSplit Hook

- **File**: `ForwardSplit.c`
- **Purpose**: Splits incoming XAH Payments to three predefined accounts based on percentage parameters, reserving 1% in the Hook account for transaction fees. Designed for e-commerce revenue distribution.

- **Logic**: Installed on `ttPayment`, the Hook verifies incoming XAH Payments, calculates splits using floating-point percentages, ensures a 1% reserve, and emits Payments to three accounts (Main, Second, Third). Outgoing Payments are accepted without splitting.
- **Hook Parameters**:
  - `M`: Main account (HEX AccountID).
  - `S`: Second account (HEX AccountID).
  - `T`: Third account (HEX AccountID).
  - `MA`: Main percentage (HEX_FLIPPED, e.g., 50%).
  - `SA`: Second percentage (HEX_FLIPPED, e.g., 30%).
  - `TA`: Third percentage (HEX_FLIPPED, e.g., 20%).
  - **Note**: Total percentages must not exceed 99% to reserve 1%.

- **Installation**:
  - Open `ForwardSplit.c` in [Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop).
  - Compile with the starter template.
  - Deploy via Hooks Builder or [XRPLWin Hook Management](https://xahau-testnet.xrplwin.com/account/YOUR_WALLET_RADDRESS_HERE/manage/hooks).
  - Set `HookOn` for `ttPayment` (`0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffbffffe`).
  - Example `SetHook` transaction:
    ```javascript
    const prepared = {
      "TransactionType": "SetHook",
      "Account": "your_account_address",
      "Flags": 0,
      "Hooks": [{
        "Hook": {
          "HookHash": "YOUR_HOOK_HASH",
          "HookNamespace": "YOUR_NAMESPACE",
          "HookOn": "0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffbffffe"
        }
      }],
      ...networkInfo.txValues
    };
    ```
  - **Note**: Obtain `HookHash` and `HookNamespace` from Hooks Builder after compilation. See the [ForwardSplit source code](ForwardSplit.c) for details.
- **Test Cases**:

  - Incoming XAH Payment (100 XAH): Splits to Main (50 XAH), Second (30 XAH), Third (20 XAH), retaining 1 XAH.
  - Outgoing XAH Payment: Accepted.
  - Non-XAH Payment: Rejected (`sfAmount` not 8 bytes).
  - Invalid Percentages (>99%): Rejected.
  - Missing Parameters: Rejected (e.g., unset `M`, `MA`).

- **E-Commerce Use**: Deployed for e-commerce platforms to split Payments among stakeholders (e.g., vendors, affiliates, platform fees).
- **Note**: Test on Testnet before Mainnet to ensure parameter accuracy.

## Testing

1. **Setup Accounts**:
   - Fund a Testnet account (e.g., `rTest123...`) using the Testnet Faucet.
2. **Deploy Hooks**:
   - Compile in Xahau Hooks Builder and deploy via Hooks Builder or XRPLWin Hook Management.
3. **Test Transactions**:
   - Send Payments using XRPLWin Tools.
4. **Verify**:
   - Check results in Xahau Explorer or Hooks Builder logs (`TRACESTR`/`TRACEHEX`).

## Debugging Tips
- **Logs**: Use `TRACESTR` and `TRACEHEX` to track execution.
- **Xahau Explorer**: Verify `TransactionType`, `Amount`, `HookHash`.
- **Common Issues**:
  - `Execution failure`: Use `uint8_t amount[48]` for `sfAmount` and check `otxn_field` returns.
  - Parameter errors: Use XRPL Hex Visualizer or Decimal to Hex/FlipHex for correct values.
- **State Tracking**: Monitor with [XRPLWin Hook Testnet](https://xahau-testnet.xrplwin.com/tools).


## Contributing
Integrate these Hooks into your e-commerce platforms or contribute new ones! Submit issues or PRs to enhance **HandyHooks**. For educational Hooks, see [Xahau Hooks 101](Xahau-Hooks-101/README.md).

## Acknowledgments
- **ForwardSplit Hook**: Original source by Satish from XRPL Labs ([YouTube](https://www.youtube.com/watch?v=KdOHr6L0Ss4&t=380s), [GitHub](https://github.com/technotip/HookTutorials)).
- Thanks to the Xahau community for inspiration and support.
