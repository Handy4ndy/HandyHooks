# Handy Hooks ~ Auction House

**Handy Hooks** Auction House (public **V2**) is a four-hook set for running URIToken auctions on a single Xahau host account.

Sellers subscribe, list a token into hook custody, accept bids (XAH or IOU), and settle through one shared **HookNamespace**. The host never needs to sign settlement: Finalise does it on-chain from hook state.

---

## How the auction chain works

```
Subscribe  →  Create  →  Bid  →  Finalise
   Sub          Create      Bids     Finalise
```

1. **Subscribe**: A seller pays a subscription fee to the host. That opens a time window and an auction cap. An admin configures price, period, treasury split, and fee settings on the Subscription hook.
2. **Create**: The seller Remits exactly one non-burnable URIToken to the host with auction parameters (duration, optional start price, min increment, buy-now, currency). The Create hook takes custody of the lot and opens the auction.
3. **Bid**: Bidders pay the host with the auction id. Higher bids refund the previous high bidder. A buy-now bid can end the auction immediately and hand the token to the winner.
4. **Finalise**: After expiry (or after buy-now), the seller, winner, or admin Invokes Finalise with the auction id. The hook delivers the URIToken, pays the seller and treasury, and clears auction state.

All four hooks must share the **same HookNamespace** so subscription, auction records, fees, and locked bid funds stay consistent.

---

## The four hooks

| Hook | Role | Directory | Wasm | HookHash |
|------|------|-----------|------|----------|
| **Subscription** | Seller access + admin config | `Subscription/` | 9149 | `6F6FD261...CBDCA68A` |
| **Create** | Open an auction (URIToken into custody) | `Create/` | 12274 | `D1D4BFCA...22869342` |
| **Bids** | Accept bids and outbid refunds | `Bids/` | 18399 | `8E5085A3...01F9497A` |
| **Finalise** | Settle, cancel, or claim stranded refunds | `Finalise/` | 25813 | `5D165115...17619E2A` |

Full hashes (sha512Half):

- Subscription: `6F6FD261881A285C9B038B46025809BBC7217E8EE904B914C2ED89EACBDCA68A`
- Create: `D1D4BFCA240733EF3E00AB04E74481697D2C67C552A2994911732CC822869342`
- Bids: `8E5085A30AADFA0554A237442782299AEBDD03BCEF83B27CBBDCB55201F9497A`
- Finalise: `5D1651156BC4348C8F0CF683AB22698AE7CFD556E1A13F10AA986A5A17619E2A`

Each directory holds the hook source (`.c`), the pinned wasm, headers, and that hook's integration tests. See the README in each folder for parameters, HookOn, and result strings.

---

## Install (overview)

Install in this order, all on one host, all with the **same HookNamespace**:

1. **Subscription**: set install param `ADMIN` to your operator account (not the host, not the baked default)
2. **Create**: no install params
3. **Bids**: optional same `ADMIN` if you want the CLR clear on this hook
4. **Finalise**: same `ADMIN` AccountID as Subscription

Then admin-Invoke Subscription to set `SUBPRICE`, `SUBPERIOD`, `SUBSPLIT`, `AUCCAP`, `TREASURY`, and `FEE` before sellers can subscribe.

Recommended HookOn:

| Hook | HookOn |
|------|--------|
| Subscription | Payment + Invoke |
| Create | Payment + Remit + Invoke |
| Bids | Payment + Remit + Invoke |
| Finalise | Invoke |

Create and Bids also gate host outflows against locked bid principal (LCK). Do not install Create as Remit-only or Bids as Payment-only: that gate will not run. Escrow, Check, PayChan, Offer, SetHook, and AccountDelete stay unhooked by design.

---

## Fees and treasury

Admin sets host-level `FEE` (basis points of the winning bid, max 50%) and `TREASURY` on the Subscription hook.

Create **stamps** the current fee and treasury onto each new auction. Finalise uses that stamp, so changing live settings later does not retax an open listing. If fee or treasury is missing at create time, the seller receives 100%. A stamped `FEE` of 0 means no treasury cut.

Subscription payments can also split a share to treasury via `SUBSPLIT`.

---

## Seller cancel

While an auction is open, has **no bids**, and at least half of its duration remains, the seller may Invoke Finalise with `AID` and `CNCL` to reclaim the URIToken. Cancel does not change LCK. A stranded outbid refund does not block cancel.

---

## Operator notes

These match the wasm on disk. They are install and runtime rules, not open gaps.

- Install **ADMIN** must not be the baked default (`baked ADMIN refused`). Details are in the Subscription and Finalise READMEs.
- Bid principal is locked on the host as **LCK** (XAH or per-IOU). Keep extra XAH float for emit fees and Remit reserves: LCK is face-value principal only.
- Host Payment checks **SendMax** as well as Amount when present.
- **Create** refuses non-zero TransferRate, global freeze, clawback, and an existing issuer-side freeze on the host IOU line.
- Host gen-0 Remit allows at most **3** Amounts.
- A stranded outbid refund does **not** freeze later bids. The owed bidder can claim it through Finalise.
- Buy-now URI-fail Finalise retry: seller, Sub ADMIN, or winner.

---

## Acknowledgements

Thanks to **KVT** for the assessments that shaped this release. Findings from that review are closed in this pack.

---

## Integration tests

Live **testnet** integration tests (NetworkID 21338 / `xahau-test.net`) install the hooks, send real transactions, and assert results and ledger state. They are the formal test layer for this release. They are not unit tests and they do not prove a mainnet install.

Requires Node 18+ and **xahau.js** only.

```bash
npm install
npm run it:combined   # one-host publishable path → TESTNET_PATH.md
npm run it            # full per-hook matrices
```

Or without npm scripts:

```bash
node IT_COMBINED.js
node IT_ALL.js
```

Per hook: `npm run it:sub` / `it:create` / `it:bids` / `it:finalise`.

**Publish proof:** `TESTNET_PATH.md` + `IT_COMBINED.json`  
**Build verification:** `BUILD_VERIFICATION.md` (byte-for-byte rebuild of all four pins, lint, and test totals)

`package.json` pins `xahau`. `node_modules`, build logs, and per-hook integration JSON stay out of the publish zip (see `.gitignore`).

---

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
