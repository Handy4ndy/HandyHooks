# Build verification

Subscription and Create checked 5 October 2026 (UK time). Bids and Finalise (max bid release) checked 7 October 2026 (UK time). Nothing in this pack was edited or recompiled into the tree during these checks.

## Tools and sources

| Tool | Role in this check | Source |
| --- | --- | --- |
| hook-buildbox | Byte-for-byte pin rebuild (hosted compile) | [hook-buildbox.xrpl.org](https://hook-buildbox.xrpl.org/) |
| xahc | Static wasm lint (`xahc lint`) | [github.com/Hugegreencandle/xahc](https://github.com/Hugegreencandle/xahc), release **v1.12.0** |
| hook-repro | Hermetic byte-for-byte rebuild (`buildbox-2026-10`, with `xhc-bin127` for comparison) | [github.com/Hugegreencandle/hook-repro](https://github.com/Hugegreencandle/hook-repro), commit `2f25a6e` |

Integration tests use **xahau.js** only (`npm` dependency in `package.json`).

## Release pins

HookHash is sha512Half of the wasm (first 32 bytes of SHA-512, uppercase hex).

| Hook | File | Bytes | HookHash |
| --- | --- | ---: | --- |
| Subscription | `Subscription/AuctionSub.wasm` | 9149 | `6F6FD261881A285C9B038B46025809BBC7217E8EE904B914C2ED89EACBDCA68A` |
| Create | `Create/AuctionCreate.wasm` | 12274 | `D1D4BFCA240733EF3E00AB04E74481697D2C67C552A2994911732CC822869342` |
| Bids | `Bids/AuctionBids.wasm` | 22984 | `F73BCBF899147F512DB4768546DE75AED84235F3E4BA9D50C04BB460AAFF6832` |
| Finalise | `Finalise/AuctionFinalise.wasm` | 31464 | `D9881023127345F3DAFAC09744F6BE6952500EAF230FF0DC1A8FC28D0E66CC06` |

Guard checker worst-case counts on the build log: Bids hook 30540 / cbak 10549, Finalise hook 60917 / cbak 18124.

## 1. Reproducible build (hook-buildbox)

Each wasm is built from its single `.c` file plus the Handy Hooks custom header set (the same six headers that sit beside each hook). That set includes the `macro.h` where `PREPARE_PAYMENT_SIMPLE` takes a `sizeout` argument, plus the encode helpers the hooks use. The stock Xahau headers do not have these, so a build against them alone will fail.

To rebuild, submit each `.c` together with the six headers from its folder to the public [hook-buildbox](https://hook-buildbox.xrpl.org/) (one hook per build), then take the output wasm:

- `Subscription/AuctionSub.c`
- `Create/AuctionCreate.c`
- `Bids/AuctionBids.c`
- `Finalise/AuctionFinalise.c`

hook-buildbox compiles with clang, runs one wasm-opt pass, cleans the binary, and runs the guard checker. The server applies its own fixed optimisation settings, so the optimisation level chosen in the request does not change the output. For an offline rebuild, use the hook-repro `buildbox-2026-10` recipe in section 3.

A fresh rebuild of all four came out byte-identical to the pins:

| Hook | Rebuilt bytes | Rebuilt HookHash | Match |
| --- | ---: | --- | --- |
| Subscription | 9149 | `6F6FD261...DCA68A` | yes |
| Create | 12274 | `D1D4BFCA...869342` | yes |
| Bids | 22984 | `F73BCBF8...FF6832` | yes (7 Oct) |
| Finalise | 31464 | `D9881023...66CC06` | yes (7 Oct) |

## 2. Static lint (xahc)

Tool: [`xahc`](https://github.com/Hugegreencandle/xahc) **v1.12.0** (official release binary). Command: `xahc lint <wasm>` and `xahc lint --json <wasm>`.

All four exit 0 with **0 errors** (Bids and Finalise re-linted on the new pins 7 October). Each reports the same two notes, which are expected for this design:

- Warning `STATE_FOREIGN_WRITE`: the hooks write foreign state through a shared HookNamespace. That is how Create, Bids and Finalise share auction keys with Subscription. Install with the matching namespace and grants as described in the README.
- Info `FLOAT_USAGE`: IOU amounts use the XFL float API. The hooks handle and compare them with `float_*` calls only.

Lint is a static check. It is not proof of behaviour. Behaviour is covered by the integration tests below.

## 3. Hermetic rebuild (hook-repro)

Tool: [`hook-repro`](https://github.com/Hugegreencandle/hook-repro), commit `2f25a6e`, recipe `buildbox-2026-10` (preset `buildbox-2026-10`: `CLANG_OPT=-O3`, `WASMOPT=builder2025`, `STAGES=opt,clean`). Checked 7 October 2026 (UK time) in a `--network none` container.

Each `.c` was staged with the six custom headers beside it, in a scratch folder outside the pack. Nothing in the tree was written.

| Hook | Rebuilt bytes | Rebuilt HookHash | Delta vs pin | Match |
| --- | ---: | --- | ---: | --- |
| Subscription | 9149 | `6F6FD261...DCA68A` | 0 | yes |
| Create | 12274 | `D1D4BFCA...869342` | 0 | yes |
| Bids | 22984 | `F73BCBF8...FF6832` | 0 | yes |
| Finalise | 31464 | `D9881023...66CC06` | 0 | yes |

All four builds exit 0, pass the guard checker, and are byte-identical to the pins. So the pins are now reproduced in a sealed environment, not only on the hosted buildbox.

Why the earlier hook-repro builds did not match: the clang 15 driver quietly runs `wasm-opt -O3` on the linked output whenever `wasm-opt` is on PATH. hook-buildbox has `wasm-opt` on PATH, so every pin went through that extra pass before the explicit wasm-opt pass and hook-cleaner. The older `xhc-bin127` recipe ran clang without `wasm-opt` on PATH and skipped that hidden pass. `buildbox-2026-10` uses the same pinned clang, wasm-opt, hook-cleaner, guard checker and headers as `xhc-bin127`, and puts `wasm-opt` on PATH during the link. This finding and the recipe come from hook-repro (Hugegreencandle).

For comparison, `xhc-bin127` (commit `57a8efd`, `CLANG_OPT=-O3`, `WASMOPT=builder2025`, `STAGES=opt,clean`) was re-run on the same staged sources the same day. All four exit 0 and pass the guard checker, but none byte-match. Each is slightly larger: Sub +51, Create +43, Bids +58, Finalise +96 bytes. The Sub and Create deltas are the same as on 5 October.

## 4. Integration tests (testnet)

Live tests on Xahau testnet (NetworkID 21338), using xahau.js only. See the README for how to run them.

| Runner | Result |
| --- | --- |
| `IT_ALL.js` (full per-hook matrices, 7 October on the current pins) | **733 passed, 0 failed** (Subscription 62, Create 81, Bids 288, Finalise 302) |
| `IT_COMBINED.js` (one host, all four hooks in one SetHook, 7 October on the current pins) | **30 passed, 0 failed**, host `rMw9AFYX94f4ZU8UhAhtphrVcwPXZ11BTe`, trail in `TESTNET_PATH.md` and `IT_COMBINED.json` |

Both runs reported the HookHashes in the release pins table.

## What this does and does not prove

- The shipped wasm is exactly what this source and header set produce on hook-buildbox, and in the sealed hook-repro `buildbox-2026-10` recipe.
- The wasm passes static lint with no errors.
- The installed hooks behave as specified across the full testnet matrix.
- It does not prove a mainnet install. Check the on-ledger HookHash against the pins table after you install.
