/**
 * Auction House V2 - AuctionCreate.c
 *
 * Create auction via Remit of exactly one URIToken to host.
 * One fat file; hookapi.h only. All logic in hook()/cbak().
 *
 * Install: param-free. Invokes and Payments passthrough.
 * Remits: valid Create only; invalid/non-Create Remits reject (no passthrough).
 * Outgoing host -> ok.
 *
 * Seller foreign ns = account ID (20) zero-padded to 32 (same as AuctionSub):
 *   SUBEXP (8) uint64 BE, ACTIVE (2) uint16 BE, CAP (2) uint16 BE
 * Require SUBEXP > ledger_last_time and ACTIVE < CAP; on success ACTIVE += 1.
 *
 * Remit params:
 *   DUR (req) exactly 8 bytes BE uint64 seconds 300..2592000
 *   SP  (opt) 8 bytes drops (XAH) or XFL (IOU)
 *   MB  (opt) 8 bytes; if set must be > 0
 *   BN  (opt) 8 bytes; if SP+BN set, BN > SP
 *   CUR (opt) 3-letter ASCII, 20 raw, or 40 hex ASCII; omit = XAH
 *   ISS (opt) 20-byte AccountID; required with CUR for IOU
 *
 * AID ns = sha512h(otxn_id || URIToken id). Keys under AID (hook owns state):
 *   DUR, SP, MB, BN, CUR, ISS (if present), SLR, URI, EXP, ST=1 (open)
 * Skip optional keys when absent. No soft-close. No commissions/treasury.
 *
 * Create gates (fail-closed Remit rollback):
 *   1) URIToken must not be burnable (lsfBurnable / tfBurnable bit).
 *   2) Seller AccountRoot must not have remits disabled
 *      (lsfDisallowIncomingRemit / asfDisallowIncomingRemit).
 *   3) Seller must not have DepositAuth (lsfDepositAuth) - entry gate
 *      so Finalise XAH Payment / Remit payouts can land.
 *   4) IOU issuer only (XAH has no issuer): refuse a TransferRate that
 *      is present and is neither 0 nor the parity sentinel, a global
 *      freeze, clawback, or an issuer-side freeze on an existing host
 *      line. A missing line is not a freeze. LCK stays face value.
 *
 * IOU: if hook has no trustline for CUR+ISS, emit TrustSet (huge limit,
 * tfSetNoRipple). Stamps current FEE and TREASURY onto the auction.
 * Fail-closed on emit / any required state write / ACTIVE bump failure.
 *
 * Host local state (state/state_set - share HookNamespace with Bids/Finalise):
 *   TAC (4 BE uint32) total auctions created; bump +1 after ACTIVE ok.
 *   Missing TAC -> 0. uint32 wrap -> fail-closed.
 */
#define HAS_CALLBACK
#include "hookapi.h"

#define NOPE(msg) rollback(SBUF(msg), __LINE__)
/* use stock DONE from macro.h */

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
#ifndef LSF_BURNABLE
/* URIToken lsfBurnable / tfBurnable - xahaud ls_flags.h */
#define LSF_BURNABLE 0x00000001U
#endif
#ifndef LSF_DISALLOW_INCOMING_REMIT
/* AccountRoot: asfDisallowIncomingRemit ("lsfDisableRemit" equivalent) */
#define LSF_DISALLOW_INCOMING_REMIT 0x80000000U
#endif
#ifndef LSF_DEPOSIT_AUTH
/* AccountRoot: asfDepositAuth / lsfDepositAuth */
#define LSF_DEPOSIT_AUTH 0x01000000U
#endif
#ifndef LSF_GLOBAL_FREEZE
/* AccountRoot lsfGlobalFreeze - rshooks vendor xahaud-hook/ls_flags.h */
#define LSF_GLOBAL_FREEZE 0x00400000U
#endif
#ifndef LSF_ALLOW_TRUSTLINE_CLAWBACK
/* AccountRoot lsfAllowTrustLineClawback - same ls_flags.h (not asf 17 shifted) */
#define LSF_ALLOW_TRUSTLINE_CLAWBACK 0x00001000U
#endif
#ifndef LSF_LOW_FREEZE
/* RippleState lsfLowFreeze - freeze set by the low account */
#define LSF_LOW_FREEZE 0x00400000U
#endif
#ifndef LSF_HIGH_FREEZE
/* RippleState lsfHighFreeze - freeze set by the high account */
#define LSF_HIGH_FREEZE 0x00800000U
#endif
#ifndef LSF_LOW_DEEP_FREEZE
/* RippleState lsfLowDeepFreeze */
#define LSF_LOW_DEEP_FREEZE 0x02000000U
#endif
#ifndef LSF_HIGH_DEEP_FREEZE
/* RippleState lsfHighDeepFreeze */
#define LSF_HIGH_DEEP_FREEZE 0x04000000U
#endif
/* sfTransferRate: absent or 0 is no fee. 1000000000 is parity (1.0). */
#define TRANSFER_RATE_PARITY 1000000000U

