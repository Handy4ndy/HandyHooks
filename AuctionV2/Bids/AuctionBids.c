/**
 * Auction House V2 — AuctionBids.c
 *
 * Bid / buy-now via Payment to host with otxn param AID (32-byte auction ns).
 * One fat file; hookapi.h only. All logic in hook()/cbak().
 *
 * Install: param-free for bids. Shares the Payment+Remit+Invoke HookOn
 * with the set (Sub passthrough when no SUB; Bids passthrough when no AID).
 * KVT #6: an Invoke that carries CLR is an ADMIN clear of stuck PEN/SPEN.
 * Other Invokes stay passthrough. HookOn is not widened.
 *
 * ---- Judgment calls (Finalise-friendly defaults) ----
 * 1) First bid: amount >= SP (missing SP → 0; still require amount > 0).
 * 2) Later bids: if MB set → amount >= HIGH+MB; else amount > HIGH.
 * 3) Buy-now: if BN set and amount >= BN → accept payment, refund any prior
 *    high bidder (full) including self seat on self-BN, Remit URIToken from
 *    host to buy-now winner, set ST=2 (lot settled / no further bids). Seller
 *    payment claimed via Finalise later — do NOT pay seller here. Fail-closed
 *    if refund or URI Remit emit fails. Also set BNW=1 and clear URI custody
 *    key (lot left host).
 * 4) HIGH stores actual paid principal (XAH drops BE, or IOU XFL bit-pattern),
 *    same encoding as Create SP/MB/BN. WIN = 20-byte bidder. BCNT = uint32 BE.
 * 5) Outbid refund: XAH = Payment (with WDT if stored); IOU = Remit Amounts.
 *    LCK does NOT subtract prior in the bid otxn — only on refund cbak success.
 *    emit() failure → rollback. No fee-net. No soft-close. No auto-RETRY.
 * 6) Already-high rebid rejected unless this Payment also hits buy-now
 *    (self-BN refunds prior seat to self via same refund/cbak path as outbid).
 * 7) Seller (SLR) cannot bid. Host cannot bid.
 * 8) Bidder AccountRoot must NOT have asfDisallowIncomingRemit /
 *    LSF_DISALLOW_INCOMING_REMIT so Finalise / buy-now Remits can reach them.
 * 9) Bidder must NOT have DepositAuth at bid entry (entry gate). Do NOT block
 *    outbid if prior later sets DepositAuth — emit refund; on cbak fail strand
 *    RFD/RFDA under AID (claim via Finalise).
 * 10) If bidder has lsfRequireDestTag, bid Payment MUST carry DestinationTag;
 *     store as WDT (4 BE) with WIN. Missing tag → NOPE. Refund uses stored WDT.
 * 11) Payment without AID / not to host / Invoke / Remit / outgoing /
 *     emitted re-entry → passthrough.
 * 12) ST must be 1 (open). Missing auction / ST!=1 / expired → reject.
 * 13) Currency must match Create CUR+ISS (omit = XAH). Partial pays rejected.
 *
 * Create AID keys read: ST, EXP, SP, MB, BN, CUR, ISS, SLR, URI, WDT, RFD, PEN
 * Bids AID keys written: HIGH, WIN, BCNT, WDT; on buy-now set SPEN (URI
 * [+REFUND]); ST=2/BNW/URI clear/TBN only on URI cbak success (C02). On
 * refund-only emit PEN; on cbak emit-fail write the strand first, then
 * RFD+RFDA(+RFDT) and clear PEN only if that write returns the full
 * length. A short write leaves PEN and sets LCKU+SSF (no RFD).
 * ok+under → LCKU+SSF (no RFD); URI cbak fail → SSF
 *
 * Host local state (state/state_set — share HookNamespace with Create/Finalise):
 *   TBD (4 BE uint32) total accepted normal bids (not buy-now)
 *   TBN (4 BE uint32) total buy-now settles
 *   LCK (8 BE drops) locked XAH principal; +pay on accept; -prior only on
 *     refund cbak success (or Finalise claim cbak success)
 *   IOU LCK: key = sha512Half(CUR||ISS) (32), value = XFL 8-byte bit pattern
 *   Refund cbak map: key=emit hash (32), value=AID(32)|amt(8)|prior(20)|wdt(4)|flags(1)
 *     flags bit0=has_wdt, bit1=is_claim (Finalise sets), bit2=is_iou
 *   Missing → 0. uint32 wrap / LCK under/overflow → fail-closed.
 *
 * Judgment (emit→AID link): store pending map keyed by emit hash (not etxn_nonce
 * alone). cbak reads otxn_id as emit hash, loads map, then LCK-/strand.
 * Reject a new bid while PEN/SPEN is in flight. A stranded refund does not
 * block later bids; the owed bidder claims it on Finalise.
 * PW4-H01 A2: refund cbak ok+LCK-under → clear PEN/SPEN, set LCKU+SSF, no RFD
 * (forensic only). Bids NOPE while LCKU. Emit-fail writes the strand before
 * clearing PEN. A failed strand write leaves PEN set.
 */
#define HAS_CALLBACK
#include "hookapi.h"

#define NOPE(msg) rollback(SBUF(msg), __LINE__)

#ifndef ttREMIT
#define ttREMIT 95
#endif

#ifndef sfURITokenIDs
#define sfURITokenIDs ((19U << 16) + 99U)
#endif
#ifndef sfAmounts
#define sfAmounts ((15U << 16) + 92U)
#endif

#ifndef LT_URI_TOKEN
#define LT_URI_TOKEN 0x0055U
#endif

/* AccountRoot: asfDisallowIncomingRemit ("lsfDisableRemit" equivalent) */
#ifndef LSF_DISALLOW_INCOMING_REMIT
#define LSF_DISALLOW_INCOMING_REMIT 0x80000000U
#endif
#ifndef LSF_DEPOSIT_AUTH
#define LSF_DEPOSIT_AUTH 0x01000000U
#endif
#ifndef LSF_REQUIRE_DEST_TAG
/* AccountRoot lsfRequireDestTag (same numeric bit as tfPartialPayment on tx) */
#define LSF_REQUIRE_DEST_TAG 0x00020000U
#endif

/* tfPartialPayment */
#ifndef TF_PARTIAL_PAYMENT
#define TF_PARTIAL_PAYMENT 0x00020000U
#endif

/* Refund / settle cbak map flags */
#define RMAP_HAS_WDT  0x01U
#define RMAP_IS_CLAIM 0x02U
#define RMAP_IS_IOU   0x04U
#define RMAP_IS_URI   0x08U
#define RMAP_IS_STRAND 0x10U
#define RMAP_LEN      65U
/* Per-bidder stranded refund under the AID namespace.
 * key = 0x52 || account || zeros. value = amt(8) tag(4) flags(1) cur(20) iss(20). */
#define STRAND_MARK   0x52U
#define STRAND_LEN    53U
#define STRAND_F_WDT  0x01U
#define STRAND_F_IOU  0x02U

/* AID SPEN bits (buy-now multi-leg pending) */
#define SPEN_BIT_REFUND 0x01U
#define SPEN_BIT_URI    0x02U

/* KVT #6. Bids has no published ADMIN parameter. The clear path still
 * refuses either published definition default (Sub, then Finalise) if
 * that account is installed as ADMIN. */
static const uint8_t BAKED_ADMIN_SUB[20] = {
    0x3AU, 0xC4U, 0x79U, 0xABU, 0x56U, 0x17U, 0x0DU, 0x79U, 0x18U, 0x76U,
    0x60U, 0x22U, 0xEFU, 0x3CU, 0x02U, 0x3BU, 0x61U, 0xAAU, 0xD4U, 0xCAU
};
static const uint8_t BAKED_ADMIN_FIN[20] = {
    0x54U, 0x25U, 0xF4U, 0x15U, 0x5CU, 0x34U, 0x0CU, 0x94U, 0x4DU, 0xE0U,
    0xF6U, 0x64U, 0x01U, 0xB8U, 0xCDU, 0xA8U, 0x8AU, 0x71U, 0x14U, 0x09U
};


