/**
 * Auction House V2 — AuctionFinalise.c
 *
 * Finalise auction via Invoke with otxn param AID (32-byte auction ns).
 * One fat file; hookapi.h only. All logic in hook()/cbak().
 *
 * Install: ADMIN (20) — same install-param key as AuctionSub.
 *   PW5-M01: ADMIN required only when LCKU is set (migration: installs
 *   without ADMIN OK until LCKU hits). Reject ADMIN == hook account
 *   (Sub-style). NOPE("ADMIN install param required") /
 *   NOPE("ADMIN must not be the host").
 * HookOn: Invoke only. Shares HookNamespace with Sub/Create/Bids for
 * FEE, TREASURY, LCK, TAC (TAC lifetime — never decremented here).
 *
 * Callers:
 *   Buy-now (ST=2 + BNW): seller or ADMIN only
 *   Buy-now URI-fail retry (ST=1 + SSF + BNW): seller, WIN, or ADMIN (KVT #15)
 *   Timed after EXP: seller, WIN (winner), or ADMIN
 *
 * Paths:
 *   1) Buy-now claim: URI already gone; pay seller(+treasury) from HIGH;
 *      LCK-; ACTIVE-1; clear all AID keys
 *   2) Timed with bids: Remit URI→WIN; pay seller(+treasury); LCK-;
 *      ACTIVE-1; clear AID
 *   3) Timed no bids: Remit URI→seller; ACTIVE-1; clear AID; no LCK change
 *   4) Seller cancel (AID+CNCL=0x01): ST=1, no bids, rem>=DUR/2 —
 *      same URI→seller SMAP as path 3; DONE Cancel pending; no LCK change
 *
 * Emit order (fail-closed): URI Remit (if needed) → treasury fee →
 * seller remainder → then state (LCK-, ACTIVE-1, clear AID).
 *
 * FEE: basis points of HIGH, hard max 5000 (Sub admin state key FEE).
 * Missing FEE or TREASURY → 100% seller. FEE=0 → no treasury emit.
 * XAH payouts = Payment; IOU payouts (seller + treasury) = Remit always.
 *
 * Judgment: missing AID → passthrough (Sub admin Invoke coexistence);
 * wrong-size AID → reject. CNCL present + AID missing/wrong →
 * NOPE(CNCL needs AID). Non-Invoke / outgoing / emit re-entry →
 * passthrough.
 *
 * KVT #6: CLR present (1 byte, 0x01 PEN / 0x02 SPEN / 0x03 both) is an
 * installed-ADMIN clear of that marker. No emit, no LCK write, no strand
 * delete. CLR absent leaves CNCL and settle unchanged. HookOn stays Invoke.
 *
 * Stranded refund claim:
 *   A failed outbid refund is stored per bidder (strand key) and mirrored
 *   in RFD/RFDA. It does not block bids, settle, or cancel. The owed
 *   account Invokes AID to claim. Legacy RFD still pays that RFDA.
 *   In-flight PEN/SPEN still reject. No auto-RETRY.
 *
 * PW4-H01 A2 / M01 S1 (Andy locked):
 *   ok-under refund is forensic (LCKU+SSF), not claimable RFD (L5 superseded).
 *   PEN+RFD both set → NOPE(refund state corrupt) belt.
 *   Seller or ADMIN Invoke with AID clears LCKU; clears SSF unless BNW set; no emit.
 *   PW5-H01 Option 2: keep SSF when BNW present so ST=1+SSF+BNW buynow classify survives.
 *   Settle AID/ACTIVE commit blocked while SSF set (M01 S1).
 *
 * PW5-M02 (Andy locked):
 *   Settle money LCK-under → set SSF+LCKU (reuse H01 Option 2 ack).
 *   Keep lock A (TPAY/SPAY on under). Keep S1. Emit-prep clears SSF only
 *   (never LCKU). Ack→retry→under→ack OK until ops repairs LCK.
 */
#define HAS_CALLBACK
#include "hookapi.h"

/* KVT #10. Published Finalise definition default ADMIN
 * (HookHash DEB6ABA1…51AE4AD4 parameter ADMIN).
 * r3CANwccnAMqEyYeBW3q7Gk9sMAfuYZe45
 * hook_param returns these bytes when the installer does not override.
 * An override to any other account still passes. */
static const uint8_t BAKED_ADMIN[20] = {
    0x54U, 0x25U, 0xF4U, 0x15U, 0x5CU, 0x34U, 0x0CU, 0x94U, 0x4DU, 0xE0U,
    0xF6U, 0x64U, 0x01U, 0xB8U, 0xCDU, 0xA8U, 0x8AU, 0x71U, 0x14U, 0x09U
};

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

/* Refund cbak map flags — must match AuctionBids.c */
#define RMAP_HAS_WDT  0x01U
#define RMAP_IS_CLAIM 0x02U
#define RMAP_IS_IOU   0x04U
#define RMAP_IS_STRAND 0x10U
/* Must match AuctionBids.c strand record. */
#define STRAND_MARK   0x52U
#define STRAND_LEN    53U
#define STRAND_F_WDT  0x01U
#define STRAND_F_IOU  0x02U

/* Settlement SMAP (67B) + SPEN/SEXP bits (C01) */
#define SMAP_KIND_URI   2U
#define SMAP_KIND_TREAS 3U
#define SMAP_KIND_SELL  4U
#define SMAP_LEN        67U
#define SMAP_IS_IOU     0x04U
#define SPEN_BIT_URI    0x02U
#define SPEN_BIT_TREAS  0x04U
#define SPEN_BIT_SELL   0x08U

#define KEY_ACTIVE       "ACTIVE"
#define KEY_ACTIVE_LEN   6
#define FEE_MAX_BPS      5000U

