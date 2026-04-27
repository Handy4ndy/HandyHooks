# Blacklist Hookset — Overview & File Summary

This directory contains a small Hookset that implements centralized blacklist management for Xahau Hooks. It includes a Provider hook (manages blacklist state) and a Trustee hook (enforces or references blacklist state). The files are kept concise so you can deploy and test quickly using the Xahau Hooks Builder (Testnet).

Purpose
- Centralize blacklist data in the `Provider` hook so other hooks can query blacklist status via `state_foreign()`.
- Provide a `Trustee` hook that integrates with the Provider and enforces blacklist rules on incoming transactions.

Structure
- Provider
  - [HandyHooks/Blacklist/Provider/BlacklistProvider.c](https://github.com/Handy4ndy/HandyHooks/blob/main/Blacklist/Provider/BlacklistProvider.c) — Provider hook source. Implements `ADD_BLACKLIST`, `REMOVE_BLACKLIST`, and `CHECK_BLACKLIST` invoke commands and exposes blacklist state via namespaced keys (`BLACKLISTED`).
  - [HandyHooks/Blacklist/Provider/deploy.md](https://github.com/Handy4ndy/HandyHooks/blob/main/Blacklist/Provider/deploy.md) — Deploy + test guide for the Provider on Xahau Testnet.
  - [HandyHooks/Blacklist/Provider/README.md](https://github.com/Handy4ndy/HandyHooks/blob/main/Blacklist/Provider/README.md)) — Provider notes (brief).

- Trustee
  - [HandyHooks/Blacklist/Trustee/BlacklistTrustee.c](https://github.com/Handy4ndy/HandyHooks/blob/main/Blacklist/Trustee/BlacklistTrustee.c) — Trustee hook source. Reads configuration via Invoke parameters (e.g., `PROVIDER_ACC`) and enforces blacklist checks on Payments/Invoke flows.
  - [HandyHooks/Blacklist/Trustee/deploy.md](https://github.com/Handy4ndy/HandyHooks/blob/main/Blacklist/Trustee/deploy.md) — Deploy + test guide for the Trustee on Xahau Testnet (includes example JSON payloads to paste into Builder Test UI).
  - [HandyHooks/Blacklist/Trustee/README.md](https://github.com/Handy4ndy/HandyHooks/blob/main/Blacklist/Trustee/README.md) — Trustee notes (brief).

Quick start (deploy & basic test)
1. Create Testnet accounts in Builder: one Provider account, one Trustee account, and at least one normal user and one bad_actor account (for negative tests).
2. Deploy Provider
   - In Builder → Developer paste `Provider/BlacklistProvider.c`, `Compile to WASM`, then Builder → Deploy → Set Hook under the Provider account.
   - Set `HookOn` triggers (Payments/Invoke) and provide install parameters if required.
3. Deploy Trustee
   - In Builder → Developer paste `Trustee/BlacklistTrustee.c`, compile, then Deploy under the Trustee account.
   - Configure `PROVIDER_ACC` (Invoke) pointing to the Provider hook account (use accountID hex via hooks.services conversion) and set `HookOn` appropriately.
4. Run test flows
   - Use Provider `Invoke` (ADD_BLACKLIST) to add the `bad_actor` AccountID to the Provider blacklist.
   - Send a Payment from `user` to the Trustee — expect success.
   - Send a Payment from `bad_actor` to the Trustee — expect rejection if the Trustee enforces blacklist checks.

Testing notes & gotchas
- HookParameter values that represent AccountIDs must be the 20‑byte hex AccountID bytes (not the human-readable r-address). Convert using https://hooks.services/tools/raddress-to-accountid.
- Remove inline comments from JSON before pasting into a strict JSON parser — place explanatory notes outside the JSON block when using Builder Test UI.
- Ensure `HookOn` triggers match the transaction types you will submit; mismatched triggers will prevent hook execution.
- Use Builder logs (`TRACESTR` / `TRACEHEX`) and Xahau Explorer for verification.

Examples and automation
- Example JSON payloads are included in the `deploy.md` files for both Provider and Trustee; they are intentionally simple so you can copy/paste into Builder → Test.
- For scripted tests and batch automation, use `xahau-latest-min.js` (included in XahauHooks101) or the Xahau Hook Tx Builder to craft and submit properly formatted transactions.

Contributing
- Keep examples minimal and testable. If you update Hook invoke keys or state layout in the `.c` files, please update the corresponding `deploy.md` example payloads.

Contact / References
- Xahau Hooks Builder: https://builder.xahau.network/develop
- HookOn calculator: https://richardah.github.io/xrpl-hookon-calculator/
- Xahau Explorer (Testnet): https://test.xahauexplorer.com/en
