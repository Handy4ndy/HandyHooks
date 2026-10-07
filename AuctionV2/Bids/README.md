# Auction House: Bids (`AuctionBids`)

Part of **Handy Hooks ~ Auction House** (public V2).

Bid or buy-now via Payment to the host with otxn param `AID` (32-byte auction namespace). Param-free install; shares Payment coexistence with Subscription (`SUB` vs `AID`).

## HookOn / install

- **HookOn:** Payment + Remit + Invoke (not Payment-only). Bid path is Payment+AID; gen-0 host Remit/Payment gated vs LCK; Invoke/Remit without host-outflow work passthrough. Escrow, Check, PayChan, Offer, SetHook, and AccountDelete stay unhooked.
- **Install params:** none for bids. The CLR clear needs `ADMIN` (20), not the host, and not either baked default (`raMjZ7ayJ3txQY75vQWr8RTzErAcUD3gee` or `r3CANwccnAMqEyYeBW3q7Gk9sMAfuYZe45`).
- **Wasm:** 22984 bytes
- **HookHash:** `F73BCBF899147F512DB4768546DE75AED84235F3E4BA9D50C04BB460AAFF6832`

Payment without `AID` / not to host -> passthrough (Sub / donation). Payment with both `SUB` and `AID` -> `NOPE("SUB and AID both set")`.

## Judgment (bid rules)

Every bid is a **hidden max bid**. `HIGH` holds the escrowed max of the leading seat. The visible price is in `PRC` (16 bytes: price || HIGH snapshot). `PRC` is valid only while its snapshot equals `HIGH`. Otherwise the price falls back to `HIGH`. Below, C = current price and inc = MB if set, else 1 drop for XAH, else 0 with a strict "greater than" for IOU.

1. First bid: amount >= SP (missing SP -> any amount > 0). Price = SP if set, else the bid amount. DONE `Max bid accepted`
2. Later bids from someone else: if MB set -> amount >= C+MB; else amount > C. Otherwise NOPE `below min increment` / `bid not above price` (first bid under SP: `below start price`)
3. **Outbid** (amount > HIGH): challenger takes the seat. Price = min(amount, HIGH + inc). Prior max refunded in full. DONE `Max bid accepted with prior refund`
4. **Underbid** (amount <= HIGH, tie goes to the incumbent): seat, `HIGH`, `WIN` and `WDT` unchanged. Price = min(amount + inc, HIGH). The challenger's whole payment is refunded (with its own DestinationTag). DONE `Bid did not exceed current max bid, payment returned`
5. **Self-raise** (Payment from the current WIN): top-up. New max = HIGH + amount. Amount must be >= MB when MB is set (XAH without MB: 1 drop minimum, IOU without MB: new max must be strictly greater). No refund, no emit, price unchanged. DONE `Max bid raised`. NOPE `raise below min increment`. If the new max reaches BN it is a buy-now at BN (see 6)
6. Buy-now: amount >= BN -> accept, refund any prior high (incl. self-BN), Remit URIToken to winner, set `ST=2` / `BNW=1` on URI cbak success. Price = BN (`PRC` = BN || paid). Any overpay above BN goes back to the winner at Finalise (remainder leg). Seller paid only via Finalise. DONE `Buy-now accepted` / `Buy-now accepted with prior refund`
7. `HIGH` = escrowed max (drops or XFL bits), `WIN` = 20-byte bidder, `BCNT` = uint32 BE
8. Refunds: XAH Payment (with stored WDT); IOU Remit Amounts. LCK - refund only on refund cbak success
9. **Refund cap:** `IFR` counts every Bids refund in flight (underbid plus outbid). An underbid is rejected with `too many refunds in flight` once `IFR` reaches 4. Outbids are never rejected by the cap. `IFR` blocks settle while above 0
10. **Host float:** a refund-causing bid is rejected with `host float low` unless host XAH balance >= XAH LCK + 2 XAH. For IOU refunds only the XAH emit reserve is checked
11. `PEN` (strand claim or legacy refund in flight) still blocks new bids with `refund in flight`
12. Seller and host cannot bid
13. Bidder must not have remits disabled or DepositAuth at entry
14. If bidder has `lsfRequireDestTag`, bid Payment must carry DestinationTag -> stored as `WDT`
15. Currency must match Create CUR+ISS; partial pays rejected
16. `ST` must be 1; missing / not open / expired -> reject

## Stranded refund (non-blocking)

A failed bid refund (outbid or underbid) is stored **per bidder** (strand key under AID) and mirrored on legacy `RFD`/`RFDA`. That bidder claims via Finalise Invoke. **Later bids, settle, and cancel are not frozen** by a stranded refund. Only in-flight `PEN`/`SPEN`, a full refund cap (`IFR` = 4, underbids only), forensic `LCKU`, or URI-strand `SSF` block new bids.

