# Auction House V2 - testnet path (one host)

**Handy Hooks ~ Auction House** public V2. NetworkID **21338** (`xahau-test.net`).

Generated: 2026-10-07T13:18:41.317Z (UTC). Run 7 October 2026 (UK time) on the current release pins.

## Host

| Field | Value |
|-------|-------|
| Host account | `rMw9AFYX94f4ZU8UhAhtphrVcwPXZ11BTe` |
| HookNamespace | `3E4C2AE9ADCD80EC50B2CF4BBF107CFD4F300316B382A04196D95A69878FB3CE` |
| ADMIN (override) | `rQny1eqQy2NZzi2ztWq3YLfahbQhmTttXU` |
| Treasury | `rsd3cbMDJAAsGpCFGEA6fXeLEiCn2Zpj1m` |
| Seller | `rsxhvRuQT2ZQtLpT3T1njpGw4wL4C2yKvT` |

## Wasm pins (sha512Half)

| Hook | Bytes | HookHash |
|------|------:|----------|
| Subscription | 9149 | `6F6FD261881A285C9B038B46025809BBC7217E8EE904B914C2ED89EACBDCA68A` |
| Create | 12274 | `D1D4BFCA240733EF3E00AB04E74481697D2C67C552A2994911732CC822869342` |
| Bids | 22984 | `F73BCBF899147F512DB4768546DE75AED84235F3E4BA9D50C04BB460AAFF6832` |
| Finalise | 31464 | `D9881023127345F3DAFAC09744F6BE6952500EAF230FF0DC1A8FC28D0E66CC06` |

## Key transactions

| Step | Hash |
|------|------|
| SetHook (all four) | `D4426843E0FEBD236AA377BF564200370C6D269F0299C28579C147BEF4B5EF7B` |
| Seller SUB | `D6E906B2E0882787543E08B36E39E44056857E9C1DDE7C282A4632B9EC635201` |
| SUB+AID reject | `24502C9649B393D4A30ABAE730B1B4FC373DAAEB675498A6D81B246D444977F5` |
| Buy-now Create | `C79D54B5E668A7373679797710EB19DAA59B4CA1C8B5D4550570BD7DFB85FD94` |
| Buy-now Payment | `1193D3D6EC7C56393AB0830654C15653FA33AFC3E3857994866D95B5B6559C80` |
| Buy-now Finalise | `B243DC0E18A2AEED63C9AD03A916E05E31951552BD5C90174AD7E53AF3D2C7FA` |
| Strand Create | `E24CF66A87D23B7472C38ED930CDBF47FDEFEB5CDBBA23B524F2F68072F3EA88` |
| Strand bid A / B / C | `86A180E192D9CCE3D749473802ABF7F64A0D7EFE072010404084DE56E13E5F07` / `D26CDC42AB95647E068A7ECCC795E9EA10C3641AF9A1FDA2E94270BE6443AF98` / `DF1D978C24F6E3D3890EA15DB5182C3E57307D54E21D966A6A002913191ED35E` |
| Seller cancel | `8FE0E722DA90BBB0A30E27DC0815DC935668B67694D203430B349F95475192C0` |

## Results

- **PASS:** 30
- **FAIL:** 0

Full machine-readable log: `IT_COMBINED.json`.

## What this proves

1. One fresh host installs Sub+Create+Bids+Finalise with **ADMIN override** and shared namespace.
2. Seller subscribes, Creates a URIToken auction, buy-now settles, Finalise pays seller+treasury, AID clears.
3. Payment with both `SUB` and `AID` is rejected.
4. After a forced stranded outbid refund (DepositAuth), a **later max bid is still accepted** (not frozen).
5. Seller cancel (`CNCL`) returns the lot when open with no bids.

Re-run: `node IT_COMBINED.js` from the Auction House V2 folder (xahau.js).
