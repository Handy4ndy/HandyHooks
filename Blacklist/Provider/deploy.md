# Blacklist Provider — Deploy & Test (Hooks Builder)

Overview
- Deploy and validate the Provider-side Blacklist hook using the Hooks Builder.
- Source: HandyHooks/Blacklist/Provider/BlacklistProvider.c

Xahau Testnet — Hooks Builder (step-by-step)
1. Fund test accounts
  - Open https://builder.xahau.network/deploy and create/fund two Testnet accounts: `provider` and `attacker` (use the Builder faucet/fund flow).

2. Prepare & compile
  - In Builder → `Develop` paste `HandyHooks/Blacklist/Provider/BlacklistProvider.c` and `Compile to WASM`.

3. Deploy (Set Hook)
  - In Builder → `Deploy` choose the `provider` account and click `Set Hook`.
  - Configure `HookOn` triggers that match the hook's expected transactions (e.g., Payments, Invoke).

4. Setup / Run tests (Builder `Test` UI)
  - Create the following transactions (replace account placeholders with actual Testnet addresses):
    
    - `provider` Invoke, Hook Parameter to add `attacker` to the blacklist (replace `ATTACKER_ACCOUNTID` [hooks.Services](https://hooks.services/tools/raddress-to-accountid)):

    ```json
    {
        "TransactionType": "Invoke",
        "Account": `provider`,
        "Destination": "",
        "Fee": "1000",
        "HookParameters": [
            {
                "HookParameter": {
                    "HookParameterName": "4144445F424C41434B4C495354",    // ADD_BLACKLIST (HEX)
                    "HookParameterValue": "ATTACKER_ACCOUNTID"    // `attacker` (AccountID)
                }
            }
        ],
        "Memos": []
    }
    ```

    - `provider` Invoke, Hook Parameter to Remove `attacker` from the blacklist (replace `ATTACKER_ACCOUNTID`[hooks.Services](https://hooks.services/tools/raddress-to-accountid) ):

     ```json
    {
        "TransactionType": "Invoke",
        "Account": `provider`,
        "Destination": "",
        "Fee": "1000",
        "HookParameters": [
            {
                "HookParameter": {
                      "HookParameterName": "52454D4F56455F424C41434B4C495354",    // REMOVE_BLACKLIST (HEX)
                    "HookParameterValue": "ATTACKER_ACCOUNTID"    // `attacker` (AccountID)
                }
            }
        ],
        "Memos": []
    }
    ```

    - `attacker` Payment, simple payment of 100 XAH to the `provider`.

    ```json
    {
        "TransactionType": "Payment",
        "Account": `attacker`,
        "Destination": `provider`,
        "Amount": "100000000",
        "Fee": "12",
        "HookParameters": [],
        "Flags": "2147483648",
        "Memos": []
    }
    ```

  - Test cases:
    - Provider operations with invoke transactions: expect success.
    - Operations from `attacker` (when blacklisted): expect rejection.

5. Verify & debug
  - Inspect Builder logs (TRACESTR / TRACEHEX) for hook runtime traces and error messages.
  - Confirm transaction status and effect using Xahau Explorer: https://test.xahauexplorer.com/en and XRPLWin Hook Management: https://xahau-testnet.xrplwin.com/.

Notes
- Keep the test account keys handy in the Builder to sign tests. Use separate accounts for attacker vs normal users to avoid state confusion.
- The hook expects specific `Invoke` payload formats, review the header from the .c code for a quick reminder.

Useful references
- [XahauHooks101/Basic_State](https://github.com/Handy4ndy/XahauHooks101/tree/main/Basic_State) and [XahauHooks101/Basic_Invoke_Parameters](https://github.com/Handy4ndy/XahauHooks101/tree/main/Basic_Invoke_Parameters) for example invocation patterns.
- [XahauHooks101/Basic_Native](https://github.com/Handy4ndy/XahauHooks101/tree/main/Basic_Native) and [XahauHooks101/Basic_IOU](https://github.com/Handy4ndy/XahauHooks101/tree/main/Basic_IOU) for Accept / Rejection patterns.
