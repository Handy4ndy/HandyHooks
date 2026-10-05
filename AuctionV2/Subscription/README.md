# Auction House: Subscription (`AuctionSub`)

Part of **Handy Hooks ~ Auction House** (public V2).

Seller subscription gate (XAH only). Gates Create: sellers need a live `SUBEXP` window and `ACTIVE < CAP`.

## HookOn / install

- **HookOn:** Payment + Invoke
- **Install param:** `ADMIN` (20). Sole account that may change settings. Must not be the host. Must not be the baked default `raMjZ7ayJ3txQY75vQWr8RTzErAcUD3gee`. That account returns `baked ADMIN refused`.
- **Wasm:** 9149 bytes
- **HookHash:** `6F6FD261881A285C9B038B46025809BBC7217E8EE904B914C2ED89EACBDCA68A`

Fail-closed until `SUBPRICE`, `SUBPERIOD`, `SUBSPLIT`, `AUCCAP`, and `TREASURY` are all set.

## Admin Invokes (one param each)

| Param | Size | Meaning |
|-------|------|---------|
| `SUBPRICE` | 8 | uint64 BE drops, > 0 |
| `SUBPERIOD` | 4 | uint32 BE seconds, > 0 (bounded ~60s .. 366d) |
| `SUBSPLIT` | 2 | uint16 BE percent 0..100 (treasury share of each SUB) |
| `AUCCAP` | 2 | uint16 BE max active auctions per seller, ≥ 1 |
| `TREASURY` | 20 | account receiving the split (≠ host) |
| `FEE` | 2 | uint16 BE bps 0..5000 (Finalise seller fee; Create snapshots per auction) |
| `GRANT` | 20 | seller: extend one SUBPERIOD (same as paid) |
| `REVOKE` | 20 | seller: clear window (`NOPE` if `ACTIVE > 0`) |

## Seller Payment

Payment to host with otxn param `SUB`: exact `SUBPRICE` XAH. Extends expiry from max(now, prior expiry); snapshots `AUCCAP` into seller `CAP`; emits `SUBSPLIT%` to TREASURY (remainder stays on host). Treasury emit fail → whole SUB rolls back.

Payment without `SUB` → accept (passthrough for Bids / others). A payment that carries both `SUB` and `AID` is rejected (`NOPE("SUB and AID both set")`).

## Seller foreign namespace

Namespace = seller AccountID (20) zero-padded to 32:

| Key | Size | Meaning |
|-----|------|---------|
| `SUBEXP` | 8 | uint64 BE ledger-time expiry |
| `ACTIVE` | 2 | uint16 BE open auction count (Create ±1; Sub preserves) |
| `CAP` | 2 | uint16 BE AUCCAP snapshot at SUB/GRANT |

## Happy paths

- Admin sets SUBPRICE / SUBPERIOD / SUBSPLIT / AUCCAP / TREASURY / FEE → `DONE("... updated")`
- Seller pays exact SUBPRICE with `SUB` → callback `DONE("Auction subscription successful")`
- `GRANT` / `REVOKE` by ADMIN on a seller account

## Important NOPE / DONE

- `ADMIN install param required` / `ADMIN must not be the host` / `baked ADMIN refused`
- `Unknown admin parameter` (unknown admin Invoke param, not the CLR clear)
- `SUBPRICE must be > 0` / `SUBPRICE above maximum`
- `SUBPERIOD out of range` / `SUBSPLIT must be 0..100` / `AUCCAP out of range`
- `TREASURY must be 20 bytes` / treasury ≠ host
- `SUB and AID both set`
- REVOKEs with open auctions → NOPE
- Callback success: `Auction subscription successful`

## Integration tests

`IT_SUB.js`: run from this directory (or via root `IT_ALL.js`).
