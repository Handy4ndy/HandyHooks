# Auction House: Finalise (`AuctionFinalise`)

Part of **Handy Hooks ~ Auction House** (public V2).

Settle or cancel an auction via Invoke with otxn param `AID` (32-byte auction namespace). Shares HookNamespace with Sub / Create / Bids for FEE, TREASURY, LCK, TAC, and refund maps.

## HookOn / install

- **HookOn:** Invoke only. Escrow, Check, PayChan, Offer, SetHook, and AccountDelete stay unhooked. HookOn was not widened.
- **Install param:** `ADMIN` (20), same key as Subscription. Must not be the host. Must not be the baked default `r3CANwccnAMqEyYeBW3q7Gk9sMAfuYZe45` (`baked ADMIN refused`). Required when `LCKU` is set (migration: installs without ADMIN OK until LCKU hits). Also required for the CLR clear.
- **Wasm:** 25813 bytes
- **HookHash:** `5D1651156BC4348C8F0CF683AB22698AE7CFD556E1A13F10AA986A5A17619E2A`

Missing AID → passthrough (Sub admin Invokes coexist). Wrong-size AID → reject. `CNCL` without valid AID → `NOPE("CNCL needs AID")`.

## Callers

| Situation | Who may Invoke |
|-----------|----------------|
| Buy-now claim (`ST=2` + `BNW`) | seller or ADMIN |
| Buy-now URI-fail retry (`ST=1` + `SSF` + `BNW`) | seller, WIN, or ADMIN |
| Timed after `EXP` | seller, WIN, or ADMIN |
| Seller cancel (`CNCL=0x01`) | seller only |
| Stranded refund claim (per-bidder strand, or legacy `RFD`+`RFDA`) | owed bidder |
| Clear forensic `LCKU` | seller or ADMIN (no emit) |
| Clear stuck `PEN` / `SPEN` (`CLR`) | installed ADMIN only |

## Paths

1. **Buy-now claim**: URI already gone; pay seller (+treasury) from HIGH; LCK-; ACTIVE-1; clear AID
   - **Buy-now URI-fail retry** (KVT #15): the buy-now URIToken Remit failed, so Bids left `ST=1` + `SSF` + `BNW`. Seller, ADMIN, or the winner (WIN) Invokes to re-send the lot to WIN and pay out. The winner does not have to wait on the seller.
2. **Timed with bids**: Remit URI→WIN; pay seller (+treasury); LCK-; ACTIVE-1; clear AID
3. **Timed no bids**: Remit URI→seller; ACTIVE-1; clear AID; no LCK change
4. **Seller cancel** (`AID` + `CNCL=0x01`): `ST=1`, no bids, remaining ≥ DUR/2; URI→seller; no LCK change

Emit order (fail-closed): URI Remit (if needed) → treasury fee → seller remainder → then settle commit (LCK- / ACTIVE-1 / clear AID on cbak coverage).

## FEE / TREASURY (Create snapshot)

Create stamps `FEE` and `TREASURY` onto the auction. Finalise reads that stamp first so a later admin change cannot retax an open listing. Older auctions without a stamp still fall back to live host keys. Missing FEE or TREASURY → 100% seller; `FEE=0` → no treasury emit.

XAH payouts = Payment; IOU seller + treasury = Remit Amounts.

## Stranded refund claim

A failed outbid refund is stored per bidder and mirrored on `RFD`/`RFDA`. That bidder Invokes with `AID` to claim. **Other bids, settle, and cancel keep going.** `PEN` still blocks while a refund emit is in flight. No auto-RETRY. A failed strand write on Bids leaves PEN set. A successful refund still clears PEN.

## Clear a stuck marker (CLR)

Invoke with `AID` (32 bytes) and `CLR` (1 byte) while this hook is installed. Byte 1 drops PEN. Byte 2 drops SPEN. Byte 3 drops both. Only the installed ADMIN may call it. A missing CLR does not enter this path, so CNCL and settle stay. DONE `marker cleared`, including when the marker was already absent. NOPE `CLR bad` or `not admin`. No payment, no LCK change, and the strand is not deleted. This does not run if the hook was removed. Extra XAH float remains the rule. LCK does not count emit fees or Remit reserves.

## Seller cancel gates

Blocked while `PEN` / `LCKU` / `SSF` / `TSF` / `BNW` / in-flight settle; not seller; bids present; remaining time &lt; DUR/2; `CNCL` invalid. Not blocked by stranded RFD alone.

## Happy paths

- Timed settle with winner → URI to WIN, fee split, seller remainder, AID cleared
- Buy-now claim by seller/ADMIN after BN settle
- Buy-now URI-fail retry by seller, ADMIN, or WIN
- No-bid expiry → URI back to seller
- Cancel mid-auction under CNCL rules → URI back to seller
- Owed bidder claims stranded refund without freezing the auction

## Important NOPE / DONE

- `ADMIN install param required` / `ADMIN must not be the host` / `baked ADMIN refused`
- `CLR bad` / `not admin`
- DONE `marker cleared`
- `AID must be 32 bytes` / `CNCL needs AID` / `CNCL invalid`
- `auction not found` / cancel-* blockers (`cancel seller only`, `cancel PEN set`, ...)
- `buy-now finalise forbidden` (settled buy-now claim by anyone but seller or ADMIN; URI-fail retry by anyone but seller, ADMIN, or WIN)
- IOU LCK subtract that would go negative is LCK under (settle sets `SSF` + `LCKU`), never a negative LCK write (KVT #12 class)
- Settle blocked while PEN/SSF/LCKU as designed. A stranded refund does not block it.
- Otxn success often `Settlement pending` / `Cancel pending` until cbak commits

## Integration tests

`IT_FINALISE.js`: run from this directory (or via root `IT_ALL.js`). Includes shared-NS combined chain cases.
