# Handy Hooks ~ Auction House

**Handy Hooks** — Auction House hook set (public **V2**; first published release).

Four Xahau hooks that run a URIToken auction on one host account. They share one **HookNamespace** so seller subscription, auction state, FEE/TREASURY, and LCK lock accounting stay consistent.

| Hook | Dir | HookOn (recommended) | Install params | Wasm bytes | HookHash (sha512Half) |
|------|-----|----------------------|----------------|------------|------------------------|
| Subscription | `Subscription/` | Payment + Invoke | `ADMIN` (20), not the baked default | 9149 | `6F6FD261881A285C9B038B46025809BBC7217E8EE904B914C2ED89EACBDCA68A` |
| Create | `Create/` | Payment + Remit + Invoke | none | 12274 | `D1D4BFCA240733EF3E00AB04E74481697D2C67C552A2994911732CC822869342` |
| Bids | `Bids/` | Payment + Remit + Invoke | `ADMIN` (20) only for the CLR clear | 18399 | `8E5085A30AADFA0554A237442782299AEBDD03BCEF83B27CBBDCB55201F9497A` |
| Finalise | `Finalise/` | Invoke | `ADMIN` (20), not the baked default | 25813 | `5D1651156BC4348C8F0CF683AB22698AE7CFD556E1A13F10AA986A5A17619E2A` |

Canonical sources live under each hook directory (`.c` + `.wasm` + integration tests). Subscription is the only Sub source — do not keep root `AuctionSub.*` copies.

## Install order

1. **Subscription** (override `ADMIN` to your operator account, then admin Invokes for SUBPRICE / SUBPERIOD / SUBSPLIT / AUCCAP / TREASURY / FEE)
2. **Create**
3. **Bids** (pass the same `ADMIN` only if you want the CLR clear on this hook)
4. **Finalise** (same `ADMIN` AccountID as Sub)

All four must use the **same HookNamespace**.

### ADMIN override (required)

Install ADMIN must not be the baked default. If the install param is still that account, the hook returns `baked ADMIN refused`. A different 20-byte AccountID still works, and it must not be the host.

Baked accounts:

- Subscription: `raMjZ7ayJ3txQY75vQWr8RTzErAcUD3gee`
- Finalise: `r3CANwccnAMqEyYeBW3q7Gk9sMAfuYZe45`

The Bids CLR clear checks both of those baked accounts and refuses either one.

## HookOn truth

Integration tests and recommended installs use:

- Sub: Payment + Invoke (host Remit/Payment LCK gates are on Create/Bids)
- Create / Bids: Payment + Remit + Invoke — Create opens on seller Remit; both also gate **gen-0 host** Payment/Remit outflows against LCK, and passthrough other tt
- Finalise: Invoke only

Escrow, Check, PayChan, Offer, SetHook, and AccountDelete stay unhooked. HookOn does not cover them. Do not widen HookOn to gate those types.

Do not install Create as "Remit only" or Bids as "Payment only". The host LCK spend gate will not fire.

## Roles

- **Subscription** — seller pays `SUB` for a time window + auction cap; admin configures prices, treasury split, and Finalise fee bps. Payment with both `SUB` and `AID` is rejected.
- **Create** — seller Remits exactly one non-burnable URIToken to host with `DUR` (+ optional SP/MB/BN/CUR/ISS). Requires active subscription. IOU TrustSet uses `tfSetNoRipple`. Stamps current `FEE` + `TREASURY` onto the auction.
- **Bids** — Payment to host with `AID` (32-byte auction ns). Outbid refunds prior; buy-now Remits the lot and sets `ST=2`. A stranded refund does **not** freeze later bids.
- **Finalise** — Invoke with `AID` to settle (seller / winner / ADMIN). Seller cancel via `AID` + `CNCL=0x01` when open, no bids, and remaining time ≥ half of DUR. Stranded refund claim is pullable by the owed bidder and does not block settle/cancel.

## Shared admin / money keys

Written by Subscription admin Invokes; snapshotted by Create onto each auction; Finalise prefers the auction stamp:

- `FEE` — uint16 BE basis points of HIGH, 0..5000
- `TREASURY` — 20-byte account (must not be host)
- Missing FEE or TREASURY at create → 100% to seller; `FEE=0` stamp → no treasury emit
- Changing live FEE/TREASURY after Create does **not** retax an open listing

## LCK (high level)

Host-local lock of accepted bid principal:

- XAH: key `LCK` (8 BE drops)
- IOU: key `sha512Half(CUR||ISS)`, value XFL bits