Bid refunds are tracked per refund in `IFR`, not in `PEN`. Each refund cbak (ok or stranded) takes `IFR` down by 1. A failed strand write leaves the count in place and sets forensic `LCKU` + `SSF`. For a strand-claim refund, a failed strand write leaves PEN set and a successful refund clears PEN.

IOU float check (KVT #12): a bid is refused with `IOU amount invalid` if the stored IOU LCK is not a valid non-negative float, or the new LCK sum errors or would be negative. A negative LCK is never written. On the refund callback a negative result is LCK under, so it takes the forensic `LCKU` / `SSF` path.

Host gen-0 Remit (KVT #13): at most 3 Amounts. More than 3 returns `too many Remit amounts`. Each IOU entry is refused while that IOU has LCK above zero.

Host Payment checks SendMax as well as Amount. `SendMax not drops` if SendMax is present and not XAH drops. Extra XAH float remains the rule. LCK does not count emit fees or Remit reserves.

## AID keys

**Read (Create):** `ST`, `EXP`, `SP`, `MB`, `BN`, `CUR`, `ISS`, `SLR`, `URI`, (+ `WDT`/`RFD`/`PEN` as needed)

**Write:** `HIGH`, `WIN`, `BCNT`, `WDT`, `PRC` (every accepted bid, 16 bytes price || HIGH snapshot), `IFR` (uint16 BE, +1 per refund emit, -1 on its cbak); buy-now uses `SPEN` then on URI cbak success `ST=2`, `BNW=1`, clear `URI`. Bid refund emit -> `IFR` += 1 (no `PEN`); cbak emit-fail -> per-bidder strand + `RFD`+`RFDA`(+`RFDT`).

## Host local state

| Key | Meaning |
|-----|---------|
| `TBD` | 4 BE u32: accepted normal bids (not buy-now) |
| `TBN` | 4 BE u32: buy-now settles |
| `LCK` | 8 BE drops: locked XAH principal |
| IOU lock | `sha512Half(CUR||ISS)` -> XFL bits |
| emit map | emit hash -> AID\|amt\|prior\|wdt\|flags (refund/URI cbak). Flag 0x20 = bid refund counted in `IFR` |

## Happy paths

- First bid -> accept at SP, update HIGH/WIN/BCNT/PRC, LCK+
- Outbid -> new seat, price = min(amount, old max + inc), prior max refunded
- Underbid -> seat kept, price climbs to min(amount + inc, max), challenger refunded
- Self-raise -> max goes up, price stays, no emit
- Buy-now -> refund prior if any, Remit lot, ST=2/BNW on URI cbak success
- Outbid chain with DestinationTag preserved on refund
- New bid while a prior bidder has an unclaimed stranded refund -> still accepted

## Clear a stuck marker (CLR)

Invoke with `AID` (32 bytes) and `CLR` (1 byte). `CLR` is a bitmask: 0x01 drops PEN, 0x02 drops SPEN, 0x04 drops IFR. Any value 0x01 to 0x07 combines them. 0x00 or anything above 0x07 is `CLR bad`. Only the installed ADMIN may call it. DONE `marker cleared`, including when the marker was already absent. NOPE `CLR bad` or `not admin`. No payment, no LCK change, and the strand is not deleted. This does not run if the hook was removed.

## Important NOPE

- `AID must be 32 bytes` / `SUB and AID both set`
- `auction not found` / `auction not open` / `auction expired`
- `seller cannot bid` / `buy-now already won`
- `refund in flight` / `pending in flight` (PEN/SPEN only)
- `below start price` / `below min increment` / `bid not above price` / `raise below min increment`
- `too many refunds in flight` (underbid while IFR = 4) / `host float low`
- `IFR overflow` / `IFR write failed` / `PRC write failed` / `max overflow`
- `LCK under forensic` / `URI deliver stranded` / `create pending`
- `SendMax not drops`
- `IOU amount invalid` (bad or negative IOU LCK float)
- `too many Remit amounts` (host gen-0 Remit with more than 3 Amounts)
- `CLR bad` / `not admin` / `baked ADMIN refused`
- DONE `Max bid accepted` / `Max bid accepted with prior refund` / `Bid did not exceed current max bid, payment returned` / `Max bid raised` / `Buy-now accepted` / `Buy-now accepted with prior refund` / `marker cleared`
- Bidder remits disabled / DepositAuth / missing DestinationTag when required

## Integration tests

`IT_BIDS.js`: run from this directory (or via root `IT_ALL.js`).