#define KEY_SUBEXP       "SUBEXP"
#define KEY_SUBEXP_LEN   6
#define KEY_ACTIVE       "ACTIVE"
#define KEY_ACTIVE_LEN   6
#define KEY_CAP          "CAP"
#define KEY_CAP_LEN      3

#define DUR_MIN_S        300U
#define DUR_MAX_S        2592000U /* 30 days */

/* TrustSet LimitAmount = 1e15 (receive-capable). */
#ifndef tfSetNoRipple
#define tfSetNoRipple 0x00020000UL
#endif
#define PREPARE_TRUSTSET_IOU_SIZE 512U
#define PREPARE_TRUSTSET_IOU(buf_out_master, currency20, issuer20, sizeout)\
{\
    int64_t _lim_xfl = float_set(15, 1);\
    if (_lim_xfl < 0)\
        NOPE("TrustSet limit float_set failed");\
    uint8_t _sto[49];\
    if (float_sto((uint32_t)_sto, 49,\
                  (uint32_t)(currency20), 20,\
                  (uint32_t)(issuer20), 20,\
                  _lim_xfl, sfLimitAmount) != 49)\
        NOPE("TrustSet LimitAmount encode failed");\
    uint8_t _amt[48];\
    {\
        int _i;\
        for (_i = 0; GUARD(48), _i < 48; ++_i)\
            _amt[_i] = _sto[_i + 1];\
    }\
    uint8_t* buf_out = (uint8_t*)(buf_out_master);\
    uint8_t* _txn0 = buf_out;\
    uint8_t _acc[20];\
    hook_account(SBUF(_acc));\
    uint32_t _cls = (uint32_t)ledger_seq();\
    _01_02_ENCODE_TT                   (buf_out, ttTRUST_SET                   );\
    /* tfSetNoRipple: host lines must not ripple one issuer's IOU for another */\
    _02_02_ENCODE_FLAGS                (buf_out, tfCANONICAL | tfSetNoRipple   );\
    _02_04_ENCODE_SEQUENCE             (buf_out, 0                             );\
    _02_26_ENCODE_FLS                  (buf_out, _cls + 1                      );\
    _02_27_ENCODE_LLS                  (buf_out, _cls + 5                      );\
    ENCODE_TL                          (buf_out, _amt, amLIMITAMOUNT           );\
    uint8_t* _fee_ptr = buf_out;\
    _06_08_ENCODE_DROPS_FEE            (buf_out, 0                             );\
    _07_03_ENCODE_SIGNING_PUBKEY_NULL  (buf_out                                );\
    _08_01_ENCODE_ACCOUNT_SRC          (buf_out, _acc                          );\
    {\
        uint32_t _used = (uint32_t)(buf_out - _txn0);\
        int64_t _edlen = etxn_details((uint32_t)buf_out, PREPARE_TRUSTSET_IOU_SIZE - _used);\
        if (_edlen < 0)\
            NOPE("TrustSet etxn_details failed");\
        buf_out += _edlen;\
    }\
    sizeout = (uint32_t)(buf_out - _txn0);\
    {\
        int64_t _fee = etxn_fee_base((uint32_t)_txn0, sizeout);\
        if (_fee < 0)\
            NOPE("TrustSet fee quote failed");\
        _06_08_ENCODE_DROPS_FEE        (_fee_ptr, _fee                         );\
    }\
}

