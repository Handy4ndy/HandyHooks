# Blacklist Trustee — Deploy & Test (Hooks Builder)

Overview
- Deploy and validate the Trustee-side Blacklist hook using the Hooks Builder.
- Source: HandyHooks/Blacklist/Trustee/BlacklistTrustee.c

Prerequisites
- Create the `provider` and `attacker` accounts.
- Deploy the Blacklist Provider hook on the `provider`.
- Invoke the `provider` with a transaction that adds `attacker` to the blacklist.
reference: 

Xahau Testnet — Hooks Builder (step-by-step)
1. Fund test accounts
  - Use https://builder.xahau.network/deploy to create/fund `trustee`and `userA` Testnet accounts.

2. Prepare & compile
  - In Builder → `Develop` paste `HandyHooks/Blacklist/Trustee/BlacklistTrustee.c` and `Compile to WASM`.

3. Deploy (Set Hook)
  - Builder → `Deploy` → select `trustee` and click `Set Hook`.
  - Configure `HookOn` triggers (Payments/Invoke) required by the trustee logic.

4.  Setup / Run tests (Builder `Test` UI)
  - Create the following transactions:
    - `trustee` Invoke, Hook Parameter Name 'PROVIDER_ACC' && Value `provider` (replace `PROVIDER_ACCOUNTID` [hooks.Services](https://hooks.services/tools/raddress-to-accountid)):

    ```json
    {
        "TransactionType": "Invoke",
        "Account": `trustee`,
        "Destination": "",
        "Fee": "1000",
        "HookParameters": [
            {
                "HookParameter": {
                    "HookParameterName": "50524F56494445525F414343",                    // PROVIDER_ACC (HEX)
                    "HookParameterValue": "PROVIDER_ACCOUNTID"    // `provider` (AccountID)
                }
            }
        ],
        "Memos": []
    }
    ```

    - `trustee` Invoke, Hook Parameter Name 'BLACKLIST' && Value '00' (disable) || '01' (enable)

     ```json
    {
        "TransactionType": "Invoke",
        "Account": `trustee`,
        "Destination": "",
        "Fee": "1000",
        "HookParameters": [
            {
                "HookParameter": {
                    "HookParameterName": "424C41434B4C495354",    // BLACKLIST (HEX)
                    "HookParameterValue": "01"                    // 00 [disable] || 01 [enable] (Byte)
                }
            }
        ],
        "Memos": []
    }
    ```

    - `userA` Payment, simple payment of 100 XAH to the `trustee`.
    ```json
    {
        "TransactionType": "Payment",
        "Account": `userA`,
        "Destination": `trustee`,
        "Amount": "100000000",
        "Fee": "12",
        "HookParameters": [],
        "Flags": "2147483648",
        "Memos": []
    }
    ```

    - `attacker` Payment, simple payment of 100 XAH to the `trustee`.

    ```json
    {
        "TransactionType": "Payment",
        "Account": `attacker`,
        "Destination": `trustee`,
        "Amount": "100000000",
        "Fee": "12",
        "HookParameters": [],
        "Flags": "2147483648",
        "Memos": []
    }
    ```

  - Test cases:
    - Trustee operations with invoke transactions: expect success.
    - Operations from `userA`: expect success.
    - Operations from `attacker` (blacklisted): expect rejection.

5. Verify & debug
  - Check TRACESTR/TRACEHEX logs in Builder for runtime traces and errors.
  - Confirm transaction status and effect using Xahau Explorer: https://test.xahauexplorer.com/en and XRPLWin Hook Management: https://xahau-testnet.xrplwin.com/.

Notes
- Use distinct accounts for `trustee` and `provider` during tests to avoid permission confusion.
- The hook expects specific `Invoke` payload formats, review the header from the .c code for a quick reminder.

Useful references
- [XahauHooks101/Basic_State](https://github.com/Handy4ndy/XahauHooks101/tree/main/Basic_State) and [XahauHooks101/Basic_Invoke_Parameters](https://github.com/Handy4ndy/XahauHooks101/tree/main/Basic_Invoke_Parameters) for example invocation patterns.
- [XahauHooks101/Basic_Native](https://github.com/Handy4ndy/XahauHooks101/tree/main/Basic_Native) and [XahauHooks101/Basic_IOU](https://github.com/Handy4ndy/XahauHooks101/tree/main/Basic_IOU) for Accept / Rejection patterns.

