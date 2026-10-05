# Auction House — Bids (`AuctionBids`)

Part of **Handy Hooks ~ Auction House** (public V2).

Bid or buy-now via Payment to the host with otxn param `AID` (32-byte auction namespace). Param-free install; shares Payment coexistence with Subscription (`SUB` vs `AID`).

## HookOn / install

- **HookOn:** Payment + Remit + Invoke (not Payment-only). Bid path is Payment+AID; gen-0 host Remit/Payment gated vs LCK; Invoke/Remit without host-outflow work passthrough. Escrow, Check, PayChan, Offer, SetHook, and AccountDelete stay unhooked.
- **Install params:** none for bids. The CLR clear needs `ADMIN` (20), not the host, and not either baked default (`raMjZ7ayJ3txQY75vQWr8RTzErAcUD3gee` or `r3CANwccnAMqEyYeBW3q7Gk9sMAfuYZe45`).
- **Wasm:** 18399 bytes
- **HookHash:** `8E5085A30AADFA0554A237442782299AEBDD03BCEF83B27CBBDCB55201F9497A`

Payment without `AID` / not to host → passthrough (Sub / donation). Payment with both `SUB` and `AID` → `NOPE("SUB and AID both set")`.

## Judgment (bid rules)

1. First bid: amount ≥ SP (missing SP → 0; still require amount > 0)
2. Later bids: if MB set → amount ≥ HIGH+MB; else amount > HIGH
3. Buy-now: amount ≥ BN → accept, refund any prior high (incl. self-BN), Remit URIToken to winner, set `ST=2` / `BNW=1` on URI cbak success. Seller paid only via Finalise
4. `HIGH` = actual paid principal; `WIN` = 20-byte bidder; `BCNT` = uint32 BE
5. Outbid refund: XAH Payment (with stored WDT); IOU Remit Amounts. LCK − prior only on refund cbak success
6. Already-high rebid rejected unless this Payment also hits buy-now
7. Seller and host cannot bid
8. Bidder must not have remits disabled or DepositAuth at entry
9. If bidder has `lsfRequireDestTag`, bid Payment must carry DestinationTag → stored as `WDT`
10. Currency must match Create CUR+ISS; partial pays rejected
11. `ST` must be 1; missing / not open / expired → reject

## Stranded refund (non-blocking)

A failed outbid refund is stored **per bidder** (strand key under AID) and mirrored on legacy `RFD`/`RFDA`. That bidder claims via Finalise Invoke. **Later bids, settle, and cancel are not frozen** by a stranded refund. Only in-flight `PEN`/`SPEN`, forensic `LCKU`, or URI-strand `SSF` block new bids.

A failed strand write leaves PEN set. A successful refund still clears PEN.

IOU float check (KVT #12): a bid is refused with `IOU amount invalid` if the stored IOU LCK is not a valid non-negative float, or the new LCK sum errors or would be negative. A negative LCK is never written. On the refund callback a negative result is LCK under, so it takes the forensic `LCKU` / `SSF` path.

Host gen-0 Remit (KVT #13): at most 3 Amounts. More than 3 returns `too many Remit amounts`. Each IOU entry is refused while that IOU has LCK above zero.

Host Payment checks SendMax as well as Amount. `SendMax not drops` if SendMax is present and not XAH drops. Extra XAH float remains the rule. LCK does not count emit fees or Remit reserves.

## AID keys

**Read (Create):** `ST`, `EXP`, `SP`, `MB`, `BN`, `CUR`, `ISS`, `SLR`, `URI`, (+ `WDT`/`RFD`/`PEN` as needed)

**Write:** `HIGH`, `WIN`, `BCNT`, `WDT`; buy-now uses `SPEN` then on URI cbak success `ST=2`, `BNW=1`, clear `URI`. Refund-only emit → `PEN`; cbak emit-fail → per-bidder strand + `RFD`+`RFDA`(+`RFDT`).

## Host local state

| Key | Meaning |
|-----|---------|
| `TBD` | 4 BE u32 — accepted normal bids (not buy-now) |
| `TBN` | 4 BE u32 — buy-now settles |
| `LCK` | 8 BE drops — locked XAH principal |
| IOU lock | `sha512Half(CUR||ISS)` → XFL bits |
| emit map | emit hash → AID\|amt\|prior\|wdt\|flags (refund/URI cbak) |

## Happy paths

- Normal bid → accept, optional refund of prior, update HIGH/WIN/BCNT, LCK+
- Buy-now → refund prior if any, Remit lot, ST=2/BNW on URI cbak success
- Outbid chain with DestinationTag preserved on refund
- New bid while a prior bidder has an unclaimed stranded refund → still accepted

## Clear a stuck marker (CLR)

Invoke with `AID` (32 bytes) and `CLR` (1 byte). Byte 1 drops PEN. Byte 2 drops SPEN. Byte 3 drops both. Only the installed ADMIN may call it. DONE `marker cleared`, including when the marker was already absent. NOPE `CLR bad` or `not admin`. No payment, no LCK change, and the strand is not deleted. This does not run if the hook was removed.

## Important NOPE

- `AID must be 32 bytes` / `SUB and AID both set`
- `auction not found` / `auction not open` / `auction expired`
- `seller cannot bid` / `buy-now already won`
- `refund in flight` / `pending in flight` (PEN/SPEN only)
- `LCK under forensic` / `URI deliver stranded` / `create pending`
- `SendMax not drops`
- `IOU amount invalid` (bad or negative IOU LCK float)
- `too many Remit amounts` (host gen-0 Remit with more than 3 Amounts)
- `CLR bad` / `not admin` / `baked ADMIN refused`
- DONE `marker cleared`
- Bidder remits disabled / DepositAuth / missing DestinationTag when required

## Integration tests

`IT_BIDS.js` — run from this directory (or via root `IT_ALL.js`).
