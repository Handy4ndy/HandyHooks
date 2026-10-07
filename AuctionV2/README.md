# Handy Hooks ~ Auction House

**Handy Hooks** Auction House (public **V2**) is a four-hook set for running URIToken auctions on a single Xahau host account.

Sellers subscribe, list a token into hook custody, accept bids (XAH or IOU), and settle through one shared **HookNamespace**. The host never needs to sign settlement. Finalise does it on-chain from hook state.

---

## How the auction chain works

```
Subscribe  ->  Create  ->  Bid  ->  Finalise
   Sub          Create      Bids     Finalise
```

1. **Subscribe**: A seller pays a subscription fee to the host. That opens a time window and an auction cap. An admin configures price, period, treasury split, and fee settings on the Subscription hook.
2. **Create**: The seller Remits exactly one non-burnable URIToken to the host with auction parameters (duration, optional start price, min increment, buy-now, currency). The Create hook takes custody of the lot and opens the auction.
3. **Bid**: Bidders pay the host with the auction id. Every bid is a **hidden max bid**: you pay your max up front, but the visible price only goes as high as it needs to beat the other bidders.
   - The first bid sets the price at the start price (or at the bid itself when there is no start price).
   - A bid above the current max takes the lead. The price becomes the old max plus one increment (never more than the new max), and the previous leader is refunded in full.
   - A bid at or below the current max does not take the lead. It pushes the price up toward the leader's max and the whole payment is sent straight back. A tie goes to the earlier bidder.
   - The leader can **raise** their own max at any time without changing the price.
   - At most 4 bid refunds can be in flight per auction. Once that many are waiting, losing bids are turned away until they land. Bids that take the lead are never turned away.
   - A bid that would cause a refund is turned away with `host float low` if the host does not hold at least the locked XAH plus 2 XAH.
   - A buy-now bid can end the auction immediately and hand the token to the winner. Anything paid above the buy-now price is returned at Finalise.
4. **Finalise**: After expiry (or after buy-now), anyone allowed (seller, winner, or admin) Invokes Finalise with the auction id. The hook delivers the URIToken, pays the seller and treasury from the final price, sends the winner back the difference between their max and the price, and clears auction state.

All four hooks must share the **same HookNamespace** so subscription, auction records, fees, and locked bid funds stay consistent.

---

## The four hooks

| Hook | Role | Directory | Wasm | HookHash |
|------|------|-----------|------|----------|
| **Subscription** | Seller access + admin config | `Subscription/` | 9149 | `6F6FD261...CBDCA68A` |
| **Create** | Open an auction (URIToken into custody) | `Create/` | 12274 | `D1D4BFCA...22869342` |
| **Bids** | Accept max bids, refunds, and buy-now | `Bids/` | 22984 | `F73BCBF8...AAFF6832` |
| **Finalise** | Settle, cancel, or claim stranded refunds | `Finalise/` | 31464 | `D9881023...0E66CC06` |

Full hashes (sha512Half):

- Subscription: `6F6FD261881A285C9B038B46025809BBC7217E8EE904B914C2ED89EACBDCA68A`
- Create: `D1D4BFCA240733EF3E00AB04E74481697D2C67C552A2994911732CC822869342`
- Bids: `F73BCBF899147F512DB4768546DE75AED84235F3E4BA9D50C04BB460AAFF6832`
- Finalise: `D9881023127345F3DAFAC09744F6BE6952500EAF230FF0DC1A8FC28D0E66CC06`

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

Create and Bids also gate host outflows against locked bid principal (LCK). Do not install Create as Remit-only or Bids as Payment-only. That gate will not run. Escrow, Check, PayChan, Offer, SetHook, and AccountDelete stay unhooked by design.

---

## Fees and treasury

Admin sets host-level `FEE` (basis points of the final price, max 50%) and `TREASURY` on the Subscription hook.

Create **stamps** the current fee and treasury onto each new auction. Finalise uses that stamp, so changing live settings later does not retax an open listing. If fee or treasury is missing at create time, the seller receives 100%. A stamped `FEE` of 0 means no treasury cut.

Subscription payments can also split a share to treasury via `SUBSPLIT`.

---

## Seller cancel

While an auction is open, has **no bids**, and at least half of its duration remains, the seller may Invoke Finalise with `AID` and `CNCL` to reclaim the URIToken. Cancel does not change LCK. A stranded outbid refund does not block cancel.

---

## Operator notes

These match the wasm on disk. They are install and runtime rules, not open gaps.

- Install **ADMIN** must not be the baked default (`baked ADMIN refused`). Details are in the Subscription and Finalise READMEs.
- Bid principal is locked on the host as **LCK** (XAH or per-IOU). Keep extra XAH float for emit fees and Remit reserves. LCK is face-value principal only. A bid that would cause a refund needs host XAH of at least LCK + 2 XAH (`host float low`).
- Host Payment checks **SendMax** as well as Amount when present.
- **Create** refuses non-zero TransferRate, global freeze, clawback, and an existing issuer-side freeze on the host IOU line.
- Host gen-0 Remit allows at most **3** Amounts.
- A stranded bid refund does **not** freeze later bids; the owed bidder can claim it through Finalise.
- Up to 4 bid refunds may be in flight per auction (`IFR`). Settle waits until they land. If one ever sticks, the admin can clear it with `CLR` 0x04 (details in the Bids and Finalise READMEs).
- A winner remainder that cannot be delivered is kept as a strand the winner claims through Finalise.
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
npm run it:combined   # one-host publishable path -> TESTNET_PATH.md
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
