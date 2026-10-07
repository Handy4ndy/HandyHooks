# Auction House: Create (`AuctionCreate`)

Part of **Handy Hooks ~ Auction House** (public V2).

Open an auction by Remitting exactly one URIToken to the host. Param-free install; shares HookNamespace with Sub / Bids / Finalise.

## HookOn / install

- **HookOn:** Payment + Remit + Invoke (not Remit-only). Seller Create is Remit; gen-0 host Payment/Remit are gated vs LCK; other Payments/Invokes passthrough. Escrow, Check, PayChan, Offer, SetHook, and AccountDelete stay unhooked.
- **Install params:** none
- **Wasm:** 12274 bytes
- **HookHash:** `D1D4BFCA240733EF3E00AB04E74481697D2C67C552A2994911732CC822869342`

## Remit params

| Param | Req | Size / notes |
|-------|-----|----------------|
| `DUR` | yes | 8 BE uint64 seconds, 300 .. 2592000 (30d) |
| `SP` | opt | 8: start price (XAH drops or IOU XFL) |
| `MB` | opt | 8: min bid increment; if set must be > 0 |
| `BN` | opt | 8: buy-now; if SP+BN set, BN > SP |
| `CUR` | opt | 3 ASCII / 20 raw / 40 hex; omit = XAH |
| `ISS` | opt | 20 AccountID; required with CUR for IOU |

## Seller gates (subscription)

Reads seller foreign ns (same as Sub): `SUBEXP`, `ACTIVE`, `CAP`. Requires `SUBEXP > ledger_last_time` and `ACTIVE < CAP`; on success `ACTIVE += 1`.

## AID namespace

`AID = sha512h(otxn_id || URIToken id)`. Keys under AID (hook-owned):

`DUR`, `SP`, `MB`, `BN`, `CUR`, `ISS` (if present), `SLR`, `URI`, `EXP`, `ST=1` (open), plus stamped `FEE` / `TREASURY` when present on host.

## Create gates (fail-closed)

1. URIToken must not be burnable (`lsfBurnable`)
2. Seller must not have remits disabled (`asfDisallowIncomingRemit`)
3. Seller must not have DepositAuth (`lsfDepositAuth`): so Finalise payouts can land

IOU: if host has no trustline for CUR+ISS, emit TrustSet (large limit, **`tfSetNoRipple`**). Fail-closed on emit / required state write / ACTIVE bump failure.

IOU Create also refuses a bad issuer before auction keys are written. Non-zero TransferRate returns `transfer rate set`. Global freeze, or an existing issuer-side freeze (including deep freeze) on the host line, returns `issuer frozen`. Clawback returns `clawback issuer`. A missing trust line is not treated as frozen. XAH listings skip this check. LCK stays face value.

Host gen-0 Remit (KVT #13): at most 3 Amounts. More than 3 returns `too many Remit amounts` (fails closed). Each IOU entry is refused while that IOU has LCK above zero. Auction payouts are hook emits and are not affected.

Host Payment: if SendMax is present as XAH drops, the lock check uses the larger of Amount and SendMax. If SendMax is present and is not drops, the hook returns `SendMax not drops`. Extra XAH float remains the rule. LCK does not count emit fees or Remit reserves.

## FEE / TREASURY stamp

Create copies the current host `FEE` and `TREASURY` onto the auction. Finalise uses that stamp so a later admin FEE change cannot retax an open listing.

## Host local state

- `TAC` (4 BE uint32): total auctions created; +1 after ACTIVE ok. Missing -> 0; wrap -> fail-closed.

## Happy paths

- Subscribed seller Remits one URIToken + `DUR` (+ opts) -> auction open, `ST=1`, TAC bumped
- IOU Create auto-TrustSets host line with NoRipple when needed

## Important NOPE

- `Remit must be to host` / `URITokenIDs must contain exactly one token`
- `URIToken not found` / burnable / seller remits disabled / DepositAuth
- Subscription expired or at CAP
- `DUR` out of range; `BN` <= `SP` when both set
- TrustSet / state write failures
- Host gen-0 outflow: `Insufficient spendable float`
- Host gen-0 Remit with more than 3 Amounts: `too many Remit amounts`
- `SendMax not drops`
- `transfer rate set` / `issuer frozen` / `clawback issuer`

## Integration tests

`IT_CREATE.js`: run from this directory (or via root `IT_ALL.js`).