int64_t cbak(uint32_t what)
{
    _g(1, 1);

    uint8_t hook_acc[20];
    hook_account(SBUF(hook_acc));

    uint8_t txid[32];
    if (otxn_id(SBUF(txid), 0) != 32)
        DONE("cbak no id");

    int ok = 0;
    if (what == 0)
    {
        ok = 1;
        uint8_t trb[1];
        if (meta_slot(1) >= 0 && slot_subfield(1, sfTransactionResult, 2) >= 0
            && slot(SBUF(trb), 2) == 1 && trb[0] != 0)
            ok = 0;
    }

    /* PW-C01: TSF reclaim map (32 → AID) */
    {
        uint8_t aid_tsf[32];
        if (state(SBUF(aid_tsf), SBUF(txid)) == 32)
        {
            state_set(0, 0, SBUF(txid));
            uint8_t tsf = 0;
            int is_tsf =
                (state_foreign(&tsf, 1, "TSF", 3, aid_tsf, 32, hook_acc, 20)
                 == 1 && tsf != 0);
            if (!is_tsf)
                DONE("cbak unmap");
            if (!ok)
                DONE("TSF reclaim strand");
            state_foreign_set(0, 0, "URI", 3, aid_tsf, 32, hook_acc, 20);
            state_foreign_set(0, 0, "SLR", 3, aid_tsf, 32, hook_acc, 20);
            state_foreign_set(0, 0, "TSF", 3, aid_tsf, 32, hook_acc, 20);
            DONE("TSF reclaim ok");
        }
    }

    /* SMAP settlement (67) */
    uint8_t smap[67];
    if (state(SBUF(smap), SBUF(txid)) == 67)
    {
        uint8_t aid[32];
        uint8_t kind = smap[32];
        uint8_t flags = smap[33];
        uint8_t amt[8];
        {
            int i;
            for (i = 0; GUARD(32), i < 32; ++i)
                aid[i] = smap[i];
            for (i = 0; GUARD(8), i < 8; ++i)
                amt[i] = smap[34 + i];
        }
        int is_iou = (flags & SMAP_IS_IOU) ? 1 : 0;

        uint8_t bit = 0;
        if (kind == SMAP_KIND_URI)
            bit = SPEN_BIT_URI;
        else if (kind == SMAP_KIND_TREAS)
            bit = SPEN_BIT_TREAS;
        else if (kind == SMAP_KIND_SELL)
            bit = SPEN_BIT_SELL;

        {
            uint8_t sp = 0;
            if (state_foreign(&sp, 1, "SPEN", 4, aid, 32, hook_acc, 20) == 1)
            {
                sp = (uint8_t)(sp & (uint8_t)~bit);
                if (sp == 0)
                    state_foreign_set(0, 0, "SPEN", 4, aid, 32, hook_acc, 20);
                else
                    state_foreign_set(&sp, 1, "SPEN", 4, aid, 32, hook_acc, 20);
            }
        }

        if (!ok)
        {
            uint8_t one = 1;
            state_foreign_set(&one, 1, "SSF", 3, aid, 32, hook_acc, 20);
            state_set(0, 0, SBUF(txid));
            DONE("settle cbak fail");
        }

        if (kind == SMAP_KIND_URI)
        {
            uint8_t one = 1;
            state_foreign_set(&one, 1, "UOK", 3, aid, 32, hook_acc, 20);
            {
                /* PW-H02: timed URI ok → UOK only, leave ST=1, no BNW.
                 * Buy-now repair (BNW already set) → ST=2 keep BNW. */
                uint8_t bnw_chk = 0;
                int already_bnw =
                    (state_foreign(&bnw_chk, 1, "BNW", 3, aid, 32, hook_acc, 20)
                     == 1 && bnw_chk == 1);
                if (already_bnw)
                {
                    uint8_t settled = 2;
                    state_foreign_set(&settled, 1, "ST", 2, aid, 32,
                                      hook_acc, 20);
                }
            }
            state_foreign_set(0, 0, "URI", 3, aid, 32, hook_acc, 20);
            state_foreign_set(0, 0, "SSF", 3, aid, 32, hook_acc, 20);
        }
        else if (kind == SMAP_KIND_TREAS || kind == SMAP_KIND_SELL)
        {
            /* PW3-M03 lock A: under → set SSF, still set TPAY/SPAY */
            {
                int lck_under = 0;
                if (is_iou)
                {
                    uint8_t currency[20];
                    uint8_t issuer[20];
                    if (state_foreign(SBUF(currency), "CUR", 3, aid, 32,
                                      hook_acc, 20) == 20
                        && state_foreign(SBUF(issuer), "ISS", 3, aid, 32,
                                         hook_acc, 20) == 20)
                    {
                        uint8_t iou_lck_key[32];
                        uint8_t pre[40];
                        int i;
                        for (i = 0; GUARD(20), i < 20; ++i)
                        {
                            pre[i] = currency[i];
                            pre[20 + i] = issuer[i];
                        }
                        if (util_sha512h(SBUF(iou_lck_key), SBUF(pre)) == 32)
                        {
                            int64_t lck_xfl = 0;
                            uint8_t lb[8];
                            if (state(lb, 8, iou_lck_key, 32) == 8)
                                lck_xfl = (int64_t)UINT64_FROM_BUF(lb);
                            int64_t ax = (int64_t)UINT64_FROM_BUF(amt);
                            int64_t neu = float_sum(lck_xfl, float_negate(ax));
                            /* KVT #12 class: negative XFL is a positive int64.
                             * Treat it as LCK under, never store it. */
                            if (neu >= 0 && float_sign(neu) == 0)
                            {
                                if (float_compare(neu, 0, COMPARE_EQUAL) == 1)
                                    state_set(0, 0, iou_lck_key, 32);
                                else
                                {
                                    UINT64_TO_BUF(lb, (uint64_t)neu);
                                    state_set(lb, 8, iou_lck_key, 32);
                                }
                            }
                            else
                                lck_under = 1;
                        }
                    }
                }
                else
                {
                    uint64_t lck = 0ULL;
                    uint8_t lb[8];
                    if (state(lb, 8, "LCK", 3) == 8)
                        lck = UINT64_FROM_BUF(lb);
                    uint64_t ad = UINT64_FROM_BUF(amt);
                    if (lck >= ad)
                    {
                        lck -= ad;
                        if (lck == 0ULL)
                            state_set(0, 0, "LCK", 3);
                        else
                        {
                            UINT64_TO_BUF(lb, lck);
                            state_set(lb, 8, "LCK", 3);
                        }
                    }
                    else
                        lck_under = 1;
                }
                {
                    /* PW3-M03 lock A + PW5-M02: under → SSF+LCKU, still TPAY/SPAY */
                    uint8_t one = 1;
                    if (lck_under)
                    {
                        state_foreign_set(&one, 1, "SSF", 3, aid, 32,
                                          hook_acc, 20);
                        state_foreign_set(&one, 1, "LCKU", 4, aid, 32,
                                          hook_acc, 20);
                    }
                    if (kind == SMAP_KIND_TREAS)
                        state_foreign_set(&one, 1, "TPAY", 4, aid, 32,
                                          hook_acc, 20);
                    else
                        state_foreign_set(&one, 1, "SPAY", 4, aid, 32,
                                          hook_acc, 20);
                }
            }
        }

        state_set(0, 0, SBUF(txid));

        /* Commit iff SPEN clear and flags cover SEXP */
        {
            uint8_t sp = 0;
            int busy = (state_foreign(&sp, 1, "SPEN", 4, aid, 32, hook_acc, 20)
                        == 1 && sp != 0);
            if (!busy)
            {
                uint8_t sexp = 0;
                state_foreign(&sexp, 1, "SEXP", 4, aid, 32, hook_acc, 20);
                uint8_t uok = 0, tpay = 0, spay = 0;
                int has_uok = (state_foreign(&uok, 1, "UOK", 3, aid, 32,
                                             hook_acc, 20) == 1 && uok);
                int has_tpay = (state_foreign(&tpay, 1, "TPAY", 4, aid, 32,
                                              hook_acc, 20) == 1 && tpay);
                int has_spay = (state_foreign(&spay, 1, "SPAY", 4, aid, 32,
                                              hook_acc, 20) == 1 && spay);
                int covered = 1;
                if ((sexp & SPEN_BIT_URI) && !has_uok)
                    covered = 0;
                if ((sexp & SPEN_BIT_TREAS) && !has_tpay)
                    covered = 0;
                if ((sexp & SPEN_BIT_SELL) && !has_spay)
                    covered = 0;
                /* empty SEXP + no busy: nothing expected — do not wipe */
                if (sexp == 0)
                    covered = 0;
                /* PW4-M01 S1: never AID/ACTIVE commit while SSF set */
                {
                    uint8_t ssf_c = 0;
                    if (state_foreign(&ssf_c, 1, "SSF", 3, aid, 32,
                                      hook_acc, 20) == 1 && ssf_c != 0)
                        covered = 0;
                }

                if (covered)
                {
                    uint8_t seller[20];
                    if (state_foreign(SBUF(seller), "SLR", 3, aid, 32,
                                      hook_acc, 20) == 20)
                    {
                        uint8_t seller_ns[32];
                        int i;
                        for (i = 0; GUARD(20), i < 20; ++i)
                            seller_ns[i] = seller[i];
                        for (i = 20; GUARD(32), i < 32; ++i)
                            seller_ns[i] = 0;
                        uint8_t actb[2];
                        if (state_foreign(actb, 2, KEY_ACTIVE, KEY_ACTIVE_LEN,
                                          seller_ns, 32, hook_acc, 20) == 2)
                        {
                            uint16_t active =
                                (uint16_t)(((uint16_t)actb[0] << 8) | actb[1]);
                            if (active >= 1U)
                            {
                                active = (uint16_t)(active - 1U);
                                actb[0] = (uint8_t)((active >> 8) & 0xFFU);
                                actb[1] = (uint8_t)(active & 0xFFU);
                                state_foreign_set(actb, 2, KEY_ACTIVE,
                                                  KEY_ACTIVE_LEN, seller_ns, 32,
                                                  hook_acc, 20);
                            }
                        }
                    }
                    state_foreign_set(0, 0, "ST", 2, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "EXP", 3, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "SP", 2, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "MB", 2, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "BN", 2, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "CUR", 3, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "ISS", 3, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "SLR", 3, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "URI", 3, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "HIGH", 4, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "WIN", 3, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "BCNT", 4, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "BNW", 3, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "DUR", 3, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "WDT", 3, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "RFD", 3, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "RFDA", 4, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "RFDT", 4, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "FEE", 3, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "TREASURY", 8, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "PEN", 3, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "SPEN", 4, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "SSF", 3, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "LCKU", 4, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "UOK", 3, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "TPAY", 4, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "SPAY", 4, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "SEXP", 4, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "TSF", 3, aid, 32, hook_acc, 20);
                    DONE("settle commit ok");
                }
            }
        }
        DONE("settle cbak ok");
    }

    /* RMAP claim (65) — M03: clear map after LCK- */
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
    int is_iou = (flags & RMAP_IS_IOU) ? 1 : 0;
    (void)wdt;
    (void)prior;

    state_foreign_set(0, 0, "PEN", 3, aid, 32, hook_acc, 20);

    if (!ok)
    {
        state_set(0, 0, SBUF(txid));
        DONE("claim cbak fail");
    }

    if (is_iou)
    {
        uint8_t currency[20];
        uint8_t issuer[20];
        int got_ci = 0;
        if (state_foreign(SBUF(currency), "CUR", 3, aid, 32, hook_acc, 20)
            == 20
            && state_foreign(SBUF(issuer), "ISS", 3, aid, 32, hook_acc, 20)
               == 20)
            got_ci = 1;
        if (!got_ci && (flags & RMAP_IS_STRAND))
        {
            uint8_t skey[32];
            uint8_t sb[STRAND_LEN];
            int si;
            skey[0] = STRAND_MARK;
            for (si = 0; GUARD(20), si < 20; ++si)
                skey[1 + si] = prior[si];
            for (si = 21; GUARD(32), si < 32; ++si)
                skey[si] = 0;
            if (state_foreign(sb, STRAND_LEN, skey, 32, aid, 32, hook_acc, 20)
                == STRAND_LEN
                && (sb[12] & STRAND_F_IOU))
            {
                for (si = 0; GUARD(20), si < 20; ++si)
                {
                    currency[si] = sb[13 + si];
                    issuer[si] = sb[33 + si];
                }
                got_ci = 1;
            }
        }
        if (!got_ci)
        {
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
            /* KVT #12 class: also catch a valid negative XFL result. */
            if (neu < 0 || float_sign(neu) != 0)
            {
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
    {
        uint8_t rfda_now[20];
        int mirror = 1;
        if (flags & RMAP_IS_STRAND)
        {
            mirror = 0;
            if (state_foreign(SBUF(rfda_now), "RFDA", 4, aid, 32, hook_acc, 20)
                == 20)
                BUFFER_EQUAL(mirror, rfda_now, prior, 20);
        }
        if (mirror)
        {
            state_foreign_set(0, 0, "RFD", 3, aid, 32, hook_acc, 20);
            state_foreign_set(0, 0, "RFDA", 4, aid, 32, hook_acc, 20);
            state_foreign_set(0, 0, "RFDT", 4, aid, 32, hook_acc, 20);
        }
    }
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
    state_set(0, 0, SBUF(txid));
    DONE("claim cbak ok");
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
                    if (alen == 8)
                    {
                        uint64_t pay = UINT64_FROM_BUF(amtbuf);
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

    /* Invoke-only: other types passthrough (defensive) */
    if (tt != ttINVOKE)
        DONE("Passthrough");

    /* KVT #6. Enter only when CLR is present so CNCL and settle stay. */
    {
        uint8_t clr_buf[8];
        int64_t clr_len = otxn_param(SBUF(clr_buf), "CLR", 3);
        if (clr_len != DOESNT_EXIST)
        {
            if (clr_len != 1 ||
                (clr_buf[0] != 0x01U && clr_buf[0] != 0x02U &&
                 clr_buf[0] != 0x03U))
                NOPE("CLR bad");

            uint8_t clr_aid[32];
            if (otxn_param(SBUF(clr_aid), "AID", 3) != 32)
                NOPE("AID must be 32 bytes");

            uint8_t clr_admin[20];
            if (hook_param(SBUF(clr_admin), "ADMIN", 5) != 20)
                NOPE("ADMIN install param required");
            {
                int baked = 0;
                BUFFER_EQUAL(baked, clr_admin, BAKED_ADMIN, 20);
                if (baked)
                    NOPE("baked ADMIN refused");
            }
            {
                int bad = 0;
                BUFFER_EQUAL(bad, clr_admin, hook_acc, 20);
                if (bad)
                    NOPE("ADMIN must not be the host");
            }
            {
                int ok = 0;
                BUFFER_EQUAL(ok, otxn_acc, clr_admin, 20);
                if (!ok)
                    NOPE("not admin");
            }

            /* Same namespace keys as Bids. PEN 0x50 0x45 0x4E.
             * SPEN 0x53 0x50 0x45 0x4E. Namespace is the 32-byte AID.
             * Absent marker is a clean no-op. */
            if (clr_buf[0] == 0x01U || clr_buf[0] == 0x03U)
            {
                uint8_t pen_chk[32];
                if (state_foreign(pen_chk, 32, "PEN", 3, clr_aid, 32,
                                  hook_acc, 20) == 32)
                {
                    if (state_foreign_set(0, 0, "PEN", 3, clr_aid, 32,
                                          hook_acc, 20) < 0)
                        NOPE("PEN write failed");
                }
            }
            if (clr_buf[0] == 0x02U || clr_buf[0] == 0x03U)
            {
                uint8_t spen_chk = 0;
                if (state_foreign(&spen_chk, 1, "SPEN", 4, clr_aid, 32,
                                  hook_acc, 20) == 1)
                {
                    if (state_foreign_set(0, 0, "SPEN", 4, clr_aid, 32,
                                          hook_acc, 20) < 0)
                        NOPE("SPEN write failed");
                }
            }
            DONE("marker cleared");
        }
    }

    /* CNCL peek — seller cancel. Present + AID missing/wrong → NOPE. */
    uint8_t cncl_buf[8];
    int64_t cncl_len = otxn_param(SBUF(cncl_buf), "CNCL", 4);
    int has_cncl = (cncl_len >= 0) ? 1 : 0;

    /* AID: missing → passthrough (Sub admin Invokes); wrong size → reject.
     * CNCL present + AID missing/wrong → NOPE("CNCL needs AID"). */
    uint8_t aid[32];
    {
        int64_t alen = otxn_param(SBUF(aid), "AID", 3);
        if (alen < 0 || alen != 32)
        {
            if (has_cncl)
                NOPE("CNCL needs AID");
            if (alen < 0)
                DONE("Invoke passthrough");
            NOPE("AID must be 32 bytes");
        }
    }

    /* -------- Seller cancel (CNCL) — early exit before TSF/settle -------- */
    if (has_cncl)
    {
        if (cncl_len != 1 || cncl_buf[0] != 0x01U)
            NOPE("CNCL invalid");

        uint8_t st = 0;
        if (state_foreign(&st, 1, "ST", 2, aid, 32, hook_acc, 20) != 1)
            NOPE("auction not found");

        uint8_t seller[20];
        if (state_foreign(SBUF(seller), "SLR", 3, aid, 32, hook_acc, 20) != 20)
            NOPE("SLR missing");

        {
            int is_seller = 0;
            BUFFER_EQUAL(is_seller, otxn_acc, seller, 20);
            if (!is_seller)
                NOPE("cancel seller only");
        }

        /* Fail-closed mid-flight / forensic / buy-now strand */
        {
            uint8_t pen_chk[32];
            if (state_foreign(pen_chk, 32, "PEN", 3, aid, 32, hook_acc, 20)
                == 32)
                NOPE("cancel PEN set");
        }
        {
            uint8_t lcku = 0;
            if (state_foreign(&lcku, 1, "LCKU", 4, aid, 32, hook_acc, 20) == 1
                && lcku != 0)
                NOPE("cancel LCKU set");
        }
        {
            uint8_t ssf = 0;
            if (state_foreign(&ssf, 1, "SSF", 3, aid, 32, hook_acc, 20) == 1
                && ssf != 0)
                NOPE("cancel SSF set");
        }
        {
            uint8_t tsf = 0;
            if (state_foreign(&tsf, 1, "TSF", 3, aid, 32, hook_acc, 20) == 1
                && tsf != 0)
                NOPE("cancel TSF set");
        }
        {
            uint8_t bnw = 0;
            if (state_foreign(&bnw, 1, "BNW", 3, aid, 32, hook_acc, 20) == 1
                && bnw != 0)
                NOPE("cancel BNW set");
        }
        /* Belt: settle emit already in flight */
        {
            uint8_t spen_chk = 0;
            if (state_foreign(&spen_chk, 1, "SPEN", 4, aid, 32, hook_acc, 20)
                == 1 && spen_chk != 0)
                NOPE("cancel pending in flight");
        }

        /* No bids: WIN absent AND HIGH absent; BCNT missing or 0 */
        {
            uint8_t win_chk[20];
            uint8_t high_chk[8];
            if (state_foreign(SBUF(win_chk), "WIN", 3, aid, 32, hook_acc, 20)
                == 20)
                NOPE("cancel has bids");
            if (state_foreign(high_chk, 8, "HIGH", 4, aid, 32, hook_acc, 20)
                == 8)
                NOPE("cancel has bids");
            {
                uint8_t bcb[4];
                if (state_foreign(bcb, 4, "BCNT", 4, aid, 32, hook_acc, 20)
                    == 4)
                {
                    uint32_t bcnt = UINT32_FROM_BUF(bcb);
                    if (bcnt > 0U)
                        NOPE("cancel BCNT belt");
                }
            }
        }

        if (st != 1)
            NOPE("cancel ST");

        uint8_t expb[8];
        if (state_foreign(expb, 8, "EXP", 3, aid, 32, hook_acc, 20) != 8)
            NOPE("EXP missing");
        uint64_t exp = UINT64_FROM_BUF(expb);
        uint8_t durb[8];
        if (state_foreign(durb, 8, "DUR", 3, aid, 32, hook_acc, 20) != 8)
            NOPE("DUR missing");
        uint64_t dur = UINT64_FROM_BUF(durb);
        int64_t now = ledger_last_time();
        if (now < 0)
            NOPE("ledger time unavailable");
        if ((uint64_t)now >= exp)
            NOPE("cancel window closed");
        {
            uint64_t rem = exp - (uint64_t)now;
            uint64_t half = dur >> 1;
            if (rem < half)
                NOPE("cancel window closed");
        }

        /* URI must be present for Remit (fail closed) */
        uint8_t uri[32];
        if (state_foreign(SBUF(uri), "URI", 3, aid, 32, hook_acc, 20) != 32)
            NOPE("URI missing");
        {
            uint8_t owner[20];
            uint8_t kl[34];
            int found = 0;
            if (util_keylet(SBUF(kl), KEYLET_UNCHECKED, SBUF(uri), 0, 0, 0, 0)
                == 34 && slot_set(SBUF(kl), 1) >= 0)
                found = 1;
            if (!found)
            {
                kl[0] = (uint8_t)((LT_URI_TOKEN >> 8) & 0xFFU);
                kl[1] = (uint8_t)(LT_URI_TOKEN & 0xFFU);
                {
                    int i;
                    for (i = 0; GUARD(32), i < 32; ++i)
                        kl[2 + i] = uri[i];
                }
                if (slot_set(SBUF(kl), 1) >= 0)
                    found = 1;
            }
            if (!found)
                NOPE("URIToken not found on ledger");
            if (slot_subfield(1, sfOwner, 2) < 0)
                NOPE("URIToken Owner field missing");
            if (slot(SBUF(owner), 2) != 20)
                NOPE("URIToken Owner read failed");
            {
                int ok = 0;
                BUFFER_EQUAL(ok, owner, hook_acc, 20);
                if (!ok)
                    NOPE("URIToken not in host custody");
            }
        }

        /* Reuse timed no-bids: URI Remit → seller + SMAP URI leg */
        if (etxn_reserve(1) != 1)
            NOPE("emit reserve failed");

        uint8_t uri_txn[384];
        uint32_t uri_len = 0;
        {
            volatile uint64_t* zq = (volatile uint64_t*)uri_txn;
            int z;
            for (z = 0; GUARD(48), z < 48; ++z)
                zq[z] = 0ULL;
        }
        uri_txn[0] = 0x12U;
        uri_txn[1] = 0x00U;
        uri_txn[2] = 0x5FU;
        uri_txn[3] = 0x22U;
        uri_txn[4] = 0x80U;
        uri_txn[5] = 0x00U;
        uri_txn[6] = 0x00U;
        uri_txn[7] = 0x00U;
        uri_txn[8] = 0x24U;
        uri_txn[9] = 0x00U;
        uri_txn[10] = 0x00U;
        uri_txn[11] = 0x00U;
        uri_txn[12] = 0x00U;
        uri_txn[13] = 0x2EU;
        uri_txn[14] = 0x00U;
        uri_txn[15] = 0x00U;
        uri_txn[16] = 0x00U;
        uri_txn[17] = 0x00U;
        uri_txn[18] = 0x20U;
        uri_txn[19] = 0x1AU;
        uri_txn[24] = 0x20U;
        uri_txn[25] = 0x1BU;
        uri_txn[30] = 0x68U;
        uri_txn[31] = 0x40U;
        uri_txn[39] = 0x73U;
        uri_txn[40] = 0x21U;
        uri_txn[74] = 0x81U;
        uri_txn[75] = 0x14U;
        uri_txn[96] = 0x83U;
        uri_txn[97] = 0x14U;
        {
            uint32_t fls = (uint32_t)ledger_seq() + 1U;
            uri_txn[20] = (uint8_t)((fls >> 24) & 0xFFU);
            uri_txn[21] = (uint8_t)((fls >> 16) & 0xFFU);
            uri_txn[22] = (uint8_t)((fls >> 8) & 0xFFU);
            uri_txn[23] = (uint8_t)(fls & 0xFFU);
            uint32_t lls = fls + 4U;
            uri_txn[26] = (uint8_t)((lls >> 24) & 0xFFU);
            uri_txn[27] = (uint8_t)((lls >> 16) & 0xFFU);
            uri_txn[28] = (uint8_t)((lls >> 8) & 0xFFU);
            uri_txn[29] = (uint8_t)(lls & 0xFFU);
        }
        {
            int i;
            for (i = 0; GUARD(20), i < 20; ++i)
            {
                uri_txn[76 + i] = hook_acc[i];
                uri_txn[98 + i] = seller[i];
            }
        }
        {
            int64_t edlen = etxn_details(uri_txn + 118, 160U);
            if (edlen < 105)
                NOPE("URI Remit details failed");
            uint8_t* p = uri_txn + 118 + (uint32_t)edlen;
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
            uri_len = (uint32_t)(p - uri_txn);
        }
        {
            int64_t fee = etxn_fee_base(uri_txn, uri_len);
            if (fee < 0)
                NOPE("URI Remit fee quote failed");
            {
                uint8_t* b = uri_txn + 31;
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
        if (uri_len == 0 || uri_len > 384U)
            NOPE("URI Remit build failed");

        /* SEXP/SPEN URI bit — same as timed no-bids */
        {
            uint8_t emitting = (uint8_t)SPEN_BIT_URI;
            uint8_t sexp = 0;
            if (state_foreign(&sexp, 1, "SEXP", 4, aid, 32, hook_acc, 20) != 1)
                sexp = 0;
            sexp = (uint8_t)(sexp | emitting);
            if (state_foreign_set(&sexp, 1, "SEXP", 4, aid, 32, hook_acc, 20)
                != 1)
                NOPE("SEXP write failed");
            if (state_foreign_set(&emitting, 1, "SPEN", 4, aid, 32,
                                  hook_acc, 20) != 1)
                NOPE("SPEN write failed");
        }

        {
            uint8_t emh[32];
            if (emit(SBUF(emh), uri_txn, uri_len) != 32)
                NOPE("URI Remit emit failed");
            {
                uint8_t sm[67];
                int i;
                for (i = 0; GUARD(67), i < 67; ++i)
                    sm[i] = 0;
                for (i = 0; GUARD(32), i < 32; ++i)
                    sm[i] = aid[i];
                sm[32] = (uint8_t)SMAP_KIND_URI;
                sm[33] = 0;
                if (state_set(sm, 67, SBUF(emh)) != 67)
                    NOPE("URI SMAP write failed");
            }
        }
        DONE("Cancel pending");
    }

    /* -------- PW-C01: TSF recovery reclaim (before ST/EXP gates) -------- */
    {
        uint8_t tsf = 0;
        if (state_foreign(&tsf, 1, "TSF", 3, aid, 32, hook_acc, 20) == 1
            && tsf != 0)
        {
            uint8_t seller[20];
            if (state_foreign(SBUF(seller), "SLR", 3, aid, 32, hook_acc, 20)
                != 20)
                NOPE("SLR missing");
            uint8_t admin[20];
            int has_admin = 0;
            if (hook_param(SBUF(admin), "ADMIN", 5) == 20)
            {
                int baked = 0;
                BUFFER_EQUAL(baked, admin, BAKED_ADMIN, 20);
                if (baked)
                    NOPE("baked ADMIN refused");
                has_admin = 1;
            }
            int is_seller = 0;
            int is_admin = 0;
            BUFFER_EQUAL(is_seller, otxn_acc, seller, 20);
            if (has_admin)
                BUFFER_EQUAL(is_admin, otxn_acc, admin, 20);
            if (!(is_seller || is_admin))
                NOPE("TSF reclaim forbidden");

            uint8_t uri[32];
            if (state_foreign(SBUF(uri), "URI", 3, aid, 32, hook_acc, 20) != 32)
                NOPE("URI missing");
            {
                uint8_t owner[20];
                uint8_t kl[34];
                int found = 0;
                if (util_keylet(SBUF(kl), KEYLET_UNCHECKED, SBUF(uri), 0, 0, 0, 0)
                    == 34 && slot_set(SBUF(kl), 1) >= 0)
                    found = 1;
                if (!found)
                {
                    kl[0] = (uint8_t)((LT_URI_TOKEN >> 8) & 0xFFU);
                    kl[1] = (uint8_t)(LT_URI_TOKEN & 0xFFU);
                    {
                        int i;
                        for (i = 0; GUARD(32), i < 32; ++i)
                            kl[2 + i] = uri[i];
                    }
                    if (slot_set(SBUF(kl), 1) >= 0)
                        found = 1;
                }
                /* PW3-M01: heal when URI absent or already with seller */
                if (!found)
                {
                    state_foreign_set(0, 0, "URI", 3, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "SLR", 3, aid, 32, hook_acc, 20);
                    state_foreign_set(0, 0, "TSF", 3, aid, 32, hook_acc, 20);
                    DONE("TSF reclaim URI absent");
                }
                if (slot_subfield(1, sfOwner, 2) < 0)
                    NOPE("URIToken Owner field missing");
                if (slot(SBUF(owner), 2) != 20)
                    NOPE("URIToken Owner read failed");
                {
                    int host_owns = 0;
                    int slr_owns = 0;
                    BUFFER_EQUAL(host_owns, owner, hook_acc, 20);
                    BUFFER_EQUAL(slr_owns, owner, seller, 20);
                    if (!host_owns)
                    {
                        if (slr_owns)
                        {
                            state_foreign_set(0, 0, "URI", 3, aid, 32,
                                              hook_acc, 20);
                            state_foreign_set(0, 0, "SLR", 3, aid, 32,
                                              hook_acc, 20);
                            state_foreign_set(0, 0, "TSF", 3, aid, 32,
                                              hook_acc, 20);
                            DONE("TSF reclaim already with seller");
                        }
                        NOPE("URIToken not in host custody");
                    }
                }
            }

            if (etxn_reserve(1) != 1)
                NOPE("emit reserve failed");

            uint8_t uri_txn[384];
            uint32_t uri_len = 0;
            {
                volatile uint64_t* zq = (volatile uint64_t*)uri_txn;
                int z;
                for (z = 0; GUARD(48), z < 48; ++z)
                    zq[z] = 0ULL;
            }
            uri_txn[0] = 0x12U;
            uri_txn[1] = 0x00U;
            uri_txn[2] = 0x5FU;
            uri_txn[3] = 0x22U;
            uri_txn[4] = 0x80U;
            uri_txn[5] = 0x00U;
            uri_txn[6] = 0x00U;
            uri_txn[7] = 0x00U;
            uri_txn[8] = 0x24U;
            uri_txn[9] = 0x00U;
            uri_txn[10] = 0x00U;
            uri_txn[11] = 0x00U;
            uri_txn[12] = 0x00U;
            uri_txn[13] = 0x2EU;
            uri_txn[14] = 0x00U;
            uri_txn[15] = 0x00U;
            uri_txn[16] = 0x00U;
            uri_txn[17] = 0x00U;
            uri_txn[18] = 0x20U;
            uri_txn[19] = 0x1AU;
            uri_txn[24] = 0x20U;
            uri_txn[25] = 0x1BU;
            uri_txn[30] = 0x68U;
            uri_txn[31] = 0x40U;
            uri_txn[39] = 0x73U;
            uri_txn[40] = 0x21U;
            uri_txn[74] = 0x81U;
            uri_txn[75] = 0x14U;
            uri_txn[96] = 0x83U;
            uri_txn[97] = 0x14U;
            {
                uint32_t fls = (uint32_t)ledger_seq() + 1U;
                uri_txn[20] = (uint8_t)((fls >> 24) & 0xFFU);
                uri_txn[21] = (uint8_t)((fls >> 16) & 0xFFU);
                uri_txn[22] = (uint8_t)((fls >> 8) & 0xFFU);
                uri_txn[23] = (uint8_t)(fls & 0xFFU);
                uint32_t lls = fls + 4U;
                uri_txn[26] = (uint8_t)((lls >> 24) & 0xFFU);
                uri_txn[27] = (uint8_t)((lls >> 16) & 0xFFU);
                uri_txn[28] = (uint8_t)((lls >> 8) & 0xFFU);
                uri_txn[29] = (uint8_t)(lls & 0xFFU);
            }
            {
                int i;
                for (i = 0; GUARD(20), i < 20; ++i)
                {
                    uri_txn[76 + i] = hook_acc[i];
                    uri_txn[98 + i] = seller[i];
                }
            }
            {
                int64_t edlen = etxn_details(uri_txn + 118, 160U);
                if (edlen < 105)
                    NOPE("URI Remit details failed");
                uint8_t* p = uri_txn + 118 + (uint32_t)edlen;
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
                uri_len = (uint32_t)(p - uri_txn);
            }
            {
                int64_t fee = etxn_fee_base(uri_txn, uri_len);
                if (fee < 0)
                    NOPE("URI Remit fee quote failed");
                {
                    uint8_t* b = uri_txn + 31;
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
            if (uri_len == 0 || uri_len > 384U)
                NOPE("URI Remit build failed");

            uint8_t emh[32];
            if (emit(SBUF(emh), uri_txn, uri_len) != 32)
                NOPE("URI Remit emit failed");
            if (state_set(aid, 32, SBUF(emh)) != 32)
                NOPE("TSF map write failed");
            DONE("TSF reclaim pending");
        }
    }

    /* -------- Stranded refund claim (RFD) / in-flight (PEN) gate -------- */
    {
        uint8_t pen_chk[32];
        int has_pen = (state_foreign(pen_chk, 32, "PEN", 3, aid, 32,
                                    hook_acc, 20) == 32);
        uint8_t rfd_belt[8];
        int has_rfd_belt = (state_foreign(rfd_belt, 8, "RFD", 3, aid, 32,
                                         hook_acc, 20) == 8);
        /* PW4-H01 belt: PEN+RFD corpse must not emit (double-pay risk) */
        if (has_pen && has_rfd_belt)
            NOPE("refund state corrupt");
        if (has_pen)
            NOPE("refund in flight");
        {
            uint8_t spen_chk = 0;
            if (state_foreign(&spen_chk, 1, "SPEN", 4, aid, 32, hook_acc, 20)
                == 1 && spen_chk != 0)
                NOPE("pending in flight");
        }
    }
    {
        uint8_t rfd[8];
        uint8_t rfda[20];
        uint8_t currency[20];
        uint8_t issuer[20];
        uint8_t rfdt[4];
        int has_rfd = 0;
        int from_strand = 0;
        int is_iou = 0;
        int has_rfdt = 0;
        {
            int i;
            for (i = 0; GUARD(20), i < 20; ++i)
            {
                rfda[i] = 0;
                currency[i] = 0;
                issuer[i] = 0;
            }
            for (i = 0; GUARD(8), i < 8; ++i)
                rfd[i] = 0;
            for (i = 0; GUARD(4), i < 4; ++i)
                rfdt[i] = 0;
        }
        {
            uint8_t skey[32];
            uint8_t sb[STRAND_LEN];
            int i;
            skey[0] = STRAND_MARK;
            for (i = 0; GUARD(20), i < 20; ++i)
                skey[1 + i] = otxn_acc[i];
            for (i = 21; GUARD(32), i < 32; ++i)
                skey[i] = 0;
            if (state_foreign(sb, STRAND_LEN, skey, 32, aid, 32, hook_acc, 20)
                == STRAND_LEN)
            {
                from_strand = 1;
                has_rfd = 1;
                for (i = 0; GUARD(8), i < 8; ++i)
                    rfd[i] = sb[i];
                for (i = 0; GUARD(20), i < 20; ++i)
                    rfda[i] = otxn_acc[i];
                if (sb[12] & STRAND_F_WDT)
                {
                    has_rfdt = 1;
                    for (i = 0; GUARD(4), i < 4; ++i)
                        rfdt[i] = sb[8 + i];
                }
                if (sb[12] & STRAND_F_IOU)
                {
                    is_iou = 1;
                    for (i = 0; GUARD(20), i < 20; ++i)
                    {
                        currency[i] = sb[13 + i];
                        issuer[i] = sb[33 + i];
                    }
                }
            }
        }
        if (!from_strand
            && state_foreign(rfd, 8, "RFD", 3, aid, 32, hook_acc, 20) == 8
            && state_foreign(SBUF(rfda), "RFDA", 4, aid, 32, hook_acc, 20)
               == 20)
        {
            int is_rfda = 0;
            BUFFER_EQUAL(is_rfda, otxn_acc, rfda, 20);
            if (is_rfda)
            {
                has_rfd = 1;
                {
                    int64_t clen = state_foreign(SBUF(currency), "CUR", 3,
                                                 aid, 32, hook_acc, 20);
                    if (clen == 20)
                    {
                        if (state_foreign(SBUF(issuer), "ISS", 3, aid, 32,
                                          hook_acc, 20) != 20)
                            NOPE("ISS missing for IOU auction");
                        is_iou = 1;
                    }
                }
                if (state_foreign(rfdt, 4, "RFDT", 4, aid, 32, hook_acc, 20)
                    == 4)
                    has_rfdt = 1;
            }
        }

        if (has_rfd)
        {
            uint32_t rfd_tag = 0;
            if (has_rfdt)
                rfd_tag = (uint32_t)UINT32_FROM_BUF(rfdt);

            if (etxn_reserve(1) != 1)
                NOPE("emit reserve failed");

            uint8_t claim_txn[512];
            uint32_t claim_len = 0;

            if (is_iou)
            {
                int64_t high_xfl = (int64_t)UINT64_FROM_BUF(rfd);
                if (float_compare(high_xfl, 0, COMPARE_GREATER) != 1)
                    NOPE("RFD invalid");
                uint8_t sto[49];
                if (float_sto((uint32_t)sto, 49,
                              (uint32_t)currency, 20,
                              (uint32_t)issuer, 20,
                              high_xfl, sfAmount) != 49)
                    NOPE("claim IOU encode failed");
                {
                    volatile uint64_t* zq = (volatile uint64_t*)claim_txn;
                    int z;
                    for (z = 0; GUARD(64), z < 64; ++z)
                        zq[z] = 0ULL;
                }
                claim_txn[0] = 0x12U;
                claim_txn[1] = 0x00U;
                claim_txn[2] = 0x5FU;
                claim_txn[3] = 0x22U;
                claim_txn[4] = 0x80U;
                claim_txn[5] = 0x00U;
                claim_txn[6] = 0x00U;
                claim_txn[7] = 0x00U;
                claim_txn[8] = 0x24U;
                claim_txn[9] = 0x00U;
                claim_txn[10] = 0x00U;
                claim_txn[11] = 0x00U;
                claim_txn[12] = 0x00U;
                claim_txn[13] = 0x2EU;
                claim_txn[14] = (uint8_t)((rfd_tag >> 24) & 0xFFU);
                claim_txn[15] = (uint8_t)((rfd_tag >> 16) & 0xFFU);
                claim_txn[16] = (uint8_t)((rfd_tag >> 8) & 0xFFU);
                claim_txn[17] = (uint8_t)(rfd_tag & 0xFFU);
                claim_txn[18] = 0x20U;
                claim_txn[19] = 0x1AU;
                claim_txn[24] = 0x20U;
                claim_txn[25] = 0x1BU;
                claim_txn[30] = 0x68U;
                claim_txn[31] = 0x40U;
                claim_txn[39] = 0x73U;
                claim_txn[40] = 0x21U;
                claim_txn[74] = 0x81U;
                claim_txn[75] = 0x14U;
                claim_txn[96] = 0x83U;
                claim_txn[97] = 0x14U;
                {
                    uint32_t fls = (uint32_t)ledger_seq() + 1U;
                    claim_txn[20] = (uint8_t)((fls >> 24) & 0xFFU);
                    claim_txn[21] = (uint8_t)((fls >> 16) & 0xFFU);
                    claim_txn[22] = (uint8_t)((fls >> 8) & 0xFFU);
                    claim_txn[23] = (uint8_t)(fls & 0xFFU);
                    uint32_t lls = fls + 4U;
                    claim_txn[26] = (uint8_t)((lls >> 24) & 0xFFU);
                    claim_txn[27] = (uint8_t)((lls >> 16) & 0xFFU);
                    claim_txn[28] = (uint8_t)((lls >> 8) & 0xFFU);
                    claim_txn[29] = (uint8_t)(lls & 0xFFU);
                }
                {
                    int i;
                    for (i = 0; GUARD(20), i < 20; ++i)
                    {
                        claim_txn[76 + i] = hook_acc[i];
                        claim_txn[98 + i] = rfda[i];
                    }
                }
                {
                    int64_t edlen = etxn_details(claim_txn + 118, 160U);
                    if (edlen < 105)
                        NOPE("claim Remit details failed");
                    uint8_t* p = claim_txn + 118 + (uint32_t)edlen;
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
                    claim_len = (uint32_t)(p - claim_txn);
                }
                {
                    int64_t fee = etxn_fee_base(claim_txn, claim_len);
                    if (fee < 0)
                        NOPE("claim Remit fee quote failed");
                    {
                        uint8_t* b = claim_txn + 31;
                        *b++ = (uint8_t)(0b01000000
                                         + ((fee >> 56) & 0b00111111));
                        *b++ = (uint8_t)((fee >> 48) & 0xFFU);
                        *b++ = (uint8_t)((fee >> 40) & 0xFFU);
                        *b++ = (uint8_t)((fee >> 32) & 0xFFU);
                        *b++ = (uint8_t)((fee >> 24) & 0xFFU);
                        *b++ = (uint8_t)((fee >> 16) & 0xFFU);
                        *b++ = (uint8_t)((fee >> 8) & 0xFFU);
                        *b++ = (uint8_t)((fee >> 0) & 0xFFU);
                    }
                }
                if (claim_len == 0 || claim_len > 512U)
                    NOPE("claim Remit build failed");
            }
            else
            {
                uint64_t drops = UINT64_FROM_BUF(rfd);
                if (drops == 0ULL)
                    NOPE("RFD invalid");
                {
                    uint32_t sz = 0;
                    PREPARE_PAYMENT_SIMPLE(claim_txn, drops, rfda, rfd_tag, 0,
                                           sz);
                    claim_len = sz;
                }
                if (claim_len == 0 || claim_len > 512U)
                    NOPE("claim Payment build failed");
            }

            uint8_t emh[32];
            if (emit(SBUF(emh), claim_txn, claim_len) != 32)
                NOPE("claim emit failed");

            /* Pending map for Finalise cbak (is_claim) */
            {
                uint8_t mapv[65];
                int i;
                for (i = 0; GUARD(32), i < 32; ++i)
                    mapv[i] = aid[i];
                for (i = 0; GUARD(8), i < 8; ++i)
                    mapv[32 + i] = rfd[i];
                for (i = 0; GUARD(20), i < 20; ++i)
                    mapv[40 + i] = rfda[i];
                for (i = 0; GUARD(4), i < 4; ++i)
                    mapv[60 + i] = has_rfdt ? rfdt[i] : 0;
                mapv[64] = RMAP_IS_CLAIM;
                if (has_rfdt)
                    mapv[64] |= RMAP_HAS_WDT;
                if (is_iou)
                    mapv[64] |= RMAP_IS_IOU;
                if (from_strand)
                    mapv[64] |= RMAP_IS_STRAND;
                if (state_set(mapv, 65, SBUF(emh)) != 65)
                    NOPE("claim map write failed");
                if (state_foreign_set(emh, 32, "PEN", 3, aid, 32, hook_acc, 20)
                    != 32)
                    NOPE("PEN write failed");
            }
            DONE("Stranded refund claimed");
        }
    }

    /* -------- Load auction state -------- */
    uint8_t st = 0;
    if (state_foreign(&st, 1, "ST", 2, aid, 32, hook_acc, 20) != 1)
        NOPE("auction not found");

    uint8_t bnw = 0;
    int has_bnw = 0;
    if (state_foreign(&bnw, 1, "BNW", 3, aid, 32, hook_acc, 20) == 1
        && bnw == 1)
        has_bnw = 1;

    uint8_t expb[8];
    if (state_foreign(expb, 8, "EXP", 3, aid, 32, hook_acc, 20) != 8)
        NOPE("EXP missing");
    uint64_t exp = UINT64_FROM_BUF(expb);
    int64_t now = ledger_last_time();
    if (now < 0)
        NOPE("ledger time unavailable");

    uint8_t seller[20];
    if (state_foreign(SBUF(seller), "SLR", 3, aid, 32, hook_acc, 20) != 20)
        NOPE("SLR missing");

    /* ADMIN install param (same key as Sub) */
    uint8_t admin[20];
    int has_admin = 0;
    if (hook_param(SBUF(admin), "ADMIN", 5) == 20)
    {
        int baked = 0;
        BUFFER_EQUAL(baked, admin, BAKED_ADMIN, 20);
        if (baked)
            NOPE("baked ADMIN refused");
        has_admin = 1;
    }

    int is_seller = 0;
    int is_admin = 0;
    BUFFER_EQUAL(is_seller, otxn_acc, seller, 20);
    if (has_admin)
        BUFFER_EQUAL(is_admin, otxn_acc, admin, 20);


    /* PW5-H01 Option 2 + PW5-M01: LCKU forensic — seller/ADMIN Invoke acks
     * (always clear LCKU; clear SSF only when BNW absent). No emit.
     * M01-A (locked): ADMIN install param required only when LCKU set;
     * reject ADMIN == hook (Sub-style). Buy-now Finalise auth unchanged. */
    {
        uint8_t lcku = 0;
        if (state_foreign(&lcku, 1, "LCKU", 4, aid, 32, hook_acc, 20) == 1
            && lcku != 0)
        {
            if (!has_admin)
                NOPE("ADMIN install param required");
            {
                int bad = 0;
                BUFFER_EQUAL(bad, admin, hook_acc, 20);
                if (bad)
                    NOPE("ADMIN must not be the host");
            }
            if (!(is_seller || is_admin))
                NOPE("LCK under forensic");
            state_foreign_set(0, 0, "LCKU", 4, aid, 32, hook_acc, 20);
            if (!has_bnw)
                state_foreign_set(0, 0, "SSF", 3, aid, 32, hook_acc, 20);
            DONE("LCK under acked");
        }
    }

    /* Classify path.
     * PW-H03: timed SSF (no BNW) → path_timed with WIN auth.
     * Buy-now: ST=2+BNW or ST=1+SSF+BNW (Bids URI fail sets BNW w/o ST=2). */
    int path_buynow = 0;
    int path_bn_retry = 0;
    int path_timed = 0;
    {
        uint8_t ssf = 0;
        int has_ssf = (state_foreign(&ssf, 1, "SSF", 3, aid, 32, hook_acc, 20)
                       == 1 && ssf != 0) ? 1 : 0;
        if (st == 2 && has_bnw)
            path_buynow = 1;
        else if (st == 1 && has_ssf && has_bnw)
        {
            /* Buy-now URI-fail retry (Bids URI cbak fail left ST=1+SSF+BNW) */
            path_buynow = 1;
            path_bn_retry = 1;
        }
        else if (st == 1 && has_ssf)
        {
            /* timed settle strand — skip EXP re-check */
            path_timed = 1;
        }
        else if (st == 1)
        {
            uint8_t sexp = 0;
            uint8_t uok = 0;
            int settle_started =
                (state_foreign(&sexp, 1, "SEXP", 4, aid, 32, hook_acc, 20) == 1
                 && sexp != 0)
                || (state_foreign(&uok, 1, "UOK", 3, aid, 32, hook_acc, 20) == 1
                    && uok != 0);
            if (!settle_started && !((uint64_t)now >= exp))
                NOPE("auction not expired");
            path_timed = 1;
        }
        else
            NOPE("auction not finalisable");
    }

    /* Winner (optional — required for timed-with-bids) */
    uint8_t win[20];
    uint8_t high_buf[8];
    int has_bids = 0;
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
            has_bids = 1;
    }

    /* Caller auth */
    if (path_buynow)
    {
        /* KVT #15 (Andy locked): on the buy-now URI-fail retry
         * (ST=1+SSF+BNW) the winner may Invoke too, so the lot is not
         * stranded waiting on the seller. Settled buy-now claim (ST=2+BNW)
         * stays seller or ADMIN only. */
        int is_win = 0;
        if (path_bn_retry && has_bids)
            BUFFER_EQUAL(is_win, otxn_acc, win, 20);
        if (!(is_seller || is_admin || is_win))
            NOPE("buy-now finalise forbidden");
        if (!has_bids)
            NOPE("buy-now HIGH/WIN missing");
    }
    else
    {
        /* timed: seller, WIN, or ADMIN */
        int is_win = 0;
        if (has_bids)
            BUFFER_EQUAL(is_win, otxn_acc, win, 20);
        if (!(is_seller || is_admin || is_win))
            NOPE("timed finalise forbidden");
    }

    /* Currency */
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

    /* FEE / TREASURY. Create stamps both onto the auction. That stamp
     * wins so a later admin change cannot retax an open listing.
     * No stamp (older auctions) still reads the live keys. */
    uint16_t fee_bps = 0;
    int fee_ok = 0;
    int fee_snap = 0;
    {
        uint8_t fb[2];
        if (state_foreign(fb, 2, "FEE", 3, aid, 32, hook_acc, 20) == 2)
            fee_snap = 1;
        else if (state(fb, 2, "FEE", 3) != 2)
        {
            fb[0] = 0;
            fb[1] = 0;
        }
        if (fee_snap || state(fb, 2, "FEE", 3) == 2)
        {
            fee_bps = (uint16_t)(((uint16_t)fb[0] << 8) | fb[1]);
            if (fee_bps > FEE_MAX_BPS)
                NOPE("FEE corrupt");
            fee_ok = 1;
        }
    }
    uint8_t treasury[20];
    int treas_ok = 0;
    if (fee_snap)
    {
        if (state_foreign(SBUF(treasury), "TREASURY", 8, aid, 32,
                          hook_acc, 20) == 20)
        {
            int bad = 0;
            BUFFER_EQUAL(bad, treasury, hook_acc, 20);
            if (!bad)
                treas_ok = 1;
        }
    }
    else if (state(SBUF(treasury), "TREASURY", 8) == 20)
    {
        int bad = 0;
        BUFFER_EQUAL(bad, treasury, hook_acc, 20);
        if (!bad)
            treas_ok = 1;
    }
    /* Missing FEE or TREASURY → 100% seller (no treasury split) */
    int do_fee = 0;
    if (fee_ok && treas_ok && fee_bps > 0U && has_bids)
        do_fee = 1;

    /* URI needed for timed paths; buy-now also if URI key still hosted (C02).
     * PW-H02: UOK set means URI already delivered — do not re-require URI key. */
    uint8_t uri[32];
    int need_uri = 0;
    {
        uint8_t uok_early = 0;
        int already_uok =
            (state_foreign(&uok_early, 1, "UOK", 3, aid, 32, hook_acc, 20) == 1
             && uok_early != 0);
        if (path_timed
            || (path_buynow
                && state_foreign(SBUF(uri), "URI", 3, aid, 32, hook_acc, 20)
                   == 32))
        {
            if (!already_uok)
            {
                if (!path_buynow)
                {
                    if (state_foreign(SBUF(uri), "URI", 3, aid, 32, hook_acc, 20)
                        != 32)
                        NOPE("URI missing");
                }
                need_uri = 1;
                /* verify host custody */
                {
                    uint8_t owner[20];
                    uint8_t kl[34];
                    int found = 0;
                    if (util_keylet(SBUF(kl), KEYLET_UNCHECKED, SBUF(uri), 0, 0,
                                    0, 0) == 34
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
        }
    }

    /* Payout amounts from HIGH (buy-now or timed-with-bids) */
    uint64_t high_drops = 0;
    int64_t high_xfl = 0;
    uint64_t fee_drops = 0;
    uint64_t seller_drops = 0;
    int64_t fee_xfl = 0;
    int64_t seller_xfl = 0;
    if (has_bids)
    {
        if (is_iou)
        {
            high_xfl = (int64_t)UINT64_FROM_BUF(high_buf);
            if (float_compare(high_xfl, 0, COMPARE_GREATER) != 1)
                NOPE("HIGH invalid");
            if (do_fee)
            {
                int64_t rate = float_set(-4, (int64_t)fee_bps);
                if (rate < 0)
                    NOPE("FEE rate float_set failed");
                fee_xfl = float_multiply(high_xfl, rate);
                if (fee_xfl < 0)
                    NOPE("FEE multiply failed");
                if (float_compare(fee_xfl, 0, COMPARE_GREATER) != 1)
                {
                    /* dust fee → all to seller */
                    do_fee = 0;
                    fee_xfl = 0;
                    seller_xfl = high_xfl;
                }
                else
                {
                    seller_xfl = float_sum(high_xfl, float_negate(fee_xfl));
                    if (seller_xfl < 0)
                        NOPE("seller remainder failed");
                }
            }
            else
                seller_xfl = high_xfl;
        }
        else
        {
            high_drops = UINT64_FROM_BUF(high_buf);
            if (high_drops == 0ULL)
                NOPE("HIGH invalid");
            if (do_fee)
            {
                fee_drops = (high_drops * (uint64_t)fee_bps) / 10000ULL;
                if (fee_drops > high_drops)
                    NOPE("FEE overflow");
                if (fee_drops == 0ULL)
                    do_fee = 0;
                seller_drops = high_drops - fee_drops;
            }
            else
                seller_drops = high_drops;
        }
    }

    /* -------- Prepare emits (URI Remit → treasury → seller) -------- */
    uint8_t uri_txn[384];
    uint32_t uri_len = 0;
    uint8_t treas_txn[512];
    uint32_t treas_len = 0;
    uint8_t seller_txn[512];
    uint32_t seller_len = 0;

    /* C01: skip legs already durable-success (retry) */
    uint8_t uok_f = 0, tpay_f = 0, spay_f = 0;
    int has_uok = (state_foreign(&uok_f, 1, "UOK", 3, aid, 32, hook_acc, 20)
                   == 1 && uok_f) ? 1 : 0;
    int has_tpay = (state_foreign(&tpay_f, 1, "TPAY", 4, aid, 32, hook_acc, 20)
                    == 1 && tpay_f) ? 1 : 0;
    int has_spay = (state_foreign(&spay_f, 1, "SPAY", 4, aid, 32, hook_acc, 20)
                    == 1 && spay_f) ? 1 : 0;
    if (has_uok)
        need_uri = 0;

    int emit_uri = need_uri ? 1 : 0;
    int emit_treas = 0;
    int emit_seller = 0;
    if (has_bids && do_fee && !has_tpay)
        emit_treas = 1;
    if (has_bids && !has_spay)
    {
        if (is_iou)
        {
            if (float_compare(seller_xfl, 0, COMPARE_GREATER) == 1)
                emit_seller = 1;
        }
        else if (seller_drops > 0ULL)
            emit_seller = 1;
    }

    {
        int emit_n = emit_uri + emit_treas + emit_seller;
        if (emit_n > 0)
        {
            if (etxn_reserve(emit_n) != emit_n)
                NOPE("emit reserve failed");
        }
    }

    /* URI Remit → WIN (timed with bids) or seller (timed no bids) */
    if (need_uri)
    {
        uint8_t* uri_dest = has_bids ? win : seller;
        {
            volatile uint64_t* zq = (volatile uint64_t*)uri_txn;
            int z;
            for (z = 0; GUARD(48), z < 48; ++z)
                zq[z] = 0ULL;
        }
        uri_txn[0] = 0x12U;
        uri_txn[1] = 0x00U;
        uri_txn[2] = 0x5FU;
        uri_txn[3] = 0x22U;
        uri_txn[4] = 0x80U;
        uri_txn[5] = 0x00U;
        uri_txn[6] = 0x00U;
        uri_txn[7] = 0x00U;
        uri_txn[8] = 0x24U;
        uri_txn[9] = 0x00U;
        uri_txn[10] = 0x00U;
        uri_txn[11] = 0x00U;
        uri_txn[12] = 0x00U;
        uri_txn[13] = 0x2EU;
        /* H04: DestinationTag from WIN WDT when stored */
        {
            uint8_t wdtb[4];
            uint32_t uri_tag = 0;
            if (state_foreign(wdtb, 4, "WDT", 3, aid, 32, hook_acc, 20) == 4)
                uri_tag = (uint32_t)UINT32_FROM_BUF(wdtb);
            uri_txn[14] = (uint8_t)((uri_tag >> 24) & 0xFFU);
            uri_txn[15] = (uint8_t)((uri_tag >> 16) & 0xFFU);
            uri_txn[16] = (uint8_t)((uri_tag >> 8) & 0xFFU);
            uri_txn[17] = (uint8_t)(uri_tag & 0xFFU);
        }
        uri_txn[18] = 0x20U;
        uri_txn[19] = 0x1AU;
        uri_txn[24] = 0x20U;
        uri_txn[25] = 0x1BU;
        uri_txn[30] = 0x68U;
        uri_txn[31] = 0x40U;
        uri_txn[39] = 0x73U;
        uri_txn[40] = 0x21U;
        uri_txn[74] = 0x81U;
        uri_txn[75] = 0x14U;
        uri_txn[96] = 0x83U;
        uri_txn[97] = 0x14U;
        {
            uint32_t fls = (uint32_t)ledger_seq() + 1U;
            uri_txn[20] = (uint8_t)((fls >> 24) & 0xFFU);
            uri_txn[21] = (uint8_t)((fls >> 16) & 0xFFU);
            uri_txn[22] = (uint8_t)((fls >> 8) & 0xFFU);
            uri_txn[23] = (uint8_t)(fls & 0xFFU);
            uint32_t lls = fls + 4U;
            uri_txn[26] = (uint8_t)((lls >> 24) & 0xFFU);
            uri_txn[27] = (uint8_t)((lls >> 16) & 0xFFU);
            uri_txn[28] = (uint8_t)((lls >> 8) & 0xFFU);
            uri_txn[29] = (uint8_t)(lls & 0xFFU);
        }
        {
            int i;
            for (i = 0; GUARD(20), i < 20; ++i)
            {
                uri_txn[76 + i] = hook_acc[i];
                uri_txn[98 + i] = uri_dest[i];
            }
        }
        {
            int64_t edlen = etxn_details(uri_txn + 118, 160U);
            if (edlen < 105)
                NOPE("URI Remit details failed");
            uint8_t* p = uri_txn + 118 + (uint32_t)edlen;
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
            uri_len = (uint32_t)(p - uri_txn);
        }
        {
            int64_t fee = etxn_fee_base(uri_txn, uri_len);
            if (fee < 0)
                NOPE("URI Remit fee quote failed");
            {
                uint8_t* b = uri_txn + 31;
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
        if (uri_len == 0 || uri_len > 384U)
            NOPE("URI Remit build failed");
    }

    /* Treasury fee payout */
    if (has_bids && do_fee)
    {
        if (is_iou)
        {
            uint8_t sto[49];
            if (float_sto((uint32_t)sto, 49,
                          (uint32_t)currency, 20,
                          (uint32_t)issuer, 20,
                          fee_xfl, sfAmount) != 49)
                NOPE("treasury IOU encode failed");
            {
                volatile uint64_t* zq = (volatile uint64_t*)treas_txn;
                int z;
                for (z = 0; GUARD(64), z < 64; ++z)
                    zq[z] = 0ULL;
            }
            treas_txn[0] = 0x12U;
            treas_txn[1] = 0x00U;
            treas_txn[2] = 0x5FU;
            treas_txn[3] = 0x22U;
            treas_txn[4] = 0x80U;
            treas_txn[5] = 0x00U;
            treas_txn[6] = 0x00U;
            treas_txn[7] = 0x00U;
            treas_txn[8] = 0x24U;
            treas_txn[9] = 0x00U;
            treas_txn[10] = 0x00U;
            treas_txn[11] = 0x00U;
            treas_txn[12] = 0x00U;
            treas_txn[13] = 0x2EU;
            treas_txn[14] = 0x00U;
            treas_txn[15] = 0x00U;
            treas_txn[16] = 0x00U;
            treas_txn[17] = 0x00U;
            treas_txn[18] = 0x20U;
            treas_txn[19] = 0x1AU;
            treas_txn[24] = 0x20U;
            treas_txn[25] = 0x1BU;
            treas_txn[30] = 0x68U;
            treas_txn[31] = 0x40U;
            treas_txn[39] = 0x73U;
            treas_txn[40] = 0x21U;
            treas_txn[74] = 0x81U;
            treas_txn[75] = 0x14U;
            treas_txn[96] = 0x83U;
            treas_txn[97] = 0x14U;
            {
                uint32_t fls = (uint32_t)ledger_seq() + 1U;
                treas_txn[20] = (uint8_t)((fls >> 24) & 0xFFU);
                treas_txn[21] = (uint8_t)((fls >> 16) & 0xFFU);
                treas_txn[22] = (uint8_t)((fls >> 8) & 0xFFU);
                treas_txn[23] = (uint8_t)(fls & 0xFFU);
                uint32_t lls = fls + 4U;
                treas_txn[26] = (uint8_t)((lls >> 24) & 0xFFU);
                treas_txn[27] = (uint8_t)((lls >> 16) & 0xFFU);
                treas_txn[28] = (uint8_t)((lls >> 8) & 0xFFU);
                treas_txn[29] = (uint8_t)(lls & 0xFFU);
            }
            {
                int i;
                for (i = 0; GUARD(20), i < 20; ++i)
                {
                    treas_txn[76 + i] = hook_acc[i];
                    treas_txn[98 + i] = treasury[i];
                }
            }
            {
                int64_t edlen = etxn_details(treas_txn + 118, 160U);
                if (edlen < 105)
                    NOPE("treasury Remit details failed");
                uint8_t* p = treas_txn + 118 + (uint32_t)edlen;
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
                treas_len = (uint32_t)(p - treas_txn);
            }
            {
                int64_t fee = etxn_fee_base(treas_txn, treas_len);
                if (fee < 0)
                    NOPE("treasury Remit fee quote failed");
                {
                    uint8_t* b = treas_txn + 31;
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
            if (treas_len == 0 || treas_len > 512U)
                NOPE("treasury Remit build failed");
        }
        else
        {
            uint32_t sz = 0;
            PREPARE_PAYMENT_SIMPLE(treas_txn, fee_drops, treasury, 0, 0, sz);
            treas_len = sz;
            if (treas_len == 0 || treas_len > 512U)
                NOPE("treasury Payment build failed");
        }
    }

    /* Seller remainder payout */
    if (has_bids)
    {
        int pay_seller = 0;
        if (is_iou)
        {
            if (float_compare(seller_xfl, 0, COMPARE_GREATER) == 1)
                pay_seller = 1;
        }
        else if (seller_drops > 0ULL)
            pay_seller = 1;

        if (pay_seller)
        {
            if (is_iou)
            {
                uint8_t sto[49];
                if (float_sto((uint32_t)sto, 49,
                              (uint32_t)currency, 20,
                              (uint32_t)issuer, 20,
                              seller_xfl, sfAmount) != 49)
                    NOPE("seller IOU encode failed");
                {
                    volatile uint64_t* zq = (volatile uint64_t*)seller_txn;
                    int z;
                    for (z = 0; GUARD(64), z < 64; ++z)
                        zq[z] = 0ULL;
                }
                seller_txn[0] = 0x12U;
                seller_txn[1] = 0x00U;
                seller_txn[2] = 0x5FU;
                seller_txn[3] = 0x22U;
                seller_txn[4] = 0x80U;
                seller_txn[5] = 0x00U;
                seller_txn[6] = 0x00U;
                seller_txn[7] = 0x00U;
                seller_txn[8] = 0x24U;
                seller_txn[9] = 0x00U;
                seller_txn[10] = 0x00U;
                seller_txn[11] = 0x00U;
                seller_txn[12] = 0x00U;
                seller_txn[13] = 0x2EU;
                seller_txn[14] = 0x00U;
                seller_txn[15] = 0x00U;
                seller_txn[16] = 0x00U;
                seller_txn[17] = 0x00U;
                seller_txn[18] = 0x20U;
                seller_txn[19] = 0x1AU;
                seller_txn[24] = 0x20U;
                seller_txn[25] = 0x1BU;
                seller_txn[30] = 0x68U;
                seller_txn[31] = 0x40U;
                seller_txn[39] = 0x73U;
                seller_txn[40] = 0x21U;
                seller_txn[74] = 0x81U;
                seller_txn[75] = 0x14U;
                seller_txn[96] = 0x83U;
                seller_txn[97] = 0x14U;
                {
                    uint32_t fls = (uint32_t)ledger_seq() + 1U;
                    seller_txn[20] = (uint8_t)((fls >> 24) & 0xFFU);
                    seller_txn[21] = (uint8_t)((fls >> 16) & 0xFFU);
                    seller_txn[22] = (uint8_t)((fls >> 8) & 0xFFU);
                    seller_txn[23] = (uint8_t)(fls & 0xFFU);
                    uint32_t lls = fls + 4U;
                    seller_txn[26] = (uint8_t)((lls >> 24) & 0xFFU);
                    seller_txn[27] = (uint8_t)((lls >> 16) & 0xFFU);
                    seller_txn[28] = (uint8_t)((lls >> 8) & 0xFFU);
                    seller_txn[29] = (uint8_t)(lls & 0xFFU);
                }
                {
                    int i;
                    for (i = 0; GUARD(20), i < 20; ++i)
                    {
                        seller_txn[76 + i] = hook_acc[i];
                        seller_txn[98 + i] = seller[i];
                    }
                }
                {
                    int64_t edlen = etxn_details(seller_txn + 118, 160U);
                    if (edlen < 105)
                        NOPE("seller Remit details failed");
                    uint8_t* p = seller_txn + 118 + (uint32_t)edlen;
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
                    seller_len = (uint32_t)(p - seller_txn);
                }
                {
                    int64_t fee = etxn_fee_base(seller_txn, seller_len);
                    if (fee < 0)
                        NOPE("seller Remit fee quote failed");
                    {
                        uint8_t* b = seller_txn + 31;
                        *b++ = (uint8_t)(0b01000000
                                         + ((fee >> 56) & 0b00111111));
                        *b++ = (uint8_t)((fee >> 48) & 0xFFU);
                        *b++ = (uint8_t)((fee >> 40) & 0xFFU);
                        *b++ = (uint8_t)((fee >> 32) & 0xFFU);
                        *b++ = (uint8_t)((fee >> 24) & 0xFFU);
                        *b++ = (uint8_t)((fee >> 16) & 0xFFU);
                        *b++ = (uint8_t)((fee >> 8) & 0xFFU);
                        *b++ = (uint8_t)((fee >> 0) & 0xFFU);
                    }
                }
                if (seller_len == 0 || seller_len > 512U)
                    NOPE("seller Remit build failed");
            }
            else
            {
                uint32_t sz = 0;
                PREPARE_PAYMENT_SIMPLE(seller_txn, seller_drops, seller,
                                       0, 0, sz);
                seller_len = sz;
                if (seller_len == 0 || seller_len > 512U)
                    NOPE("seller Payment build failed");
            }
        }
    }

    /* C01: zero builds for already-paid legs */
    if (!emit_uri)
    {
        need_uri = 0;
        uri_len = 0;
    }
    if (!emit_treas)
        treas_len = 0;
    if (!emit_seller)
        seller_len = 0;

    /* -------- Emit → SMAP (URI → treasury → seller); commit in cbak -------- */
    {
        uint8_t emitting = 0;
        if (emit_uri)
            emitting = (uint8_t)(emitting | SPEN_BIT_URI);
        if (emit_treas)
            emitting = (uint8_t)(emitting | SPEN_BIT_TREAS);
        if (emit_seller)
            emitting = (uint8_t)(emitting | SPEN_BIT_SELL);
        /* SEXP = durable full expected (union); SPEN = this invoke only */
        {
            uint8_t sexp = 0;
            if (state_foreign(&sexp, 1, "SEXP", 4, aid, 32, hook_acc, 20) != 1)
                sexp = 0;
            sexp = (uint8_t)(sexp | emitting);
            if (sexp != 0)
            {
                if (state_foreign_set(&sexp, 1, "SEXP", 4, aid, 32,
                                      hook_acc, 20) != 1)
                    NOPE("SEXP write failed");
            }
            if (emitting != 0)
            {
                if (state_foreign_set(&emitting, 1, "SPEN", 4, aid, 32,
                                      hook_acc, 20) != 1)
                    NOPE("SPEN write failed");
            }
        }
        /* Clear SSF on fresh attempt (PW5-M02: do NOT clear LCKU here) */
        state_foreign_set(0, 0, "SSF", 3, aid, 32, hook_acc, 20);
    }

    if (emit_uri)
    {
        uint8_t emh[32];
        if (emit(SBUF(emh), uri_txn, uri_len) != 32)
            NOPE("URI Remit emit failed");
        {
            uint8_t sm[67];
            int i;
            for (i = 0; GUARD(67), i < 67; ++i)
                sm[i] = 0;
            for (i = 0; GUARD(32), i < 32; ++i)
                sm[i] = aid[i];
            sm[32] = (uint8_t)SMAP_KIND_URI;
            sm[33] = 0;
            if (state_set(sm, 67, SBUF(emh)) != 67)
                NOPE("URI SMAP write failed");
        }
    }
    if (treas_len > 0)
    {
        uint8_t emh[32];
        if (emit(SBUF(emh), treas_txn, treas_len) != 32)
            NOPE("treasury emit failed");
        {
            uint8_t sm[67];
            int i;
            for (i = 0; GUARD(67), i < 67; ++i)
                sm[i] = 0;
            for (i = 0; GUARD(32), i < 32; ++i)
                sm[i] = aid[i];
            sm[32] = (uint8_t)SMAP_KIND_TREAS;
            sm[33] = is_iou ? SMAP_IS_IOU : 0;
            if (is_iou)
            {
                uint8_t* ap = sm + 34;
                UINT64_TO_BUF(ap, (uint64_t)fee_xfl);
            }
            else
            {
                uint8_t* ap = sm + 34;
                UINT64_TO_BUF(ap, fee_drops);
            }
            for (i = 0; GUARD(20), i < 20; ++i)
                sm[42 + i] = treasury[i];
            if (state_set(sm, 67, SBUF(emh)) != 67)
                NOPE("treasury SMAP write failed");
        }
    }
    if (seller_len > 0)
    {
        uint8_t emh[32];
        if (emit(SBUF(emh), seller_txn, seller_len) != 32)
            NOPE("seller emit failed");
        {
            uint8_t sm[67];
            int i;
            for (i = 0; GUARD(67), i < 67; ++i)
                sm[i] = 0;
            for (i = 0; GUARD(32), i < 32; ++i)
                sm[i] = aid[i];
            sm[32] = (uint8_t)SMAP_KIND_SELL;
            sm[33] = is_iou ? SMAP_IS_IOU : 0;
            if (is_iou)
            {
                uint8_t* ap = sm + 34;
                UINT64_TO_BUF(ap, (uint64_t)seller_xfl);
            }
            else
            {
                uint8_t* ap = sm + 34;
                UINT64_TO_BUF(ap, seller_drops);
            }
            for (i = 0; GUARD(20), i < 20; ++i)
                sm[42 + i] = seller[i];
            if (state_set(sm, 67, SBUF(emh)) != 67)
                NOPE("seller SMAP write failed");
        }
    }

    /* Nothing to emit and already fully flagged → commit now (rare) */
    {
        uint8_t sp = 0;
        int busy = (state_foreign(&sp, 1, "SPEN", 4, aid, 32, hook_acc, 20) == 1
                    && sp != 0);
        if (!busy && !emit_uri && treas_len == 0 && seller_len == 0)
            NOPE("nothing to settle");
    }

    /* Do NOT LCK-/ACTIVE-/clear AID here — cbak commit (C01) */
    DONE("Settlement pending");
}