Bids add principal on accept and subtract the prior seat only on refund **cbak success** (or Finalise stranded-claim cbak). Finalise subtracts HIGH when paying out a won auction. Fail-closed on under/overflow; forensic `LCKU` / `SSF` strands need seller or ADMIN ack before further settle.

LCK stays face value. It does not count emit fees, destination-hook fees, or Remit reserves. Extra XAH float remains the rule.

A host gen-0 Remit may carry at most 3 Amounts. More than 3 returns `too many Remit amounts`. Each IOU entry is refused while that IOU has LCK above zero (`Insufficient spendable float`).

The host Payment gate on Create and Bids checks SendMax as well as Amount. The lock check uses the larger of the two when SendMax is XAH drops. If SendMax is present and is not XAH drops, the hook returns `SendMax not drops`. If SendMax is absent, the Amount check stays.

## Seller cancel (CNCL)

Finalise Invoke with `AID` + `CNCL` (1 byte `0x01`): seller only; auction open (`ST=1`); no bids; remaining time ≥ `DUR/2`. Returns URIToken to seller; no LCK change. Blocked while PEN/LCKU/SSF/TSF/BNW or in-flight settle. A stranded outbid refund does **not** block cancel.

## Operator notes

These match the wasm on disk. They are install and runtime rules, not open gaps.

- Install **ADMIN** must not be the baked default (`baked ADMIN refused`). Subscription baked: `raMjZ7ayJ3txQY75vQWr8RTzErAcUD3gee`. Finalise baked: `r3CANwccnAMqEyYeBW3q7Gk9sMAfuYZe45`.
- Host Payment gate checks **SendMax** as well as Amount (`SendMax not drops` when SendMax is present and not XAH drops).
- **Create** refuses non-zero TransferRate, global freeze, clawback, and an existing issuer-side freeze on the host line.
- ADMIN **CLR** clears stuck PEN/SPEN while the hook is still installed. Drain in-flight emits before SetHook is still operator hygiene.
- A failed strand write leaves PEN set; a successful refund still clears PEN so later bids are not frozen.
- Extra **XAH float** remains the fee/reserve rule. LCK is face-value principal only.
- Escrow, Check, PayChan, Offer, SetHook, and AccountDelete stay **unhooked**.
- Host gen-0 Remit: at most **3 Amounts** (`too many Remit amounts` above that).
- Bad IOU LCK float on bid: `IOU amount invalid`. Buy-now URI-fail Finalise retry: seller, Sub ADMIN, or **WIN**.

## Acknowledgements

Thanks to **KVT** for the assessments that shaped this release. Findings from that review are closed in this pack.

## Integration tests

Live **testnet** integration tests (NetworkID 21338 / `xahau-test.net`). They install the hooks, send real txs, and assert DONE/NOPE and ledger state. They are the formal test layer for this release. They are **not** unit tests and they do **not** prove a mainnet install.

Use **xahau.js** only (`Client`, `Wallet`, `decodeAccountID`). Node 18+.

```bash
npm install
npm run it:combined   # publishable one-host testnet path (ships TESTNET_PATH.md)
npm run it            # full per-hook matrices
```

Same runners without npm scripts:

```bash
node IT_COMBINED.js
node IT_ALL.js
```

Per hook: `npm run it:sub` / `it:create` / `it:bids` / `it:finalise`, or the matching `*/IT_*.js` files.

**Publish proof:** `TESTNET_PATH.md` + `IT_COMBINED.json` (NetworkID 21338, host, HookHashes, tx hashes).

**Build verification:** see `BUILD_VERIFICATION.md` for the byte-for-byte rebuild of all four pins, xahc lint results, the hook-repro result, and the test totals.

`package.json` pins `xahau`. `node_modules`, build logs, and per-hook integration JSON stay out of the publish zip (see `.gitignore`). Ship `TESTNET_PATH.md` + combined JSON only. Matrix JSON / run logs also live under `_local_artifacts/` on the build machine.

## Layout

```
Auction House (V2)/
  README.md
  BUILD_VERIFICATION.md
  package.json
  .gitignore
  IT_COMBINED.js
  IT_COMBINED.json
  IT_ALL.js
  Subscription/   AuctionSub.c|.wasm  headers  IT_SUB.js  README.md
  Create/         AuctionCreate.c|.wasm  headers  IT_CREATE.js  README.md
  Bids/           AuctionBids.c|.wasm  headers  IT_BIDS.js  README.md
  Finalise/       AuctionFinalise.c|.wasm  headers  IT_FINALISE.js  README.md
```
