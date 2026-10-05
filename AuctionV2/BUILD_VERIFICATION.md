# Build verification

Checked 5 October 2026 (UK time) on the release pins below. Nothing in this pack was edited or recompiled into the tree during these checks.

## Tools and sources

| Tool | Role in this check | Source |
| --- | --- | --- |
| hook-buildbox | Byte-for-byte pin rebuild via `compile_hook.py` | [hook-buildbox.xrpl.org](https://hook-buildbox.xrpl.org/) |
| xahc | Static wasm lint (`xahc lint`) | [github.com/Hugegreencandle/xahc](https://github.com/Hugegreencandle/xahc): release **v1.12.0** |
| hook-repro | Hermetic rebuild attempt (`xhc-bin127`) | [github.com/Hugegreencandle/hook-repro](https://github.com/Hugegreencandle/hook-repro): commit `57a8efd` |

Integration tests use **xahau.js** only (`npm` dependency in `package.json`).

## Release pins

HookHash is sha512Half of the wasm (first 32 bytes of SHA-512, uppercase hex).

| Hook | File | Bytes | HookHash |
| --- | --- | ---: | --- |
| Subscription | `Subscription/AuctionSub.wasm` | 9149 | `6F6FD261881A285C9B038B46025809BBC7217E8EE904B914C2ED89EACBDCA68A` |
| Create | `Create/AuctionCreate.wasm` | 12274 | `D1D4BFCA240733EF3E00AB04E74481697D2C67C552A2994911732CC822869342` |
| Bids | `Bids/AuctionBids.wasm` | 18399 | `8E5085A30AADFA0554A237442782299AEBDD03BCEF83B27CBBDCB55201F9497A` |
| Finalise | `Finalise/AuctionFinalise.wasm` | 25813 | `5D1651156BC4348C8F0CF683AB22698AE7CFD556E1A13F10AA986A5A17619E2A` |

## 1. Reproducible build (hook-buildbox)

Each wasm is built from its single `.c` file plus the Handy Hooks custom header set (the same six headers that sit beside each hook). That set includes the `macro.h` where `PREPARE_PAYMENT_SIMPLE` takes a `sizeout` argument, plus the encode helpers the hooks use. The stock Xahau headers do not have these, so a build against them alone will fail.

```bash
python3 compile_hook.py Subscription/AuctionSub.c -o AuctionSub.wasm
python3 compile_hook.py Create/AuctionCreate.c -o AuctionCreate.wasm
python3 compile_hook.py Bids/AuctionBids.c -o AuctionBids.wasm
python3 compile_hook.py Finalise/AuctionFinalise.c -o AuctionFinalise.wasm
```

`compile_hook.py` posts the source and headers to the public [hook-buildbox](https://hook-buildbox.xrpl.org/), which compiles with clang, runs one wasm-opt pass, cleans the binary, and runs the guard checker. The server applies its own fixed optimisation settings, so the `--Os` switch does not change the output.

A fresh rebuild of all four came out byte-identical to the pins:

| Hook | Rebuilt bytes | Rebuilt HookHash | Match |
| --- | ---: | --- | --- |
| Subscription | 9149 | `6F6FD261...DCA68A` | yes |
| Create | 12274 | `D1D4BFCA...869342` | yes |
| Bids | 18399 | `8E5085A3...F9497A` | yes |
| Finalise | 25813 | `5D165115...619E2A` | yes |

## 2. Static lint (xahc)

Tool: [`xahc`](https://github.com/Hugegreencandle/xahc) **v1.12.0** (official release binary). Command: `xahc lint <wasm>` and `xahc lint --json <wasm>`.

All four exit 0 with **0 errors**. Each reports the same two notes, which are expected for this design:

- Warning `STATE_FOREIGN_WRITE`: the hooks write foreign state through a shared HookNamespace. That is how Create, Bids and Finalise share auction keys with Subscription. Install with the matching namespace and grants as described in the README.
- Info `FLOAT_USAGE`: IOU amounts use the XFL float API. The hooks handle and compare them with `float_*` calls only.

Lint is a static check. It is not proof of behaviour. Behaviour is covered by the integration tests below.

## 3. Hermetic rebuild (hook-repro)

Tool: [`hook-repro`](https://github.com/Hugegreencandle/hook-repro) (commit `57a8efd`), recipe `xhc-bin127`.

- With the custom headers staged beside each `.c`, all four hooks compile under the recipe. The closest settings (`CLANG_OPT=-O3`, `WASMOPT=builder2025`, `STAGES=opt,clean`) exit 0 and pass the guard checker.
- The output does **not** byte-match the pins. Each rebuild is slightly larger (Sub +51, Create +43, Bids +37, Finalise +101 bytes).
- The difference comes from the toolchain binaries. The recipe ships clang 15 and binaryen 108, which are not the same builds hook-buildbox runs. Changing flags and stages does not close the gap.

So hook-repro confirms that the source and headers build cleanly in a sealed environment, but it cannot confirm the pins until a recipe pins the same clang, wasm-opt and cleaner binaries as hook-buildbox. Until then, section 1 is the byte-for-byte reproduction path.

## 4. Integration tests (testnet)

Live tests on Xahau testnet (NetworkID 21338), using xahau.js only. See the README for how to run them.

| Runner | Result |
| --- | --- |
| `IT_ALL.js` (full per-hook matrices) | **423 passed, 0 failed** (Subscription 62, Create 81, Bids 112, Finalise 168) |
| `IT_COMBINED.js` (one host, all four hooks) | **30 passed, 0 failed**, trail in `TESTNET_PATH.md` and `IT_COMBINED.json` |

Both runs reported the HookHashes in the release pins table.

## What this does and does not prove

- The shipped wasm is exactly what this source and header set produce on hook-buildbox.
- The wasm passes static lint with no errors.
- The installed hooks behave as specified across the full testnet matrix.
- It does not prove a mainnet install. Check the on-ledger HookHash against the pins table after you install.