int64_t cbak(uint32_t what)
{
    _g(1, 1);

    uint8_t hook_acc[20];
    hook_account(SBUF(hook_acc));

    uint8_t txid[32];
    if (otxn_id(SBUF(txid), 0) != 32)
        DONE("cbak no id");

    uint8_t mapv[65];
    if (state(SBUF(mapv), SBUF(txid)) != 65)
        DONE("cbak unmap");

    uint8_t aid[32];
    uint8_t amt[8];
    uint8_t prior[20];
    uint8_t wdt[4];
    {
        int i;
        for (i = 0; GUARD(32), i < 32; ++i)
            aid[i] = mapv[i];
        for (i = 0; GUARD(8), i < 8; ++i)
            amt[i] = mapv[32 + i];
        for (i = 0; GUARD(20), i < 20; ++i)
            prior[i] = mapv[40 + i];
        for (i = 0; GUARD(4), i < 4; ++i)
            wdt[i] = mapv[60 + i];
    }
    uint8_t flags = mapv[64];
    int has_wdt = (flags & RMAP_HAS_WDT) ? 1 : 0;
    int is_claim = (flags & RMAP_IS_CLAIM) ? 1 : 0;
    int is_iou = (flags & RMAP_IS_IOU) ? 1 : 0;
    int is_uri = (flags & RMAP_IS_URI) ? 1 : 0;

    int ok = 0;
    if (what == 0)
    {
        ok = 1;
        uint8_t trb[1];
        if (meta_slot(1) >= 0 && slot_subfield(1, sfTransactionResult, 2) >= 0
            && slot(SBUF(trb), 2) == 1 && trb[0] != 0)
            ok = 0;
    }

    /* -------- URI Remit leg (C02) -------- */
    if (is_uri)
    {
        /* Clear in-flight PEN-style slot if present (idempotent) */
        state_foreign_set(0, 0, "PEN", 3, aid, 32, hook_acc, 20);
        /* Drop URI bit from SPEN */
        {
            uint8_t sp = 0;
            if (state_foreign(&sp, 1, "SPEN", 4, aid, 32, hook_acc, 20) == 1)
            {
                sp = (uint8_t)(sp & (uint8_t)~SPEN_BIT_URI);
                if (sp == 0)
                    state_foreign_set(0, 0, "SPEN", 4, aid, 32, hook_acc, 20);
                else
                    state_foreign_set(&sp, 1, "SPEN", 4, aid, 32, hook_acc, 20);
            }
        }
        if (ok)
        {
            uint8_t settled = 2;
            state_foreign_set(&settled, 1, "ST", 2, aid, 32, hook_acc, 20);
            {
                uint8_t one = 1;
                state_foreign_set(&one, 1, "BNW", 3, aid, 32, hook_acc, 20);
            }
            state_foreign_set(0, 0, "URI", 3, aid, 32, hook_acc, 20);
            state_foreign_set(0, 0, "SSF", 3, aid, 32, hook_acc, 20);
            /* TBN += 1 deferred to URI success (C02) */
            {
                uint32_t tbn = 0;
                uint8_t tb[4];
                if (state(tb, 4, "TBN", 3) == 4)
                    tbn = (uint32_t)UINT32_FROM_BUF(tb);
                if (tbn != 0xFFFFFFFFU)
                {
                    tbn = tbn + 1U;
                    UINT32_TO_BUF(tb, tbn);
                    state_set(tb, 4, "TBN", 3);
                }
            }
            /* M03: delete map after durable commits */
            state_set(0, 0, SBUF(txid));
            DONE("URI cbak ok");
        }
        /* URI fail: strand SSF + BNW without ST=2 (buy-now intent, PW-H03) */
        {
            uint8_t one = 1;
            state_foreign_set(&one, 1, "SSF", 3, aid, 32, hook_acc, 20);
            state_foreign_set(&one, 1, "BNW", 3, aid, 32, hook_acc, 20);
        }
        state_set(0, 0, SBUF(txid));
        DONE("URI cbak fail");
    }

    /* -------- Refund / claim leg (PW4-H01 A2; L5 superseded) -------- */
    /* Clear PEN/SPEN after successful LCK-. ok+under → forensic LCKU+SSF,
     * no RFD/RFDA (money already applied). Emit-fail still strands RFD. */

    if (ok)
    {
        /* LCK -= amt (XAH drops or IOU XFL bits) */
        if (is_iou)
        {
            uint8_t currency[20];
            uint8_t issuer[20];
            if (state_foreign(SBUF(currency), "CUR", 3, aid, 32, hook_acc, 20)
                != 20
                || state_foreign(SBUF(issuer), "ISS", 3, aid, 32, hook_acc, 20)
                   != 20)
            {
                /* PW4-H01 A2: money applied + LCK under → forensic only, no RFD */
                state_foreign_set(0, 0, "PEN", 3, aid, 32, hook_acc, 20);
                {
                    uint8_t sp = 0;
                    if (state_foreign(&sp, 1, "SPEN", 4, aid, 32, hook_acc, 20) == 1)
                    {
                        sp = (uint8_t)(sp & (uint8_t)~SPEN_BIT_REFUND);
                        if (sp == 0)
                            state_foreign_set(0, 0, "SPEN", 4, aid, 32, hook_acc, 20);
                        else
                            state_foreign_set(&sp, 1, "SPEN", 4, aid, 32, hook_acc, 20);
                    }
                }
                {
                    uint8_t one = 1;
                    state_foreign_set(&one, 1, "LCKU", 4, aid, 32, hook_acc, 20);
                    state_foreign_set(&one, 1, "SSF", 3, aid, 32, hook_acc, 20);
                }
                state_set(0, 0, SBUF(txid));
                DONE("cbak IOU meta miss");
            }
            uint8_t iou_lck_key[32];
            {
                uint8_t pre[40];
                int i;
                for (i = 0; GUARD(20), i < 20; ++i)
                {
                    pre[i] = currency[i];
                    pre[20 + i] = issuer[i];
                }
                if (util_sha512h(SBUF(iou_lck_key), SBUF(pre)) != 32)
                {
                    /* PW4-H01 A2: money applied + LCK under → forensic only, no RFD */
                    state_foreign_set(0, 0, "PEN", 3, aid, 32, hook_acc, 20);
                    {
                        uint8_t sp = 0;
                        if (state_foreign(&sp, 1, "SPEN", 4, aid, 32, hook_acc, 20) == 1)
                        {
                            sp = (uint8_t)(sp & (uint8_t)~SPEN_BIT_REFUND);
                            if (sp == 0)
                                state_foreign_set(0, 0, "SPEN", 4, aid, 32, hook_acc, 20);
                            else
                                state_foreign_set(&sp, 1, "SPEN", 4, aid, 32, hook_acc, 20);
                        }
                    }
                    {
                        uint8_t one = 1;
                        state_foreign_set(&one, 1, "LCKU", 4, aid, 32, hook_acc, 20);
                        state_foreign_set(&one, 1, "SSF", 3, aid, 32, hook_acc, 20);
                    }
                    state_set(0, 0, SBUF(txid));
                    DONE("cbak IOU key fail");
                }
            }
            {
                int64_t lck_xfl = 0;
                uint8_t lb[8];
                if (state(lb, 8, iou_lck_key, 32) == 8)
                    lck_xfl = (int64_t)UINT64_FROM_BUF(lb);
                int64_t prior_xfl = (int64_t)UINT64_FROM_BUF(amt);
                int64_t neu = float_sum(lck_xfl, float_negate(prior_xfl));
                /* KVT #12: a negative XFL result (LCK under) is a positive
                 * int64, so also test float_sign. Never store a negative LCK. */
                if (neu < 0 || float_sign(neu) != 0)
                {
                    /* PW4-H01 A2: money applied + LCK under → forensic only, no RFD */
                    state_foreign_set(0, 0, "PEN", 3, aid, 32, hook_acc, 20);
                    {
                        uint8_t sp = 0;
                        if (state_foreign(&sp, 1, "SPEN", 4, aid, 32, hook_acc, 20) == 1)
                        {
                            sp = (uint8_t)(sp & (uint8_t)~SPEN_BIT_REFUND);
                            if (sp == 0)
                                state_foreign_set(0, 0, "SPEN", 4, aid, 32, hook_acc, 20);
                            else
                                state_foreign_set(&sp, 1, "SPEN", 4, aid, 32, hook_acc, 20);
                        }
                    }
                    {
                        uint8_t one = 1;
                        state_foreign_set(&one, 1, "LCKU", 4, aid, 32, hook_acc, 20);
                        state_foreign_set(&one, 1, "SSF", 3, aid, 32, hook_acc, 20);
                    }
                    state_set(0, 0, SBUF(txid));
                    DONE("cbak LCK sub fail");
                }
                if (float_compare(neu, 0, COMPARE_EQUAL) == 1)
                    state_set(0, 0, iou_lck_key, 32);
                else
                {
                    UINT64_TO_BUF(lb, (uint64_t)neu);
                    state_set(lb, 8, iou_lck_key, 32);
                }
            }
        }
        else
        {
            uint64_t lck = 0ULL;
            uint8_t lb[8];
            if (state(lb, 8, "LCK", 3) == 8)
                lck = UINT64_FROM_BUF(lb);
            uint64_t prior_d = UINT64_FROM_BUF(amt);
            if (lck < prior_d)
            {
                /* PW4-H01 A2: money applied + LCK under → forensic only, no RFD */
                state_foreign_set(0, 0, "PEN", 3, aid, 32, hook_acc, 20);
                {
                    uint8_t sp = 0;
                    if (state_foreign(&sp, 1, "SPEN", 4, aid, 32, hook_acc, 20) == 1)
                    {
                        sp = (uint8_t)(sp & (uint8_t)~SPEN_BIT_REFUND);
                        if (sp == 0)
                            state_foreign_set(0, 0, "SPEN", 4, aid, 32, hook_acc, 20);
                        else
                            state_foreign_set(&sp, 1, "SPEN", 4, aid, 32, hook_acc, 20);
                    }
                }
                {
                    uint8_t one = 1;
                    state_foreign_set(&one, 1, "LCKU", 4, aid, 32, hook_acc, 20);
                    state_foreign_set(&one, 1, "SSF", 3, aid, 32, hook_acc, 20);
                }
                state_set(0, 0, SBUF(txid));
                DONE("cbak LCK under");
            }
            lck -= prior_d;
            if (lck == 0ULL)
                state_set(0, 0, "LCK", 3);
            else
            {
                UINT64_TO_BUF(lb, lck);
                state_set(lb, 8, "LCK", 3);
            }
        }
        /* LCK- ok: now clear PEN / SPEN REFUND bit */
        state_foreign_set(0, 0, "PEN", 3, aid, 32, hook_acc, 20);
        {
            uint8_t sp = 0;
            if (state_foreign(&sp, 1, "SPEN", 4, aid, 32, hook_acc, 20) == 1)
            {
                sp = (uint8_t)(sp & (uint8_t)~SPEN_BIT_REFUND);
                if (sp == 0)
                    state_foreign_set(0, 0, "SPEN", 4, aid, 32, hook_acc, 20);
                else
                    state_foreign_set(&sp, 1, "SPEN", 4, aid, 32, hook_acc, 20);
            }
        }
        if (is_claim)
        {
            state_foreign_set(0, 0, "RFD", 3, aid, 32, hook_acc, 20);
            state_foreign_set(0, 0, "RFDA", 4, aid, 32, hook_acc, 20);
            state_foreign_set(0, 0, "RFDT", 4, aid, 32, hook_acc, 20);
            if (flags & RMAP_IS_STRAND)
            {
                uint8_t skey[32];
                int si;
                skey[0] = STRAND_MARK;
                for (si = 0; GUARD(20), si < 20; ++si)
                    skey[1 + si] = prior[si];
                for (si = 21; GUARD(32), si < 32; ++si)
                    skey[si] = 0;
                state_foreign_set(0, 0, skey, 32, aid, 32, hook_acc, 20);
            }
        }
        state_set(0, 0, SBUF(txid));
        DONE("refund cbak ok");
    }

    /* Emit failure: write the strand before clearing PEN. Clear PEN and
     * the SPEN refund bit only when that write returns the full length.
     * A short write sets LCKU+SSF and leaves PEN. Claim retry has no new
     * strand write, so it still clears the in-flight bits. A recorded
     * strand must not freeze later bids. */
    {
        int clear_inflight = 0;
        if (!is_claim)
        {
        /* Record the owe against the prior bidder. A second failure adds
         * to that bidder's strand instead of replacing someone else's. */
        uint8_t skey[32];
        uint8_t sb[STRAND_LEN];
        int si;
        int merge_ok = 1;
        skey[0] = STRAND_MARK;
        for (si = 0; GUARD(20), si < 20; ++si)
            skey[1 + si] = prior[si];
        for (si = 21; GUARD(32), si < 32; ++si)
            skey[si] = 0;
        for (si = 0; GUARD(53), si < STRAND_LEN; ++si)
            sb[si] = 0;
        {
            uint8_t oldb[STRAND_LEN];
            if (state_foreign(oldb, STRAND_LEN, skey, 32, aid, 32,
                              hook_acc, 20) == STRAND_LEN)
            {
                int old_iou = (oldb[12] & STRAND_F_IOU) ? 1 : 0;
                if (old_iou != (is_iou ? 1 : 0))
                    merge_ok = 0;
                else if (!is_iou)
                {
                    uint64_t a = UINT64_FROM_BUF(oldb);
                    uint64_t b = UINT64_FROM_BUF(amt);
                    uint64_t sum = a + b;
                    if (sum < a)
                        merge_ok = 0;
                    else
                        UINT64_TO_BUF(sb, sum);
                }
                else
                {
                    int64_t sum = float_sum((int64_t)UINT64_FROM_BUF(oldb),
                                            (int64_t)UINT64_FROM_BUF(amt));
                    if (sum < 0)
                        merge_ok = 0;
                    else
                        UINT64_TO_BUF(sb, (uint64_t)sum);
                }
                if (merge_ok)
                {
                    sb[12] = oldb[12];
                    for (si = 0; GUARD(4), si < 4; ++si)
                        sb[8 + si] = oldb[8 + si];
                    if (!(sb[12] & STRAND_F_WDT) && has_wdt)
                    {
                        sb[12] = (uint8_t)(sb[12] | STRAND_F_WDT);
                        for (si = 0; GUARD(4), si < 4; ++si)
                            sb[8 + si] = wdt[si];
                    }
                    for (si = 0; GUARD(40), si < 40; ++si)
                        sb[13 + si] = oldb[13 + si];
                }
            }
            else
            {
                for (si = 0; GUARD(8), si < 8; ++si)
                    sb[si] = amt[si];
                if (has_wdt)
                {
                    sb[12] = (uint8_t)(sb[12] | STRAND_F_WDT);
                    for (si = 0; GUARD(4), si < 4; ++si)
                        sb[8 + si] = wdt[si];
                }
                if (is_iou)
                {
                    uint8_t currency[20];
                    uint8_t issuer[20];
                    sb[12] = (uint8_t)(sb[12] | STRAND_F_IOU);
                    if (state_foreign(SBUF(currency), "CUR", 3, aid, 32,
                                      hook_acc, 20) == 20
                        && state_foreign(SBUF(issuer), "ISS", 3, aid, 32,
                                         hook_acc, 20) == 20)
                    {
                        for (si = 0; GUARD(20), si < 20; ++si)
                        {
                            sb[13 + si] = currency[si];
                            sb[33 + si] = issuer[si];
                        }
                    }
                    else
                        merge_ok = 0;
                }
            }
        }
            if (!merge_ok
                || state_foreign_set(sb, STRAND_LEN, skey, 32, aid, 32,
                                     hook_acc, 20) != STRAND_LEN)
            {
                /* Strand missing: leave PEN and the SPEN refund bit. */
                uint8_t one = 1;
                state_foreign_set(&one, 1, "LCKU", 4, aid, 32, hook_acc, 20);
                state_foreign_set(&one, 1, "SSF", 3, aid, 32, hook_acc, 20);
            }
            else
            {
                /* Latest owe, for readers. The strand key is what claim pays. */
                state_foreign_set(amt, 8, "RFD", 3, aid, 32, hook_acc, 20);
                state_foreign_set(prior, 20, "RFDA", 4, aid, 32, hook_acc, 20);
                if (has_wdt)
                    state_foreign_set(wdt, 4, "RFDT", 4, aid, 32, hook_acc, 20);
                else
                    state_foreign_set(0, 0, "RFDT", 4, aid, 32, hook_acc, 20);
                clear_inflight = 1;
            }
        }
        else
            clear_inflight = 1;
        if (clear_inflight)
        {
            state_foreign_set(0, 0, "PEN", 3, aid, 32, hook_acc, 20);
            {
                uint8_t sp = 0;
                if (state_foreign(&sp, 1, "SPEN", 4, aid, 32, hook_acc, 20) == 1)
                {
                    sp = (uint8_t)(sp & (uint8_t)~SPEN_BIT_REFUND);
                    if (sp == 0)
                        state_foreign_set(0, 0, "SPEN", 4, aid, 32, hook_acc, 20);
                    else
                        state_foreign_set(&sp, 1, "SPEN", 4, aid, 32, hook_acc, 20);
                }
            }
        }
    }
    state_set(0, 0, SBUF(txid));
    DONE("refund cbak fail");
}

