# HandyHooks - uriRemit.c

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
- **[Xahau Explorer](https://explorer.xahau-test.net)**: Verify transactions and Hook details.
- **[XRPL Hex Visualizer](https://xrplwin.com/tools/hex)**: Convert parameters (e.

### Example Hook: URI Token Mint Hook
- **File**: `uriRemit.c`

- **Purpose**: Autonomously mints and distributes URI Tokens (e.g., NFTs, RWAs) for e-commerce, triggered by incoming Payments. Modified to support outgoing transactions and a developing Multi Remit feature for distributing `X * COST`.

- **Original Author**: Cbot ([@Cbot_XRPL](https://x.com/Cbot_XRPL), [URI-Token-Remit](https://github.com/Cbot-XRPL/URI-Token-Remit)).

- **Modifications**:
  - Added support for outgoing Payments, enabling flexible e-commerce workflows.
  - Developing **Multi Remit** to distribute Payments based on `X * COST` (e.g., splitting revenue for multiple parties).

- **Logic**: Installed on an account, the Hook mints XLS-53-compliant URI Tokens when Payments matching `COST` are received. URIs and `NUM` are managed via `Invoke` transactions, requiring consistent `URIL` (URI length). An optional lock feature gates Payments with a passkey. Tokens are minted in `NUM` order, with state deletion post-mint. Supports e-commerce asset distribution (e.g., digital products, tickets).

- **Hook Parameters**:
  - `URIL`: URI length in bytes (uint64, hex-converted).
  - `URI`: Metadata URI (hex-converted, e.g., `ipfs://.../000001.json`).
  - `NUM`: URI number (uint64, e.g., `000001`).
  - `COST`: XAH cost for minting (uint64).
  - `LOCK`: Optional passkey for gating (uint64).
  - `PASS`: Passkey to unlock Payments (uint64).
  - `ROYALTIES`: Royalty percentage (1-99, uint64).
  - `COUNT`: Adjusts counter state (uint64, optional).
  - `DEL`: Deletes state by `NUM` (uint64).

- **Installation**:
  - Open `uriRemit.c` in Xahau Hooks Builder.
  - Compile with the starter template.
  - Deploy via Hooks Builder or XRPLWin Hook Management using HookHash `6333F34AD8ABAFE4AD2AAFDBBFBC13ECC081D1D44A42DFA2F6A69CB9CBC22EFE`.
  - Set `HookOn` for `Invoke`and `Payment`.(`0xfffffffffffffffffffffffffffffffffffffff7ffffffffffffffffffbffffe`).
  - Example `SetHook` transaction:
    ```javascript
    const prepared = {
      "TransactionType": "SetHook",
      "Account": "your_account_address",
      "Flags": 0,
      "Hooks": [{
        "Hook": {
          "HookHash": "6333F34AD8ABAFE4AD2AAFDBBFBC13ECC081D1D44A42DFA2F6A69CB9CBC22EFE",
          "HookNamespace": "0204DAD98F5ADB07B8CB073BFBCC1B331A2589ED7744C644577A024E0D98A220",
          "HookOn": "0xfffffffffffffffffffffffffffffffffffffff7ffffffffffffffffffbffffe"
        }
      }],
      ...networkInfo.txValues
    };
    ```
  - See the [uriRemit README](xMerch_/README.md) for details.

- **Test Cases**:
  - Incoming XAH Payment (matching `COST`): Mints URI Token, deletes state.
  - Incoming Payment with `PASS` (if locked): Accepted if passkey matches.
  - Outgoing Payment: Accepted (new feature).
  - Incorrect `URIL`: Rejects minting (state save fails).

- **E-Commerce Use**: Deployed for minting digital assets (e.g., tickets, art) on an e-commerce platform, with Multi Remit under development for payment splitting.

- **Note**: Test on Testnet before Mainnet due to URI length sensitivity. Originally by Cbot for the Xahau Hackathon, modified for e-commerce.


## Testing

1. **Setup Accounts**:
   - Fund a Testnet account (e.g., `rTest123...`) using the Testnet Faucet.
2. **Deploy Hooks**:
   - Compile in Xahau Hooks Builder and deploy via Hooks Builder or XRPLWin Hook Management.
3. **Test Transactions**:
   - Send Payments or `Invoke` transactions using XRPLWin Tools.
4. **Verify**:
   - Check results in Xahau Explorer or Hooks Builder logs (`TRACESTR`/`TRACEHEX`).

## Debugging Tips

- **Logs**: Use `TRACESTR` and `TRACEHEX` to track execution.
- **Xahau Explorer**: Verify `TransactionType`, `Amount`, `HookHash`.
- **Common Issues**:
  - `Execution failure`: Use `uint8_t amount[48]` for `sfAmount` and check `otxn_field` returns.
  - URI length errors: Use XRPL Hex Visualizer for consistent `URIL`.
- **State Tracking**: Monitor state with XRPLWin Hook Testnet.

## Contributing
Integrate these Hooks into your e-commerce platforms or contribute new ones! Submit issues or PRs to enhance **HandyHooks**. For educational Hooks, see [Xahau Hooks 101](Xahau-Hooks-101/README.md).

## Acknowledgments

- **URI Token Mint Hook**: Original source code by Cbot ([@Cbot_XRPL](https://x.com/Cbot_XRPL), [URI-Token-Remit](https://github.com/Cbot-XRPL/URI-Token-Remit)), modified for e-commerce with outgoing transactions and Multi Remit.
- Thanks to the Xahau community for inspiration and support.