int64_t cbak(uint32_t what)
{
    _g(1, 1);

    uint8_t hook_acc[20];
    hook_account(SBUF(hook_acc));

    uint8_t txid[32];
    if (otxn_id(SBUF(txid), 0) != 32)
        DONE("cbak no id");

    uint8_t aid[32];
    if (state(SBUF(aid), SBUF(txid)) != 32)
        DONE("TrustSet cbak unmap");

    int ok = 0;
    if (what == 0)
    {
        ok = 1;
        uint8_t trb[1];
        if (meta_slot(1) >= 0 && slot_subfield(1, sfTransactionResult, 2) >= 0
            && slot(SBUF(trb), 2) == 1 && trb[0] != 0)
            ok = 0;
    }

    state_set(0, 0, SBUF(txid));

    /* PW-C01: Remit-back cbak when TSF recovery flag set */
    {
        uint8_t tsf = 0;
        if (state_foreign(&tsf, 1, "TSF", 3, aid, 32, hook_acc, 20) == 1
            && tsf != 0)
        {
            if (ok)
            {
                state_foreign_set(0, 0, "URI", 3, aid, 32, hook_acc, 20);
                state_foreign_set(0, 0, "SLR", 3, aid, 32, hook_acc, 20);
                state_foreign_set(0, 0, "TSF", 3, aid, 32, hook_acc, 20);
                DONE("TrustSet fail Remit ok");
            }
            DONE("TrustSet fail Remit strand");
        }
    }

    if (ok)
    {
        state_foreign_set(0, 0, "CPR", 3, aid, 32, hook_acc, 20);
        DONE("TrustSet cbak ok");
    }

    /* TrustSet fail: ACTIVE-1, clear live keys, keep SLR+URI, write TSF */
    {
        uint8_t seller[20];
        if (state_foreign(SBUF(seller), "SLR", 3, aid, 32, hook_acc, 20) == 20)
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
                    state_foreign_set(actb, 2, KEY_ACTIVE, KEY_ACTIVE_LEN,
                                      seller_ns, 32, hook_acc, 20);
                }
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
    state_foreign_set(0, 0, "DUR", 3, aid, 32, hook_acc, 20);
    state_foreign_set(0, 0, "CPR", 3, aid, 32, hook_acc, 20);
    {
        uint8_t one = 1;
        state_foreign_set(&one, 1, "TSF", 3, aid, 32, hook_acc, 20);
    }

    /* PW3-M05: Remit URI->SLR map-before-emit. Finalise/M01 backup if no emit. */
    {
        uint8_t uri[32];
        uint8_t seller[20];
        if (state_foreign(SBUF(uri), "URI", 3, aid, 32, hook_acc, 20) == 32
            && state_foreign(SBUF(seller), "SLR", 3, aid, 32, hook_acc, 20)
               == 20)
        {
            if (etxn_reserve(1) == 1)
            {
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
                    if (edlen >= 105)
                    {
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
                        {
                            int64_t fee = etxn_fee_base(uri_txn, uri_len);
                            if (fee >= 0)
                            {
                                uint8_t* b = uri_txn + 31;
                                *b++ = (uint8_t)(0b01000000
                                                 + ((fee >> 56) & 0b00111111));
                                *b++ = (uint8_t)((fee >> 48) & 0xFFU);
                                *b++ = (uint8_t)((fee >> 40) & 0xFFU);
                                *b++ = (uint8_t)((fee >> 32) & 0xFFU);
                                *b++ = (uint8_t)((fee >> 24) & 0xFFU);
                                *b++ = (uint8_t)((fee >> 16) & 0xFFU);
                                *b++ = (uint8_t)((fee >> 8) & 0xFFU);
                                *b++ = (uint8_t)((fee >> 0) & 0xFFU);
                                if (uri_len > 0 && uri_len <= 384U)
                                {
                                    /* PW3-M05: map BEFORE emit; map fail -> no emit */
                                    uint8_t emh[32];
                                    if (util_sha512h(SBUF(emh), uri_txn,
                                                     uri_len) != 32)
                                        DONE("TrustSet cbak fail TSF set");
                                    if (state_set(aid, 32, SBUF(emh)) != 32)
                                        DONE("TrustSet cbak fail Remit map");
                                    {
                                        uint8_t got[32];
                                        if (emit(SBUF(got), uri_txn, uri_len)
                                            != 32)
                                        {
                                            state_set(0, 0, SBUF(emh));
                                            DONE("TrustSet cbak fail TSF set");
                                        }
                                        {
                                            int same = 0;
                                            BUFFER_EQUAL(same, got, emh, 32);
                                            if (!same)
                                            {
                                                state_set(0, 0, SBUF(emh));
                                                if (state_set(aid, 32,
                                                              SBUF(got))
                                                    != 32)
                                                    DONE("TrustSet cbak fail Remit map");
                                            }
                                        }
                                        DONE("TrustSet cbak fail reclaiming");
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    DONE("TrustSet cbak fail TSF set");
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
                            /* KVT #13: host gen-0 Remit carries at most
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
                                        /* Keep STAmount wire bits - match bal UINT64_FROM_BUF */
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

    /* Payments / Invokes passthrough; Create is Remit-only */
    if (tt == ttPAYMENT || tt == ttINVOKE)
        DONE("Passthrough");

    if (tt != ttREMIT)
        DONE("Passthrough");

    /* -------- Remit: Create only (no Remit passthrough) -------- */
    uint8_t dest[20];
    if (otxn_field(SBUF(dest), sfDestination) != 20)
        NOPE("missing Destination");
    {
        int to_host = 0;
        BUFFER_EQUAL(to_host, dest, hook_acc, 20);
        if (!to_host)
            NOPE("Remit must be to host");
    }

    /* Exactly one URIToken id */
    uint8_t uri[32];
    {
        uint8_t ids[160];
        int64_t idslen = otxn_field(SBUF(ids), sfURITokenIDs);
        if (idslen < 33)
            NOPE("URITokenIDs missing or too short");
        if (ids[0] != 0x20U)
            NOPE("URITokenIDs must be a single 32-byte id");
        if (idslen != 33)
            NOPE("URITokenIDs must contain exactly one token");
        {
            int i;
            for (i = 0; GUARD(32), i < 32; ++i)
                uri[i] = ids[i + 1];
        }
    }

    /* Current URI owner must be the Remit Account (seller) */
    {
        uint8_t owner[20];
        uint8_t kl[34];
        int found = 0;
        if (util_keylet(SBUF(kl), KEYLET_UNCHECKED, SBUF(uri), 0, 0, 0, 0) == 34
            && slot_set(SBUF(kl), 1) >= 0)
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
            BUFFER_EQUAL(ok, owner, otxn_acc, 20);
            if (!ok)
                NOPE("not URI owner");
        }
        /* Reject burnable URIToken (lsfBurnable / tfBurnable) */
        {
            uint32_t uflags = 0;
            if (slot_subfield(1, sfFlags, 3) >= 0)
            {
                uint8_t fb[4];
                if (slot(SBUF(fb), 3) == 4)
                    uflags = (uint32_t)UINT32_FROM_BUF(fb);
            }
            if (uflags & LSF_BURNABLE)
                NOPE("Burnable URITokens not allowed");
        }
    }

    /* Seller must accept incoming Remits (Finalise Remits URI/IOU back) */
    {
        uint8_t akl[34];
        if (util_keylet(SBUF(akl), KEYLET_ACCOUNT, otxn_acc, 20, 0, 0, 0, 0)
            != 34)
            NOPE("seller AccountRoot keylet failed");
        if (slot_set(SBUF(akl), 4) < 0)
            NOPE("seller AccountRoot not found");
        uint32_t sflags = 0;
        if (slot_subfield(4, sfFlags, 5) >= 0)
        {
            uint8_t fb[4];
            if (slot(SBUF(fb), 5) == 4)
                sflags = (uint32_t)UINT32_FROM_BUF(fb);
        }
        if (sflags & LSF_DISALLOW_INCOMING_REMIT)
            NOPE("seller remits disabled");
        if (sflags & LSF_DEPOSIT_AUTH)
            NOPE("seller DepositAuth");
    }

    /* Seller subscription gate (Sub individual keys) */
    uint8_t seller_ns[32];
    {
        int i;
        for (i = 0; GUARD(20), i < 20; ++i)
            seller_ns[i] = otxn_acc[i];
        for (i = 20; GUARD(32), i < 32; ++i)
            seller_ns[i] = 0;
    }

    uint8_t expb[8];
    uint8_t actb[2];
    uint8_t capb[2];
    uint64_t subexp = 0ULL;
    uint16_t active = 0;
    uint16_t cap = 0;
    if (state_foreign(expb, 8, KEY_SUBEXP, KEY_SUBEXP_LEN,
                      seller_ns, 32, hook_acc, 20) != 8)
        NOPE("not subscribed");
    subexp = UINT64_FROM_BUF(expb);
    if (state_foreign(actb, 2, KEY_ACTIVE, KEY_ACTIVE_LEN,
                      seller_ns, 32, hook_acc, 20) == 2)
        active = (uint16_t)(((uint16_t)actb[0] << 8) | actb[1]);
    if (state_foreign(capb, 2, KEY_CAP, KEY_CAP_LEN,
                      seller_ns, 32, hook_acc, 20) != 2)
        NOPE("not subscribed");
    cap = (uint16_t)(((uint16_t)capb[0] << 8) | capb[1]);

    int64_t now = ledger_last_time();
    if (now < 0)
        NOPE("ledger time unavailable");
    if (!(subexp > (uint64_t)now))
        NOPE("not subscribed");
    if (!(active < cap))
        NOPE("at CAP");

    /* -------- Remit params -------- */
    uint8_t dur_raw[8];
    int64_t dur_len = otxn_param(SBUF(dur_raw), "DUR", 3);
    if (dur_len < 0)
        NOPE("DUR required");
    if (dur_len != 8)
        NOPE("DUR must be 8 bytes");
    uint64_t dur = UINT64_FROM_BUF(dur_raw);
    if (dur < (uint64_t)DUR_MIN_S || dur > (uint64_t)DUR_MAX_S)
        NOPE("DUR out of bounds");

    uint8_t sp_buf[8];
    uint8_t mb_buf[8];
    uint8_t bn_buf[8];
    int has_sp = 0;
    int has_mb = 0;
    int has_bn = 0;
    {
        int64_t n;
        n = otxn_param(SBUF(sp_buf), "SP", 2);
        if (n >= 0)
        {
            if (n != 8)
                NOPE("SP must be 8 bytes");
            has_sp = 1;
        }
        n = otxn_param(SBUF(mb_buf), "MB", 2);
        if (n >= 0)
        {
            if (n != 8)
                NOPE("MB must be 8 bytes");
            has_mb = 1;
        }
        n = otxn_param(SBUF(bn_buf), "BN", 2);
        if (n >= 0)
        {
            if (n != 8)
                NOPE("BN must be 8 bytes");
            has_bn = 1;
        }
    }

    /* Currency / issuer */
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
        uint8_t cur_raw[40];
        int64_t clen = otxn_param(SBUF(cur_raw), "CUR", 3);
        int64_t ilen = otxn_param(SBUF(issuer), "ISS", 3);
        int has_c = (clen >= 0);
        int has_i = (ilen >= 0);

        if (has_c && !has_i)
            NOPE("ISS required with CUR");
        if (!has_c && has_i)
            NOPE("CUR required with ISS");

        if (has_c)
        {
            if (ilen != 20)
                NOPE("ISS must be 20 bytes");

            if (clen == 3)
            {
                /* ISO 3-letter -> XRPL 20-byte (code at bytes 12..14) */
                currency[12] = cur_raw[0];
                currency[13] = cur_raw[1];
                currency[14] = cur_raw[2];
            }
            else if (clen == 20)
            {
                int i;
                for (i = 0; GUARD(20), i < 20; ++i)
                    currency[i] = cur_raw[i];
            }
            else if (clen == 40)
            {
                int i;
                int bad = 0;
                for (i = 0; GUARD(20), i < 20; ++i)
                {
                    uint8_t hi = 0;
                    uint8_t lo = 0;
                    uint8_t c0 = cur_raw[i * 2];
                    uint8_t c1 = cur_raw[i * 2 + 1];
                    if (c0 >= '0' && c0 <= '9')
                        hi = (uint8_t)(c0 - '0');
                    else if (c0 >= 'a' && c0 <= 'f')
                        hi = (uint8_t)(c0 - 'a' + 10);
                    else if (c0 >= 'A' && c0 <= 'F')
                        hi = (uint8_t)(c0 - 'A' + 10);
                    else
                        bad = 1;
                    if (c1 >= '0' && c1 <= '9')
                        lo = (uint8_t)(c1 - '0');
                    else if (c1 >= 'a' && c1 <= 'f')
                        lo = (uint8_t)(c1 - 'a' + 10);
                    else if (c1 >= 'A' && c1 <= 'F')
                        lo = (uint8_t)(c1 - 'A' + 10);
                    else
                        bad = 1;
                    currency[i] = (uint8_t)((hi << 4) | lo);
                }
                if (bad)
                    NOPE("CUR hex invalid");
            }
            else
                NOPE("CUR bad length");

            /* non-zero currency + issuer */
            {
                int cz = 1;
                int iz = 1;
                int i;
                for (i = 0; GUARD(20), i < 20; ++i)
                {
                    if (currency[i] != 0)
                        cz = 0;
                    if (issuer[i] != 0)
                        iz = 0;
                }
                if (cz || iz)
                    NOPE("CUR/ISS must be non-zero");
            }
            is_iou = 1;
        }
    }

    /* MB > 0; BN > SP when both set */
    if (has_mb)
    {
        if (is_iou)
        {
            int64_t mb_xfl = (int64_t)UINT64_FROM_BUF(mb_buf);
            if (float_compare(mb_xfl, 0, COMPARE_GREATER) != 1)
                NOPE("MB must be > 0");
        }
        else
        {
            if (UINT64_FROM_BUF(mb_buf) == 0ULL)
                NOPE("MB must be > 0");
        }
    }
    if (has_sp && has_bn)
    {
        if (is_iou)
        {
            int64_t sp_xfl = (int64_t)UINT64_FROM_BUF(sp_buf);
            int64_t bn_xfl = (int64_t)UINT64_FROM_BUF(bn_buf);
            if (float_compare(bn_xfl, sp_xfl, COMPARE_GREATER) != 1)
                NOPE("BN must be > SP");
        }
        else
        {
            if (!(UINT64_FROM_BUF(bn_buf) > UINT64_FROM_BUF(sp_buf)))
                NOPE("BN must be > SP");
        }
    }

    /* AID = sha512h(otxn_id || URIToken id) */
    uint8_t aid[32];
    {
        uint8_t pre[64];
        uint8_t txid[32];
        if (otxn_id(SBUF(txid), 0) != 32)
            NOPE("otxn_id read failed");
        {
            int i;
            for (i = 0; GUARD(32), i < 32; ++i)
            {
                pre[i] = txid[i];
                pre[32 + i] = uri[i];
            }
        }
        if (util_sha512h(SBUF(aid), SBUF(pre)) != 32)
            NOPE("AID hash failed");
    }

    /* IOU TrustSet prep (fail-closed before state writes) */
    int need_ts = 0;
    uint8_t ts_txn[PREPARE_TRUSTSET_IOU_SIZE];
    uint32_t ts_len = 0;
    if (is_iou)
    {
        /* KVT #4. XAH never reaches here. LCK is not rescaled. */
        {
            uint8_t ikl[34];
            if (util_keylet(SBUF(ikl), KEYLET_ACCOUNT, issuer, 20, 0, 0, 0, 0)
                != 34)
                NOPE("issuer AccountRoot keylet failed");
            if (slot_set(SBUF(ikl), 12) < 0)
                NOPE("issuer AccountRoot not found");
            uint32_t iflags = 0;
            if (slot_subfield(12, sfFlags, 13) >= 0)
            {
                uint8_t fb[4];
                if (slot(SBUF(fb), 13) != 4)
                    NOPE("issuer flags read failed");
                iflags = (uint32_t)UINT32_FROM_BUF(fb);
            }
            /* Present and neither 0 nor parity means the issuer takes a fee. */
            if (slot_subfield(12, sfTransferRate, 13) >= 0)
            {
                uint8_t trb[4];
                if (slot(SBUF(trb), 13) != 4)
                    NOPE("issuer transfer rate unreadable");
                {
                    uint32_t tr = (uint32_t)UINT32_FROM_BUF(trb);
                    if (tr != 0U && tr != TRANSFER_RATE_PARITY)
                        NOPE("transfer rate set");
                }
            }
            if (iflags & LSF_GLOBAL_FREEZE)
                NOPE("issuer frozen");
            if (iflags & LSF_ALLOW_TRUSTLINE_CLAWBACK)
                NOPE("clawback issuer");
        }

        int host_is_iss = 0;
        BUFFER_EQUAL(host_is_iss, hook_acc, issuer, 20);
        if (!host_is_iss)
        {
            int has_line = 0;
            uint8_t lkl[34];
            int line_kl = util_keylet(SBUF(lkl), KEYLET_LINE,
                            hook_acc, 20, issuer, 20, currency, 20);
            if (line_kl != 34)
                NOPE("trustline keylet failed");
            if (slot_set(SBUF(lkl), 10) >= 0)
            {
                /* Require a positive receive limit on the hook side */
                int holder_low = 0;
                {
                    /* compare account ids as big-endian unsigned */
                    int i;
                    holder_low = 0;
                    for (i = 0; GUARD(20), i < 20; ++i)
                    {
                        if (hook_acc[i] < issuer[i])
                        {
                            holder_low = 1;
                            break;
                        }
                        if (hook_acc[i] > issuer[i])
                        {
                            holder_low = 0;
                            break;
                        }
                    }
                }
                /* Line exists. Issuer-side freeze or deep freeze blocks
                 * the host paying this IOU out. Host-side freeze does not.
                 * Low/high is account id order, same as the limit check. */
                {
                    uint32_t lflags = 0;
                    if (slot_subfield(10, sfFlags, 11) >= 0)
                    {
                        uint8_t fb[4];
                        if (slot(SBUF(fb), 11) != 4)
                            NOPE("trustline flags read failed");
                        lflags = (uint32_t)UINT32_FROM_BUF(fb);
                    }
                    {
                        uint32_t iss_fr = holder_low
                            ? (LSF_HIGH_FREEZE | LSF_HIGH_DEEP_FREEZE)
                            : (LSF_LOW_FREEZE | LSF_LOW_DEEP_FREEZE);
                        if (lflags & iss_fr)
                            NOPE("issuer frozen");
                    }
                }
                if (holder_low)
                {
                    if (slot_subfield(10, sfLowLimit, 11) >= 0)
                    {
                        int64_t lim = slot_float(11);
                        if (lim >= 0 && float_compare(lim, 0, COMPARE_GREATER) == 1)
                            has_line = 1;
                    }
                }
                else
                {
                    if (slot_subfield(10, sfHighLimit, 11) >= 0)
                    {
                        int64_t lim = slot_float(11);
                        if (lim >= 0 && float_compare(lim, 0, COMPARE_GREATER) == 1)
                            has_line = 1;
                    }
                }
            }
            if (!has_line)
            {
                need_ts = 1;
                etxn_reserve(1);
                PREPARE_TRUSTSET_IOU(ts_txn, currency, issuer, ts_len);
                if (ts_len == 0 || ts_len > PREPARE_TRUSTSET_IOU_SIZE)
                    NOPE("TrustSet build failed");
            }
        }
    }

    /* -------- AID state writes (skip optional keys when absent) -------- */
    {
        uint8_t dur_store[8];
        UINT64_TO_BUF(dur_store, dur);
        if (state_foreign_set(dur_store, 8, "DUR", 3, aid, 32, hook_acc, 20) != 8)
            NOPE("DUR write failed");
    }
    if (has_sp)
    {
        if (state_foreign_set(sp_buf, 8, "SP", 2, aid, 32, hook_acc, 20) != 8)
            NOPE("SP write failed");
    }
    if (has_mb)
    {
        if (state_foreign_set(mb_buf, 8, "MB", 2, aid, 32, hook_acc, 20) != 8)
            NOPE("MB write failed");
    }
    if (has_bn)
    {
        if (state_foreign_set(bn_buf, 8, "BN", 2, aid, 32, hook_acc, 20) != 8)
            NOPE("BN write failed");
    }
    if (is_iou)
    {
        if (state_foreign_set(currency, 20, "CUR", 3, aid, 32, hook_acc, 20) != 20)
            NOPE("CUR write failed");
        if (state_foreign_set(issuer, 20, "ISS", 3, aid, 32, hook_acc, 20) != 20)
            NOPE("ISS write failed");
    }
    if (state_foreign_set(otxn_acc, 20, "SLR", 3, aid, 32, hook_acc, 20) != 20)
        NOPE("SLR write failed");
    if (state_foreign_set(uri, 32, "URI", 3, aid, 32, hook_acc, 20) != 32)
        NOPE("URI write failed");
    {
        uint64_t exp = (uint64_t)now + dur;
        if (exp < (uint64_t)now)
            NOPE("EXP overflow");
        uint8_t expw[8];
        UINT64_TO_BUF(expw, exp);
        if (state_foreign_set(expw, 8, "EXP", 3, aid, 32, hook_acc, 20) != 8)
            NOPE("EXP write failed");
    }
    {
        uint8_t st = 1; /* open; 2 reserved settled */
        if (state_foreign_set(&st, 1, "ST", 2, aid, 32, hook_acc, 20) != 1)
            NOPE("ST write failed");
    }

    /* Lock the seller fee onto this auction. A later FEE/TREASURY
     * change applies to new listings only. Missing FEE snapshots as 0. */
    {
        uint8_t fb[2];
        fb[0] = 0;
        fb[1] = 0;
        if (state(fb, 2, "FEE", 3) == 2)
        {
            uint16_t fv = (uint16_t)(((uint16_t)fb[0] << 8) | fb[1]);
            if (fv > 5000U)
                NOPE("FEE corrupt");
        }
        if (state_foreign_set(fb, 2, "FEE", 3, aid, 32, hook_acc, 20) != 2)
            NOPE("FEE snapshot failed");
        {
            uint8_t tr[20];
            if (state(SBUF(tr), "TREASURY", 8) == 20)
            {
                int bad = 0;
                BUFFER_EQUAL(bad, tr, hook_acc, 20);
                if (!bad)
                {
                    if (state_foreign_set(tr, 20, "TREASURY", 8, aid, 32,
                                          hook_acc, 20) != 20)
                        NOPE("TREASURY snapshot failed");
                }
            }
        }
    }

    /* ACTIVE += 1 (may equal CAP after) */
    {
        uint16_t neu = (uint16_t)(active + 1U);
        uint8_t nw[2];
        nw[0] = (uint8_t)((neu >> 8) & 0xFFU);
        nw[1] = (uint8_t)(neu & 0xFFU);
        if (state_foreign_set(nw, 2, KEY_ACTIVE, KEY_ACTIVE_LEN,
                              seller_ns, 32, hook_acc, 20) != 2)
            NOPE("ACTIVE write failed");
    }

    /* Host local TAC += 1 (shared ns with Bids/Finalise when same HookNamespace) */
    {
        uint32_t tac = 0;
        uint8_t tbuf[4];
        if (state(tbuf, 4, "TAC", 3) == 4)
            tac = (uint32_t)UINT32_FROM_BUF(tbuf);
        if (tac == 0xFFFFFFFFU)
            NOPE("TAC overflow");
        tac = tac + 1U;
        UINT32_TO_BUF(tbuf, tac);
        if (state_set(tbuf, 4, "TAC", 3) != 4)
            NOPE("TAC write failed");
    }

    if (need_ts)
    {
        uint8_t one = 1;
        if (state_foreign_set(&one, 1, "CPR", 3, aid, 32, hook_acc, 20) != 1)
            NOPE("CPR write failed");
        uint8_t emh[32];
        if (emit(SBUF(emh), ts_txn, ts_len) != 32)
            NOPE("TrustSet emit failed");
        if (state_set(aid, 32, SBUF(emh)) != 32)
            NOPE("TrustSet map write failed");
        DONE("Auction created with TrustSet");
    }

    DONE("Auction created");
}