int64_t hook(uint32_t reserved)
{
    _g(1, 1);
    (void)reserved;

    int64_t tt = otxn_type();

    uint8_t hook_acc[20];
    hook_account(SBUF(hook_acc));

    uint8_t otxn_acc[20];
    if (otxn_field(SBUF(otxn_acc), sfAccount) != 20)
        NOPE("missing Account");

    /* H01: gen-0 host outflow gated vs LCK; gen>0 emits unrestricted */
    {
        int is_host = 0;
        BUFFER_EQUAL(is_host, hook_acc, otxn_acc, 20);
        if (is_host)
        {
            if (otxn_generation() == 0)
            {
                if (tt == ttREMIT)
                {
                    uint8_t peek[160];
                    if (otxn_field(SBUF(peek), sfURITokenIDs) > 0)
                        NOPE("Host gen-0 Remit must not carry URIToken");
                    /* PW-H01: gate Remit Amounts vs LCK like Payment */
                    {
                        uint8_t ap[1];
                        int64_t apn = otxn_field(SBUF(ap), sfAmounts);
                        int has_am = (apn > 0 || apn == TOO_SMALL) ? 1 : 0;
                        if (has_am)
                        {
                            uint64_t xah_out = 0ULL;
                            int parsed = 0;
                            if (otxn_slot(11) < 0)
                                NOPE("Remit Amounts unreadable");
                            if (slot_subfield(11, sfAmounts, 12) < 0)
                                NOPE("Remit Amounts unreadable");
                            int64_t rc = slot_count(12);
                            if (rc <= 0)
                                NOPE("Remit Amounts unreadable");
                            /* KVT #13 (Andy locked): host gen-0 Remit carries at most
                             * 3 Amounts. Above that fail closed with a named NOPE.
                             * Outer GUARD covers ri < rc with rc <= 3. */
                            if (rc > 3)
                                NOPE("too many Remit amounts");
                            {
                                int64_t ri;
                                for (ri = 0; GUARD(4), ri < rc; ++ri)
                                {
                                    if (slot_subarray(12, (uint32_t)ri, 13) < 0)
                                        NOPE("Remit Amounts unreadable");
                                    if (slot_subfield(13, sfAmount, 14) < 0)
                                        NOPE("Remit Amounts unreadable");
                                    uint8_t ab[48];
                                    int64_t al = slot(SBUF(ab), 14);
                                    if (al == 8)
                                    {
                                        /* Keep STAmount wire bits — match bal UINT64_FROM_BUF */
                                        uint64_t d = UINT64_FROM_BUF(ab);
                                        if (d > 0ULL)
                                        {
                                            xah_out += d;
                                            parsed = 1;
                                        }
                                    }
                                    else if (al == 48)
                                    {
                                        parsed = 1;
                                        /* KVT #13: one copy loop. It runs once per
                                         * IOU entry, so its GUARD covers 3 entries
                                         * x 21 checks (was a fixed 20 per call
                                         * site, which rolled back on a 2nd IOU). */
                                        uint8_t iou_lck_key[32];
                                        uint8_t pre[40];
                                        int i;
                                        for (i = 0; GUARD(63), i < 20; ++i)
                                        {
                                            pre[i] = ab[8 + i];
                                            pre[20 + i] = ab[28 + i];
                                        }
                                        if (util_sha512h(SBUF(iou_lck_key),
                                                         SBUF(pre)) == 32)
                                        {
                                            uint8_t lb[8];
                                            if (state(lb, 8, iou_lck_key, 32)
                                                == 8)
                                            {
                                                int64_t lck_xfl =
                                                    (int64_t)UINT64_FROM_BUF(lb);
                                                if (float_compare(
                                                        lck_xfl, 0,
                                                        COMPARE_GREATER)
                                                    == 1)
                                                    NOPE("Insufficient spendable float");
                                            }
                                        }
                                    }
                                    else
                                        NOPE("Remit Amounts unreadable");
                                }
                            }
                            if (!parsed)
                                NOPE("Remit Amounts unreadable");
                            if (xah_out > 0ULL)
                            {
                                uint64_t lck = 0ULL;
                                uint8_t lb[8];
                                if (state(lb, 8, "LCK", 3) == 8)
                                    lck = UINT64_FROM_BUF(lb);
                                uint8_t akl[34];
                                uint64_t bal_raw = 0ULL;
                                if (util_keylet(SBUF(akl), KEYLET_ACCOUNT,
                                                hook_acc, 20, 0, 0, 0, 0)
                                    == 34
                                    && slot_set(SBUF(akl), 20) >= 0
                                    && slot_subfield(20, sfBalance, 21) >= 0)
                                {
                                    uint8_t bb[8];
                                    if (slot(SBUF(bb), 21) == 8)
                                        bal_raw = UINT64_FROM_BUF(bb);
                                }
                                /* Strip STAmount canonical bit for drops math */
                                uint64_t bal = bal_raw & 0x3FFFFFFFFFFFFFFFULL;
                                uint64_t pay = xah_out & 0x3FFFFFFFFFFFFFFFULL;
                                uint64_t head = 2000000ULL;
                                uint64_t need = lck + head;
                                if (need < lck)
                                    NOPE("Insufficient spendable float");
                                if (bal < need || pay > (bal - need))
                                    NOPE("Insufficient spendable float");
                            }
                        }
                    }
                }
                if (tt == ttPAYMENT)
                {
                    uint8_t amtbuf[48];
                    int64_t alen = otxn_field(SBUF(amtbuf), sfAmount);
                    /* SendMax absent: Amount check unchanged.
                       SendMax XAH drops: gate the larger of Amount and SendMax.
                       SendMax IOU or not drops, or drops beside a non-drops Amount: fail closed. */
                    uint8_t smbuf[48];
                    int64_t slen = otxn_field(SBUF(smbuf), sfSendMax);
                    if (slen == TOO_SMALL || slen >= 0)
                    {
                        if (slen != 8 || alen != 8)
                            NOPE("SendMax not drops");
                    }
                    if (alen == 8)
                    {
                        uint64_t pay = UINT64_FROM_BUF(amtbuf);
                        if (slen == 8)
                        {
                            uint64_t sm = UINT64_FROM_BUF(smbuf);
                            if (sm > pay)
                                pay = sm;
                        }
                        uint64_t lck = 0ULL;
                        uint8_t lb[8];
                        if (state(lb, 8, "LCK", 3) == 8)
                            lck = UINT64_FROM_BUF(lb);
                        uint8_t akl[34];
                        uint64_t bal = 0ULL;
                        if (util_keylet(SBUF(akl), KEYLET_ACCOUNT, hook_acc, 20,
                                        0, 0, 0, 0) == 34
                            && slot_set(SBUF(akl), 20) >= 0
                            && slot_subfield(20, sfBalance, 21) >= 0)
                        {
                            uint8_t bb[8];
                            if (slot(SBUF(bb), 21) == 8)
                                bal = UINT64_FROM_BUF(bb);
                        }
                        uint64_t head = 2000000ULL;
                        uint64_t need = lck + head;
                        if (need < lck)
                            NOPE("Insufficient spendable float");
                        if (bal < need || pay > (bal - need))
                            NOPE("Insufficient spendable float");
                    }
                    else if (alen == 48)
                    {
                        uint8_t currency[20];
                        uint8_t issuer[20];
                        int i;
                        for (i = 0; GUARD(20), i < 20; ++i)
                        {
                            currency[i] = amtbuf[8 + i];
                            issuer[i] = amtbuf[28 + i];
                        }
                        uint8_t iou_lck_key[32];
                        uint8_t pre[40];
                        for (i = 0; GUARD(20), i < 20; ++i)
                        {
                            pre[i] = currency[i];
                            pre[20 + i] = issuer[i];
                        }
                        if (util_sha512h(SBUF(iou_lck_key), SBUF(pre)) == 32)
                        {
                            uint8_t lb[8];
                            if (state(lb, 8, iou_lck_key, 32) == 8)
                            {
                                int64_t lck_xfl = (int64_t)UINT64_FROM_BUF(lb);
                                if (float_compare(lck_xfl, 0, COMPARE_GREATER)
                                    == 1)
                                    NOPE("Insufficient spendable float");
                            }
                        }
                    }
                }
            }
            DONE("Outgoing ok");
        }
    }

    /* Emitted txn re-entry */
    if (otxn_generation() > 0)
        DONE("Emit passthrough");

    /* Invoke: passthrough unless CLR is present (KVT #6 clear).
     * Remit stays passthrough. HookOn already includes Invoke. */
    if (tt == ttINVOKE)
    {
        uint8_t clr_buf[8];
        int64_t clr_len = otxn_param(SBUF(clr_buf), "CLR", 3);
        if (clr_len != DOESNT_EXIST)
        {
            if (clr_len != 1 ||
                (clr_buf[0] != 0x01U && clr_buf[0] != 0x02U &&
                 clr_buf[0] != 0x03U))
                NOPE("CLR bad");

            uint8_t aid[32];
            if (otxn_param(SBUF(aid), "AID", 3) != 32)
                NOPE("AID must be 32 bytes");

            uint8_t admin[20];
            if (hook_param(SBUF(admin), "ADMIN", 5) != 20)
                NOPE("ADMIN install param required");
            {
                int baked = 0;
                BUFFER_EQUAL(baked, admin, BAKED_ADMIN_SUB, 20);
                if (!baked)
                    BUFFER_EQUAL(baked, admin, BAKED_ADMIN_FIN, 20);
                if (baked)
                    NOPE("baked ADMIN refused");
            }
            {
                int bad = 0;
                BUFFER_EQUAL(bad, admin, hook_acc, 20);
                if (bad)
                    NOPE("ADMIN must not be the host");
            }
            {
                int ok = 0;
                BUFFER_EQUAL(ok, otxn_acc, admin, 20);
                if (!ok)
                    NOPE("not admin");
            }

            /* Drop the named marker key. Absent is a clean no-op.
             * No emit, no LCK write, no strand delete.
             * PEN prefix 0x50 0x45 0x4E. SPEN prefix 0x53 0x50 0x45 0x4E.
             * Namespace is the 32-byte AID. */
            if (clr_buf[0] == 0x01U || clr_buf[0] == 0x03U)
            {
                uint8_t pen_chk[32];
                if (state_foreign(pen_chk, 32, "PEN", 3, aid, 32,
                                  hook_acc, 20) == 32)
                {
                    if (state_foreign_set(0, 0, "PEN", 3, aid, 32,
                                          hook_acc, 20) < 0)
                        NOPE("PEN write failed");
                }
            }
            if (clr_buf[0] == 0x02U || clr_buf[0] == 0x03U)
            {
                uint8_t spen_chk = 0;
                if (state_foreign(&spen_chk, 1, "SPEN", 4, aid, 32,
                                  hook_acc, 20) == 1)
                {
                    if (state_foreign_set(0, 0, "SPEN", 4, aid, 32,
                                          hook_acc, 20) < 0)
                        NOPE("SPEN write failed");
                }
            }
            DONE("marker cleared");
        }
        DONE("Passthrough");
    }
    if (tt == ttREMIT)
        DONE("Passthrough");

    if (tt != ttPAYMENT)
        DONE("Passthrough");

    uint8_t dest[20];
    if (otxn_field(SBUF(dest), sfDestination) != 20)
        NOPE("missing Destination");
    {
        int to_host = 0;
        BUFFER_EQUAL(to_host, dest, hook_acc, 20);
        if (!to_host)
            DONE("Payment passthrough");
    }

    /* AID required to enter bid path; absent → passthrough (Sub / donation) */
    uint8_t aid[32];
    {
        int64_t alen = otxn_param(SBUF(aid), "AID", 3);
        if (alen < 0)
            DONE("Payment passthrough");
        if (alen != 32)
            NOPE("AID must be 32 bytes");
    }

    {
        uint8_t sub_peek[8];
        if (otxn_param(SBUF(sub_peek), "SUB", 3) >= 0)
            NOPE("SUB and AID both set");
    }

    /* -------- Load Create auction state (AID foreign ns) -------- */
    uint8_t st = 0;
    if (state_foreign(&st, 1, "ST", 2, aid, 32, hook_acc, 20) != 1)
    {
        uint8_t tsf0 = 0;
        if (state_foreign(&tsf0, 1, "TSF", 3, aid, 32, hook_acc, 20) == 1
            && tsf0 != 0)
            NOPE("create TrustSet failed");
        NOPE("auction not found");
    }
    if (st != 1)
        NOPE("auction not open");

    /* Buy-now already settled (legacy BNW lock; ST=2 also blocks above) */
    {
        uint8_t bnw = 0;
        if (state_foreign(&bnw, 1, "BNW", 3, aid, 32, hook_acc, 20) == 1
            && bnw == 1)
            NOPE("buy-now already won");
    }

    uint8_t expb[8];
    if (state_foreign(expb, 8, "EXP", 3, aid, 32, hook_acc, 20) != 8)
        NOPE("EXP missing");
    uint64_t exp = UINT64_FROM_BUF(expb);
    int64_t now = ledger_last_time();
    if (now < 0)
        NOPE("ledger time unavailable");
    if (!((uint64_t)now < exp))
        NOPE("auction expired");

    uint8_t seller[20];
    if (state_foreign(SBUF(seller), "SLR", 3, aid, 32, hook_acc, 20) != 20)
        NOPE("SLR missing");
    {
        int is_seller = 0;
        BUFFER_EQUAL(is_seller, otxn_acc, seller, 20);
        if (is_seller)
            NOPE("seller cannot bid");
    }

    /* Reject if stranded refund / settle or in-flight refund/URI on this AID */
    {
        uint8_t lcku_chk = 0;
        if (state_foreign(&lcku_chk, 1, "LCKU", 4, aid, 32, hook_acc, 20) == 1
            && lcku_chk != 0)
            NOPE("LCK under forensic");
        /* A stranded outbid refund is the prior bidder's claim. It must
         * not freeze later bids. PEN/SPEN still cover an in-flight emit. */
        uint8_t pen_chk[32];
        if (state_foreign(pen_chk, 32, "PEN", 3, aid, 32, hook_acc, 20) == 32)
            NOPE("refund in flight");
        uint8_t spen_chk = 0;
        if (state_foreign(&spen_chk, 1, "SPEN", 4, aid, 32, hook_acc, 20) == 1
            && spen_chk != 0)
            NOPE("pending in flight");
        uint8_t ssf_chk = 0;
        if (state_foreign(&ssf_chk, 1, "SSF", 3, aid, 32, hook_acc, 20) == 1
            && ssf_chk != 0)
            NOPE("URI deliver stranded");
        uint8_t cpr_chk = 0;
        if (state_foreign(&cpr_chk, 1, "CPR", 3, aid, 32, hook_acc, 20) == 1
            && cpr_chk != 0)
            NOPE("create pending");
        uint8_t tsf_chk = 0;
        if (state_foreign(&tsf_chk, 1, "TSF", 3, aid, 32, hook_acc, 20) == 1
            && tsf_chk != 0)
            NOPE("create TrustSet failed");
    }

    /* Bidder gates: Remit ok, no DepositAuth, require-DT → DestinationTag */
    uint32_t bid_wdt = 0;
    int bid_has_wdt = 0;
    {
        uint8_t akl[34];
        if (util_keylet(SBUF(akl), KEYLET_ACCOUNT, otxn_acc, 20, 0, 0, 0, 0)
            != 34)
            NOPE("bidder AccountRoot keylet failed");
        if (slot_set(SBUF(akl), 1) < 0)
            NOPE("bidder AccountRoot not found");
        uint32_t bflags = 0;
        if (slot_subfield(1, sfFlags, 2) >= 0)
        {
            uint8_t fb[4];
            if (slot(SBUF(fb), 2) == 4)
                bflags = (uint32_t)UINT32_FROM_BUF(fb);
        }
        if (bflags & LSF_DISALLOW_INCOMING_REMIT)
            NOPE("bidder remits disabled");
        if (bflags & LSF_DEPOSIT_AUTH)
            NOPE("bidder DepositAuth");
        if (bflags & LSF_REQUIRE_DEST_TAG)
        {
            uint8_t dtb[4];
            if (otxn_field(SBUF(dtb), sfDestinationTag) != 4)
                NOPE("DestinationTag required");
            bid_wdt = (uint32_t)UINT32_FROM_BUF(dtb);
            bid_has_wdt = 1;
        }
    }

    /* Currency: missing CUR → XAH */
    uint8_t currency[20];
    uint8_t issuer[20];
    int is_iou = 0;
    {
        int i;
        for (i = 0; GUARD(20), i < 20; ++i)
        {
            currency[i] = 0;
            issuer[i] = 0;
        }
    }
    {
        int64_t clen = state_foreign(SBUF(currency), "CUR", 3, aid, 32,
                                     hook_acc, 20);
        if (clen == 20)
        {
            if (state_foreign(SBUF(issuer), "ISS", 3, aid, 32,
                              hook_acc, 20) != 20)
                NOPE("ISS missing for IOU auction");
            is_iou = 1;
        }
    }

    /* Reject partial payments */
    {
        uint8_t flags_buf[4];
        if (otxn_field(SBUF(flags_buf), sfFlags) == 4)
        {
            uint32_t txflags = (uint32_t)UINT32_FROM_BUF(flags_buf);
            if (txflags & TF_PARTIAL_PAYMENT)
                NOPE("partial payments not allowed");
        }
    }

    /* -------- Parse payment amount -------- */
    uint8_t amount_buf[48];
    int64_t amount_len = otxn_field(SBUF(amount_buf), sfAmount);
    uint8_t pay_raw[8]; /* XAH drops BE or IOU XFL bit-pattern */
    int64_t pay_xfl = 0; /* meaningful for IOU; for XAH unused */
    uint64_t pay_drops = 0;

    if (is_iou)
    {
        if (amount_len != 48)
            NOPE("IOU auction requires IOU payment");
        {
            int okc = 0;
            int oki = 0;
            BUFFER_EQUAL(okc, amount_buf + 8, currency, 20);
            BUFFER_EQUAL(oki, amount_buf + 28, issuer, 20);
            if (!okc)
                NOPE("payment currency mismatch");
            if (!oki)
                NOPE("payment issuer mismatch");
        }
        {
            uint8_t hdr[49];
            hdr[0] = 0x61U;
            {
                int i;
                for (i = 0; GUARD(48), i < 48; ++i)
                    hdr[i + 1] = amount_buf[i];
            }
            pay_xfl = float_sto_set((uint32_t)hdr, 49);
            if (pay_xfl < 0)
                NOPE("bad IOU amount");
            if (float_compare(pay_xfl, 0, COMPARE_GREATER) != 1)
                NOPE("bid amount must be positive");
            UINT64_TO_BUF(pay_raw, (uint64_t)pay_xfl);
        }
    }
    else
    {
        if (amount_len != 8)
            NOPE("auction requires XAH payment");
        {
            int64_t d = AMOUNT_TO_DROPS(amount_buf);
            if (d <= 0)
                NOPE("bid amount must be positive");
            pay_drops = (uint64_t)d;
            UINT64_TO_BUF(pay_raw, pay_drops);
        }
    }

    /* Optional Create params */
    uint8_t sp_buf[8];
    uint8_t mb_buf[8];
    uint8_t bn_buf[8];
    int has_sp = 0;
    int has_mb = 0;
    int has_bn = 0;
    if (state_foreign(sp_buf, 8, "SP", 2, aid, 32, hook_acc, 20) == 8)
        has_sp = 1;
    if (state_foreign(mb_buf, 8, "MB", 2, aid, 32, hook_acc, 20) == 8)
        has_mb = 1;
    if (state_foreign(bn_buf, 8, "BN", 2, aid, 32, hook_acc, 20) == 8)
        has_bn = 1;

    /* Current high / winner */
    uint8_t high_buf[8];
    uint8_t win[20];
    int has_high = 0;
    {
        int i;
        for (i = 0; GUARD(20), i < 20; ++i)
            win[i] = 0;
    }
    if (state_foreign(high_buf, 8, "HIGH", 4, aid, 32, hook_acc, 20) == 8
        && state_foreign(SBUF(win), "WIN", 3, aid, 32, hook_acc, 20) == 20)
    {
        int allz = 1;
        {
            int i;
            for (i = 0; GUARD(20), i < 20; ++i)
            {
                if (win[i] != 0)
                {
                    allz = 0;
                    break;
                }
            }
        }
        if (!allz)
            has_high = 1;
    }

    /* Already high bidder? */
    int is_self = 0;
    if (has_high)
    {
        BUFFER_EQUAL(is_self, otxn_acc, win, 20);
    }

    /* Buy-now gate */
    int bin_hit = 0;
    if (has_bn)
    {
        if (is_iou)
        {
            int64_t bn_xfl = (int64_t)UINT64_FROM_BUF(bn_buf);
            if (float_compare(pay_xfl, bn_xfl, COMPARE_GREATER | COMPARE_EQUAL)
                == 1)
                bin_hit = 1;
        }
        else
        {
            if (pay_drops >= UINT64_FROM_BUF(bn_buf))
                bin_hit = 1;
        }
    }

    if (is_self && !bin_hit)
        NOPE("already high bidder");

    /* Min bid gates (skipped when buy-now hits — BN is its own floor) */
    if (!bin_hit)
    {
        if (!has_high)
        {
            /* First bid: >= SP (missing SP → 0; amount already > 0) */
            if (has_sp)
            {
                if (is_iou)
                {
                    int64_t sp_xfl = (int64_t)UINT64_FROM_BUF(sp_buf);
                    if (float_compare(pay_xfl, sp_xfl,
                                      COMPARE_GREATER | COMPARE_EQUAL) != 1)
                        NOPE("below start price");
                }
                else
                {
                    if (pay_drops < UINT64_FROM_BUF(sp_buf))
                        NOPE("below start price");
                }
            }
        }
        else
        {
            /* Later: MB ? >= HIGH+MB : > HIGH */
            if (has_mb)
            {
                if (is_iou)
                {
                    int64_t high_xfl = (int64_t)UINT64_FROM_BUF(high_buf);
                    int64_t mb_xfl = (int64_t)UINT64_FROM_BUF(mb_buf);
                    int64_t need = float_sum(high_xfl, mb_xfl);
                    if (need < 0)
                        NOPE("min-bid overflow");
                    if (float_compare(pay_xfl, need,
                                      COMPARE_GREATER | COMPARE_EQUAL) != 1)
                        NOPE("below min increment");
                }
                else
                {
                    uint64_t high = UINT64_FROM_BUF(high_buf);
                    uint64_t mb = UINT64_FROM_BUF(mb_buf);
                    uint64_t need = high + mb;
                    if (need < high)
                        NOPE("min-bid overflow");
                    if (pay_drops < need)
                        NOPE("below min increment");
                }
            }
            else
            {
                if (is_iou)
                {
                    int64_t high_xfl = (int64_t)UINT64_FROM_BUF(high_buf);
                    if (float_compare(pay_xfl, high_xfl, COMPARE_GREATER) != 1)
                        NOPE("bid not above high");
                }
                else
                {
                    if (!(pay_drops > UINT64_FROM_BUF(high_buf)))
                        NOPE("bid not above high");
                }
            }
        }
    }

    /* Outbid refund? Self buy-now also refunds prior seat to self (C03). */
    int refund_prior = 0;
    if (has_high && (!is_self || bin_hit))
        refund_prior = 1;

    /* Buy-now: load URI + verify host custody before emits */
    uint8_t uri[32];
    if (bin_hit)
    {
        if (state_foreign(SBUF(uri), "URI", 3, aid, 32, hook_acc, 20) != 32)
            NOPE("URI missing");
        {
            uint8_t owner[20];
            uint8_t kl[34];
            int found = 0;
            if (util_keylet(SBUF(kl), KEYLET_UNCHECKED, SBUF(uri), 0, 0, 0, 0)
                == 34
                && slot_set(SBUF(kl), 3) >= 0)
                found = 1;
            if (!found)
            {
                kl[0] = (uint8_t)((LT_URI_TOKEN >> 8) & 0xFFU);
                kl[1] = (uint8_t)(LT_URI_TOKEN & 0xFFU);
                {
                    int i;
                    for (i = 0; GUARD(32), i < 32; ++i)
                        kl[i + 2] = uri[i];
                }
                if (slot_set(SBUF(kl), 3) >= 0)
                    found = 1;
            }
            if (!found)
                NOPE("URIToken not found on ledger");
            if (slot_subfield(3, sfOwner, 4) < 0)
                NOPE("URIToken Owner field missing");
            if (slot(SBUF(owner), 4) != 20)
                NOPE("URIToken Owner read failed");
            {
                int ok = 0;
                BUFFER_EQUAL(ok, owner, hook_acc, 20);
                if (!ok)
                    NOPE("URIToken not in host custody");
            }
        }
    }

    /* Capture prior WDT before we overwrite WIN/WDT (for refund DT) */
    uint8_t prior_wdt[4];
    int has_prior_wdt = 0;
    {
        int i;
        for (i = 0; GUARD(4), i < 4; ++i)
            prior_wdt[i] = 0;
    }
    if (refund_prior)
    {
        if (state_foreign(prior_wdt, 4, "WDT", 3, aid, 32, hook_acc, 20) == 4)
            has_prior_wdt = 1;
    }

    /* Prepare emits BEFORE state writes (fail-closed).
     * IOU: stock PREPARE_PAYMENT_SIMPLE_TRUSTLINE_SIZE is too small for
     * EmitDetails on current Xahau (EMISSION_FAILURE) — use 512-byte
     * manual prepare (same pattern as V1 AuctionHook / min_iou_echo). */
    uint8_t refund_txn[512];
    uint32_t refund_len = 0;
    uint8_t remit_txn[384];
    uint32_t remit_len = 0;

    {
        int emit_n = 0;
        if (refund_prior)
            emit_n++;
        if (bin_hit)
            emit_n++;
        if (emit_n > 0)
        {
            if (etxn_reserve(emit_n) != emit_n)
                NOPE("emit reserve failed");
        }
    }

    if (refund_prior)
    {
        uint32_t rfd_tag = 0;
        if (has_prior_wdt)
            rfd_tag = (uint32_t)UINT32_FROM_BUF(prior_wdt);

        if (is_iou)
        {
            /* IOU prior refund = Remit Amounts (not Payment) */
            int64_t high_xfl = (int64_t)UINT64_FROM_BUF(high_buf);
            if (float_compare(high_xfl, 0, COMPARE_GREATER) != 1)
                NOPE("prior HIGH invalid");
            uint8_t sto[49];
            if (float_sto((uint32_t)sto, 49,
                          (uint32_t)currency, 20,
                          (uint32_t)issuer, 20,
                          high_xfl, sfAmount) != 49)
                NOPE("refund IOU encode failed");
            {
                volatile uint64_t* zq = (volatile uint64_t*)refund_txn;
                int z;
                for (z = 0; GUARD(64), z < 64; ++z)
                    zq[z] = 0ULL;
            }
            refund_txn[0] = 0x12U;
            refund_txn[1] = 0x00U;
            refund_txn[2] = 0x5FU;
            refund_txn[3] = 0x22U;
            refund_txn[4] = 0x80U;
            refund_txn[5] = 0x00U;
            refund_txn[6] = 0x00U;
            refund_txn[7] = 0x00U;
            refund_txn[8] = 0x24U;
            refund_txn[9] = 0x00U;
            refund_txn[10] = 0x00U;
            refund_txn[11] = 0x00U;
            refund_txn[12] = 0x00U;
            refund_txn[13] = 0x2EU;
            refund_txn[14] = (uint8_t)((rfd_tag >> 24) & 0xFFU);
            refund_txn[15] = (uint8_t)((rfd_tag >> 16) & 0xFFU);
            refund_txn[16] = (uint8_t)((rfd_tag >> 8) & 0xFFU);
            refund_txn[17] = (uint8_t)(rfd_tag & 0xFFU);
            refund_txn[18] = 0x20U;
            refund_txn[19] = 0x1AU;
            refund_txn[24] = 0x20U;
            refund_txn[25] = 0x1BU;
            refund_txn[30] = 0x68U;
            refund_txn[31] = 0x40U;
            refund_txn[39] = 0x73U;
            refund_txn[40] = 0x21U;
            refund_txn[74] = 0x81U;
            refund_txn[75] = 0x14U;
            refund_txn[96] = 0x83U;
            refund_txn[97] = 0x14U;
            {
                uint32_t fls = (uint32_t)ledger_seq() + 1U;
                refund_txn[20] = (uint8_t)((fls >> 24) & 0xFFU);
                refund_txn[21] = (uint8_t)((fls >> 16) & 0xFFU);
                refund_txn[22] = (uint8_t)((fls >> 8) & 0xFFU);
                refund_txn[23] = (uint8_t)(fls & 0xFFU);
                uint32_t lls = fls + 4U;
                refund_txn[26] = (uint8_t)((lls >> 24) & 0xFFU);
                refund_txn[27] = (uint8_t)((lls >> 16) & 0xFFU);
                refund_txn[28] = (uint8_t)((lls >> 8) & 0xFFU);
                refund_txn[29] = (uint8_t)(lls & 0xFFU);
            }
            {
                int i;
                for (i = 0; GUARD(20), i < 20; ++i)
                {
                    refund_txn[76 + i] = hook_acc[i];
                    refund_txn[98 + i] = win[i];
                }
            }
            {
                int64_t edlen = etxn_details(refund_txn + 118, 160U);
                if (edlen < 105)
                    NOPE("refund Remit details failed");
                uint8_t* p = refund_txn + 118 + (uint32_t)edlen;
                *p++ = 0xF0U;
                *p++ = 0x5CU;
                *p++ = 0xE0U;
                *p++ = 0x5BU;
                {
                    int i;
                    for (i = 0; GUARD(49), i < 49; ++i)
                        p[i] = sto[i];
                    p += 49;
                }
                *p++ = 0xE1U;
                *p++ = 0xF1U;
                refund_len = (uint32_t)(p - refund_txn);
            }
            {
                int64_t fee = etxn_fee_base(refund_txn, refund_len);
                if (fee < 0)
                    NOPE("refund Remit fee quote failed");
                {
                    uint8_t* b = refund_txn + 31;
                    *b++ = (uint8_t)(0b01000000 + ((fee >> 56) & 0b00111111));
                    *b++ = (uint8_t)((fee >> 48) & 0xFFU);
                    *b++ = (uint8_t)((fee >> 40) & 0xFFU);
                    *b++ = (uint8_t)((fee >> 32) & 0xFFU);
                    *b++ = (uint8_t)((fee >> 24) & 0xFFU);
                    *b++ = (uint8_t)((fee >> 16) & 0xFFU);
                    *b++ = (uint8_t)((fee >> 8) & 0xFFU);
                    *b++ = (uint8_t)((fee >> 0) & 0xFFU);
                }
            }
            if (refund_len == 0 || refund_len > 512U)
                NOPE("refund Remit build failed");
        }
        else
        {
            uint64_t high = UINT64_FROM_BUF(high_buf);
            if (high == 0ULL)
                NOPE("prior HIGH invalid");
            {
                uint32_t sz = 0;
                PREPARE_PAYMENT_SIMPLE(refund_txn, high, win, rfd_tag, 0, sz);
                refund_len = sz;
            }
            if (refund_len == 0 || refund_len > 512U)
                NOPE("refund XAH build failed");
        }
    }

    /* Buy-now: prepare Remit of URIToken host → winner (otxn_acc) */
    if (bin_hit)
    {
        {
            volatile uint64_t* zq = (volatile uint64_t*)remit_txn;
            int z;
            for (z = 0; GUARD(48), z < 48; ++z)
                zq[z] = 0ULL;
        }
        remit_txn[0] = 0x12U;
        remit_txn[1] = 0x00U;
        remit_txn[2] = 0x5FU; /* ttREMIT */
        remit_txn[3] = 0x22U;
        remit_txn[4] = 0x80U;
        remit_txn[5] = 0x00U;
        remit_txn[6] = 0x00U;
        remit_txn[7] = 0x00U; /* tfCANONICAL */
        remit_txn[8] = 0x24U;
        remit_txn[9] = 0x00U;
        remit_txn[10] = 0x00U;
        remit_txn[11] = 0x00U;
        remit_txn[12] = 0x00U; /* Sequence 0 */
        remit_txn[13] = 0x2EU;
        /* H04: DestinationTag from bidder WDT when require-DT */
        {
            uint32_t uri_tag = 0;
            if (bid_has_wdt)
                uri_tag = bid_wdt;
            remit_txn[14] = (uint8_t)((uri_tag >> 24) & 0xFFU);
            remit_txn[15] = (uint8_t)((uri_tag >> 16) & 0xFFU);
            remit_txn[16] = (uint8_t)((uri_tag >> 8) & 0xFFU);
            remit_txn[17] = (uint8_t)(uri_tag & 0xFFU);
        }
        remit_txn[18] = 0x20U;
        remit_txn[19] = 0x1AU;
        remit_txn[24] = 0x20U;
        remit_txn[25] = 0x1BU;
        remit_txn[30] = 0x68U;
        remit_txn[31] = 0x40U; /* Fee placeholder */
        remit_txn[39] = 0x73U;
        remit_txn[40] = 0x21U; /* SigningPubKey null */
        remit_txn[74] = 0x81U;
        remit_txn[75] = 0x14U; /* Account */
        remit_txn[96] = 0x83U;
        remit_txn[97] = 0x14U; /* Destination */
        {
            uint32_t fls = (uint32_t)ledger_seq() + 1U;
            remit_txn[20] = (uint8_t)((fls >> 24) & 0xFFU);
            remit_txn[21] = (uint8_t)((fls >> 16) & 0xFFU);
            remit_txn[22] = (uint8_t)((fls >> 8) & 0xFFU);
            remit_txn[23] = (uint8_t)(fls & 0xFFU);
            uint32_t lls = fls + 4U;
            remit_txn[26] = (uint8_t)((lls >> 24) & 0xFFU);
            remit_txn[27] = (uint8_t)((lls >> 16) & 0xFFU);
            remit_txn[28] = (uint8_t)((lls >> 8) & 0xFFU);
            remit_txn[29] = (uint8_t)(lls & 0xFFU);
        }
        {
            int i;
            for (i = 0; GUARD(20), i < 20; ++i)
            {
                remit_txn[76 + i] = hook_acc[i];
                remit_txn[98 + i] = otxn_acc[i];
            }
        }
        {
            int64_t edlen = etxn_details(remit_txn + 118, 160U);
            if (edlen < 105)
                NOPE("Remit emit details failed");
            uint8_t* p = remit_txn + 118 + (uint32_t)edlen;
            /* sfURITokenIDs VL: type=19 field=99 → 00 13 63, then len 0x20 + id */
            *p++ = 0x00U;
            *p++ = 0x13U;
            *p++ = 0x63U;
            *p++ = 0x20U;
            {
                int i;
                for (i = 0; GUARD(32), i < 32; ++i)
                    p[i] = uri[i];
            }
            p += 32;
            remit_len = (uint32_t)(p - remit_txn);
        }
        {
            int64_t fee = etxn_fee_base(remit_txn, remit_len);
            if (fee < 0)
                NOPE("Remit fee quote failed");
            {
                uint8_t* b = remit_txn + 31;
                *b++ = (uint8_t)(0b01000000 + ((fee >> 56) & 0b00111111));
                *b++ = (uint8_t)((fee >> 48) & 0xFFU);
                *b++ = (uint8_t)((fee >> 40) & 0xFFU);
                *b++ = (uint8_t)((fee >> 32) & 0xFFU);
                *b++ = (uint8_t)((fee >> 24) & 0xFFU);
                *b++ = (uint8_t)((fee >> 16) & 0xFFU);
                *b++ = (uint8_t)((fee >> 8) & 0xFFU);
                *b++ = (uint8_t)((fee >> 0) & 0xFFU);
            }
        }
        if (remit_len == 0 || remit_len > 384U)
            NOPE("Remit build failed");
    }

    /* -------- State writes -------- */
    if (state_foreign_set(pay_raw, 8, "HIGH", 4, aid, 32, hook_acc, 20) != 8)
        NOPE("HIGH write failed");
    if (state_foreign_set(otxn_acc, 20, "WIN", 3, aid, 32, hook_acc, 20) != 20)
        NOPE("WIN write failed");

    {
        uint32_t bcnt = 0;
        uint8_t bcb[4];
        if (state_foreign(bcb, 4, "BCNT", 4, aid, 32, hook_acc, 20) == 4)
            bcnt = (uint32_t)UINT32_FROM_BUF(bcb);
        bcnt = bcnt + 1U;
        UINT32_TO_BUF(bcb, bcnt);
        if (state_foreign_set(bcb, 4, "BCNT", 4, aid, 32, hook_acc, 20) != 4)
            NOPE("BCNT write failed");
    }

    /* Host local LCK: +new pay only. Prior HIGH unlocked on refund cbak
     * success (or stranded RFD claim). high_buf still holds prior. */
    if (is_iou)
    {
        /* IOU LCK: sha512Half(CUR||ISS) */
        uint8_t iou_lck_key[32];
        {
            uint8_t pre[40];
            int i;
            for (i = 0; GUARD(20), i < 20; ++i)
            {
                pre[i] = currency[i];
                pre[20 + i] = issuer[i];
            }
            if (util_sha512h(SBUF(iou_lck_key), SBUF(pre)) != 32)
                NOPE("IOU LCK key hash failed");
        }
        {
            int64_t lck_xfl = 0;
            uint8_t lb[8];
            if (state(lb, 8, iou_lck_key, 32) == 8)
                lck_xfl = (int64_t)UINT64_FROM_BUF(lb);
            {
                /* KVT #12 (Andy locked): fail closed on any bad IOU float.
                 * float_sum error codes are negative int64. A valid negative
                 * XFL is still a positive int64 (XFL sign bit clear), so
                 * neu < 0 alone cannot see it. Check the stored LCK and the
                 * sum with float_sign as well. pay_xfl is already > 0. */
                if (float_sign(lck_xfl) != 0)
                    NOPE("IOU amount invalid");
                int64_t neu = float_sum(lck_xfl, pay_xfl);
                if (neu < 0 || float_sign(neu) != 0)
                    NOPE("IOU amount invalid");
                UINT64_TO_BUF(lb, (uint64_t)neu);
                if (state_set(lb, 8, iou_lck_key, 32) != 8)
                    NOPE("LCK write failed");
            }
        }
    }
    else
    {
        uint64_t lck = 0ULL;
        uint8_t lb[8];
        if (state(lb, 8, "LCK", 3) == 8)
            lck = UINT64_FROM_BUF(lb);
        {
            uint64_t neu = lck + pay_drops;
            if (neu < lck)
                NOPE("LCK overflow");
            UINT64_TO_BUF(lb, neu);
            if (state_set(lb, 8, "LCK", 3) != 8)
                NOPE("LCK write failed");
        }
    }

    /* WDT: store new bidder DT if require-DT; else clear prior WDT key */
    if (bid_has_wdt)
    {
        uint8_t wb[4];
        UINT32_TO_BUF(wb, bid_wdt);
        if (state_foreign_set(wb, 4, "WDT", 3, aid, 32, hook_acc, 20) != 4)
            NOPE("WDT write failed");
    }
    else
    {
        if (state_foreign_set(0, 0, "WDT", 3, aid, 32, hook_acc, 20) < 0)
            NOPE("WDT clear failed");
    }

    if (!bin_hit)
    {
        /* TBD += 1 (accepted normal bid). TBN deferred to URI cbak (C02). */
        uint32_t tbd = 0;
        uint8_t tb[4];
        if (state(tb, 4, "TBD", 3) == 4)
            tbd = (uint32_t)UINT32_FROM_BUF(tb);
        if (tbd == 0xFFFFFFFFU)
            NOPE("TBD overflow");
        tbd = tbd + 1U;
        UINT32_TO_BUF(tb, tbd);
        if (state_set(tb, 4, "TBD", 3) != 4)
            NOPE("TBD write failed");
    }

    /* C02: do NOT write ST=2 / BNW / clear URI / TBN here — URI cbak does.
     * Set SPEN expected bits so competing bids / settle are blocked. */
    if (bin_hit)
    {
        uint8_t sp = SPEN_BIT_URI;
        if (refund_prior)
            sp = (uint8_t)(sp | SPEN_BIT_REFUND);
        if (state_foreign_set(&sp, 1, "SPEN", 4, aid, 32, hook_acc, 20) != 1)
            NOPE("SPEN write failed");
    }

    if (refund_prior)
    {
        uint8_t emh[32];
        if (emit(SBUF(emh), refund_txn, refund_len) != 32)
            NOPE("refund emit failed");
        /* Link emit→AID for cbak: map[emithash]=AID|amt|prior|wdt|flags */
        {
            uint8_t mapv[65];
            int i;
            for (i = 0; GUARD(32), i < 32; ++i)
                mapv[i] = aid[i];
            for (i = 0; GUARD(8), i < 8; ++i)
                mapv[32 + i] = high_buf[i];
            for (i = 0; GUARD(20), i < 20; ++i)
                mapv[40 + i] = win[i];
            for (i = 0; GUARD(4), i < 4; ++i)
                mapv[60 + i] = prior_wdt[i];
            mapv[64] = 0;
            if (has_prior_wdt)
                mapv[64] |= RMAP_HAS_WDT;
            if (is_iou)
                mapv[64] |= RMAP_IS_IOU;
            if (state_set(mapv, 65, SBUF(emh)) != 65)
                NOPE("refund map write failed");
            /* Buy-now multi-leg uses SPEN; single-leg refund still uses PEN */
            if (!bin_hit)
            {
                if (state_foreign_set(emh, 32, "PEN", 3, aid, 32,
                                      hook_acc, 20) != 32)
                    NOPE("PEN write failed");
            }
        }
    }

    if (bin_hit)
    {
        uint8_t emh[32];
        if (emit(SBUF(emh), remit_txn, remit_len) != 32)
            NOPE("URI Remit emit failed");
        /* Map URI Remit for cbak (RMAP_IS_URI). ST/BNW/URI/TBN on success. */
        {
            uint8_t mapv[65];
            int i;
            for (i = 0; GUARD(32), i < 32; ++i)
                mapv[i] = aid[i];
            for (i = 0; GUARD(8), i < 8; ++i)
                mapv[32 + i] = 0;
            for (i = 0; GUARD(20), i < 20; ++i)
                mapv[40 + i] = otxn_acc[i];
            for (i = 0; GUARD(4), i < 4; ++i)
                mapv[60 + i] = 0;
            mapv[64] = RMAP_IS_URI;
            if (bid_has_wdt)
            {
                UINT32_TO_BUF(mapv + 60, bid_wdt);
                mapv[64] |= RMAP_HAS_WDT;
            }
            if (state_set(mapv, 65, SBUF(emh)) != 65)
                NOPE("URI map write failed");
        }
        if (refund_prior)
            DONE("Buy-now accepted with prior refund");
        DONE("Buy-now accepted");
    }

    if (refund_prior)
        DONE("Bid accepted with prior refund");
    DONE("Bid accepted");
}
