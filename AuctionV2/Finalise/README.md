# Auction House: Finalise (`AuctionFinalise`)

Part of **Handy Hooks ~ Auction House** (public V2).

Settle or cancel an auction via Invoke with otxn param `AID` (32-byte auction namespace). Shares HookNamespace with Sub / Create / Bids for FEE, TREASURY, LCK, TAC, and refund maps.

## HookOn / install

- **HookOn:** Invoke only. Escrow, Check, PayChan, Offer, SetHook, and AccountDelete stay unhooked. HookOn was not widened.
- **Install param:** `ADMIN` (20), same key as Subscription. Must not be the host. Must not be the baked default `r3CANwccnAMqEyYeBW3q7Gk9sMAfuYZe45` (`baked ADMIN refused`). Required when `LCKU` is set (migration: installs without ADMIN OK until LCKU hits). Also required for the CLR clear.
- **Wasm:** 31464 bytes
- **HookHash:** `D9881023127345F3DAFAC09744F6BE6952500EAF230FF0DC1A8FC28D0E66CC06`

Missing AID -> passthrough (Sub admin Invokes coexist). Wrong-size AID -> reject. `CNCL` without valid AID -> `NOPE("CNCL needs AID")`.

## Callers

| Situation | Who may Invoke |
|-----------|----------------|
| Buy-now claim (`ST=2` + `BNW`) | seller, WIN, or ADMIN (the winner can pull an overpay remainder) |
| Buy-now URI-fail retry (`ST=1` + `SSF` + `BNW`) | seller, WIN, or ADMIN |
| Timed after `EXP` | seller, WIN, or ADMIN |
| Seller cancel (`CNCL=0x01`) | seller only |
| Stranded refund claim (per-bidder strand, or legacy `RFD`+`RFDA`) | owed bidder |
| Stranded remainder claim (winner strand) | winner |
| Clear forensic `LCKU` | seller or ADMIN (no emit) |
| Clear stuck `PEN` / `SPEN` / `IFR` (`CLR`) | installed ADMIN only |

## Paths

1. **Buy-now claim**: URI already gone; pay seller (+treasury) from the price; remainder (HIGH - price) to WIN; LCK-; ACTIVE-1; clear AID
   - **Buy-now URI-fail retry** (KVT #15): the buy-now URIToken Remit failed, so Bids left `ST=1` + `SSF` + `BNW`. Seller, ADMIN, or the winner (WIN) Invokes to re-send the lot to WIN and pay out. The winner does not have to wait on the seller.
2. **Timed with bids**: Remit URI->WIN; pay seller (+treasury) from the price; remainder (HIGH - price) to WIN; LCK-; ACTIVE-1; clear AID
3. **Timed no bids**: Remit URI->seller; ACTIVE-1; clear AID; no LCK change
4. **Seller cancel** (`AID` + `CNCL=0x01`): `ST=1`, no bids, remaining >= DUR/2; URI->seller; no LCK change

Emit order (fail-closed): URI Remit (if needed) -> treasury fee -> seller payout -> winner remainder (only when HIGH > price) -> then settle commit (LCK- / ACTIVE-1 / clear AID on cbak coverage).

## Price and remainder

`HIGH` is the winner's escrowed max. The settle price is `PRC.price` when `PRC` (16 bytes, price || HIGH snapshot) is present and its snapshot equals `HIGH`. Otherwise a buy-now uses `BN` (when BN <= HIGH) and every other path uses `HIGH`. Belt: 0 < price <= HIGH, else `price state corrupt`.

Seller gets price minus fee. The winner gets `HIGH - price` back as a remainder leg (SMAP kind 5, SPEN/SEXP bit 0x10, `WPAY` = 1 on its cbak). A failed remainder becomes a winner strand, claimed later via Invoke `AID`. It does not block seller, treasury, lot or commit. Cbak DONE `remainder cbak ok`, or `remainder stranded` / `remainder strand failed` when it could not be delivered.

`IFR` (Bids refunds in flight) blocks settle with `refunds in flight` and cancel with `cancel refunds in flight`.

## FEE / TREASURY (Create snapshot)

Create stamps `FEE` and `TREASURY` onto the auction. Finalise reads that stamp first so a later admin change cannot retax an open listing. Older auctions without a stamp still fall back to live host keys. FEE is basis points of the final price (not of HIGH). Missing FEE or TREASURY -> 100% seller; `FEE=0` -> no treasury emit.

XAH payouts = Payment; IOU seller, treasury and remainder = Remit Amounts.

## Stranded refund claim

A failed bid refund (outbid or underbid) is stored per bidder and mirrored on `RFD`/`RFDA`. That bidder Invokes with `AID` to claim. **Other bids, settle, and cancel keep going.** `PEN` still blocks while a claim refund emit is in flight. Bids refunds are counted in `IFR` instead. No auto-RETRY. A failed strand write on Bids leaves PEN set. A successful refund still clears PEN.

## Clear a stuck marker (CLR)

Invoke with `AID` (32 bytes) and `CLR` (1 byte) while this hook is installed. `CLR` is a bitmask: 0x01 drops PEN, 0x02 drops SPEN, 0x04 drops IFR. Any value 0x01 to 0x07 combines them. 0x00 or anything above 0x07 is `CLR bad`. Only the installed ADMIN may call it. A missing CLR does not enter this path, so CNCL and settle stay. DONE `marker cleared`, including when the marker was already absent. NOPE `CLR bad` or `not admin`. No payment, no LCK change, and the strand is not deleted. This does not run if the hook was removed. Extra XAH float remains the rule. LCK does not count emit fees or Remit reserves.

## Seller cancel gates

Blocked while `PEN` / `IFR` / `LCKU` / `SSF` / `TSF` / `BNW` / in-flight settle; not seller; bids present; remaining time &lt; DUR/2; `CNCL` invalid. Not blocked by stranded RFD alone.

## Happy paths

- Timed settle with winner -> URI to WIN, fee split on the price, seller payout, winner remainder, AID cleared
- Buy-now claim by seller, WIN, or ADMIN after BN settle (overpay above BN returned)
- Buy-now URI-fail retry by seller, ADMIN, or WIN
- No-bid expiry -> URI back to seller
- Cancel mid-auction under CNCL rules -> URI back to seller
- Owed bidder claims stranded refund without freezing the auction

## Important NOPE / DONE

- `ADMIN install param required` / `ADMIN must not be the host` / `baked ADMIN refused`
- `CLR bad` / `not admin`
- DONE `marker cleared`
- `AID must be 32 bytes` / `CNCL needs AID` / `CNCL invalid`
- `auction not found` / cancel-* blockers (`cancel seller only`, `cancel PEN set`, `cancel refunds in flight`, ...)
- `refunds in flight` (IFR > 0) / `price state corrupt` / `IFR write failed`
- `remainder emit failed` (NOPE) / cbak DONE `remainder cbak ok`, `remainder stranded`, `remainder strand failed`
- `buy-now finalise forbidden` (settled buy-now claim by anyone but seller, WIN, or ADMIN; URI-fail retry by anyone but seller, ADMIN, or WIN)
- IOU LCK subtract that would go negative is LCK under (settle sets `SSF` + `LCKU`), never a negative LCK write (KVT #12 class)
- Settle blocked while PEN/SSF/LCKU as designed. A stranded refund does not block it.
- Otxn success often `Settlement pending` / `Cancel pending` until cbak commits

## Integration tests

`IT_FINALISE.js`: run from this directory (or via root `IT_ALL.js`). Includes shared-NS combined chain cases.
