/**
 * Auction House V2 - AuctionSub.c
 *
 * Seller subscription gate (XAH only). One fat file; hookapi.h only.
 * All logic in hook()/cbak() (Hooks guard: no extra C functions).
 *
 * Install param:
 *   ADMIN (20) - sole account that may change settings via Invoke
 *
 * Admin Invokes (one param per Invoke):
 *   SUBPRICE  (8)  uint64 BE drops, > 0
 *   SUBPERIOD (4)  uint32 BE seconds, > 0
 *   SUBSPLIT  (2)  uint16 BE percent 0..100 (treasury share of each SUB)
 *   AUCCAP    (2)  uint16 BE max active auctions per seller, >= 1
 *   TREASURY  (20) account that receives the split (must not be host)
 *   FEE       (2)  uint16 BE basis points 0..5000 (Finalise seller fee; shared ns)
 *   GRANT     (20) seller account - extend one SUBPERIOD (same as paid)
 *   REVOKE    (20) seller account - clear seller window (NOPE if ACTIVE > 0)
 *
 * Seller Payment to host with otxn param SUB:
 *   Exact SUBPRICE XAH; extend expiry; snapshot AUCCAP into seller state;
 *   emit SUBSPLIT% to TREASURY; remainder stays on host.
 * Payment without SUB: accept (Bids / other hooks).
 *
 * Seller foreign namespace = account ID (20) zero-padded to 32.
 * Individual keys in that namespace (replaces packed 12-byte blob):
 *   SUBEXP (6)  8 bytes uint64 BE (ledger_last_time epoch expiry)
 *   ACTIVE (6)  2 bytes uint16 BE (open auction count; Create +/-; Sub preserves)
 *   CAP    (3)  2 bytes uint16 BE (AUCCAP snapshot at SUB/GRANT time)
 *
 * Fail closed: SUB / GRANT rollback until SUBPRICE, SUBPERIOD, SUBSPLIT,
 * AUCCAP, and TREASURY are all set.
 * Treasury emit fail -> rollback whole SUB.
 */
#define HAS_CALLBACK
#include "hookapi.h"

/* KVT #10. Published Sub definition default ADMIN
 * (HookHash F825F314...A8606363 parameter ADMIN).
 * raMjZ7ayJ3txQY75vQWr8RTzErAcUD3gee
 * hook_param returns these bytes when the installer does not override.
 * An override to any other account still passes. */
static const uint8_t BAKED_ADMIN[20] = {
    0x3AU, 0xC4U, 0x79U, 0xABU, 0x56U, 0x17U, 0x0DU, 0x79U, 0x18U, 0x76U,
    0x60U, 0x22U, 0xEFU, 0x3CU, 0x02U, 0x3BU, 0x61U, 0xAAU, 0xD4U, 0xCAU
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
/* use stock DONE from macro.h */

#define KEY_SUBEXP       "SUBEXP"
#define KEY_SUBEXP_LEN   6
#define KEY_ACTIVE       "ACTIVE"
#define KEY_ACTIVE_LEN   6
#define KEY_CAP          "CAP"
#define KEY_CAP_LEN      3
#define SUBPERIOD_MIN_S  60U
#define SUBPERIOD_MAX_S  (86400U * 366U)
#define MAX_SUBPRICE_DROPS 1000000000000ULL /* 1M XAH */

#define KEY_SUBPRICE  "SUBPRICE"
#define KEY_SUBPERIOD "SUBPERIOD"
#define KEY_SUBSPLIT  "SUBSPLIT"
#define KEY_AUCCAP    "AUCCAP"
#define KEY_TREASURY  "TREASURY"
#define KEY_FEE       "FEE"


/* Payment emit: no tags; Memo Note = "Treasury split". */
#define MEMO_NOTE_LEN 26U
#ifdef HAS_CALLBACK
#define PREPARE_PAYMENT_NOTAG_SIZE (260U + MEMO_NOTE_LEN)
#else
#define PREPARE_PAYMENT_NOTAG_SIZE (238U + MEMO_NOTE_LEN)
#endif
#define PREPARE_PAYMENT_NOTAG(buf_out_master, drops_amount_raw, to_address, sizeout)\
{\
    uint8_t* buf_out = buf_out_master;\
    uint8_t acc[20];\
    uint64_t drops_amount = (drops_amount_raw);\
    uint32_t cls = (uint32_t)ledger_seq();\
    hook_account(SBUF(acc));\
    _01_02_ENCODE_TT                   (buf_out, ttPAYMENT                      );\
    _02_02_ENCODE_FLAGS                (buf_out, tfCANONICAL                    );\
    _02_04_ENCODE_SEQUENCE             (buf_out, 0                              );\
    _02_26_ENCODE_FLS                  (buf_out, cls + 1                        );\
    _02_27_ENCODE_LLS                  (buf_out, cls + 5                        );\
    _06_01_ENCODE_DROPS_AMOUNT         (buf_out, drops_amount                   );\
    uint8_t* fee_ptr = buf_out;\
    _06_08_ENCODE_DROPS_FEE            (buf_out, 0                              );\
    _07_03_ENCODE_SIGNING_PUBKEY_NULL  (buf_out                                 );\
    _08_01_ENCODE_ACCOUNT_SRC          (buf_out, acc                            );\
    _08_03_ENCODE_ACCOUNT_DST          (buf_out, to_address                     );\
    /* sfMemos: [{ Memo: { MemoType: Note, MemoData: Treasury split } }] */\
    {\
        uint8_t memo[MEMO_NOTE_LEN] = {\
            0xF9U, 0xEAU,\
            0x7CU, 0x04U, 0x4EU, 0x6FU, 0x74U, 0x65U,\
            0x7DU, 0x0EU,\
            0x54U, 0x72U, 0x65U, 0x61U, 0x73U, 0x75U, 0x72U, 0x79U,\
            0x20U, 0x73U, 0x70U, 0x6CU, 0x69U, 0x74U,\
            0xE1U, 0xF1U\
        };\
        for (int mi = 0; GUARD(26), mi < (int)MEMO_NOTE_LEN; ++mi)\
            *buf_out++ = memo[mi];\
    }\
    uint32_t remaining_size = PREPARE_PAYMENT_NOTAG_SIZE - (buf_out - buf_out_master);\
    int64_t edlen = etxn_details((uint32_t)buf_out, remaining_size);\
    buf_out += edlen;\
    sizeout = (buf_out - buf_out_master);\
    int64_t fee = etxn_fee_base(buf_out_master, sizeout);\
    _06_08_ENCODE_DROPS_FEE            (fee_ptr, fee                            );\
}

int64_t cbak(uint32_t what)
{
    _g(1, 1);
    (void)what;
    DONE("Auction subscription successful");
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
                            if (rc > 32)
                                NOPE("Remit Amounts unreadable");
                            {
                                int64_t ri;
                                for (ri = 0; GUARD(33), ri < rc; ++ri)
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
                                        uint8_t currency[20];
                                        uint8_t issuer[20];
                                        int i;
                                        for (i = 0; GUARD(20), i < 20; ++i)
                                        {
                                            currency[i] = ab[8 + i];
                                            issuer[i] = ab[28 + i];
                                        }
                                        uint8_t iou_lck_key[32];
                                        uint8_t pre[40];
                                        for (i = 0; GUARD(20), i < 20; ++i)
                                        {
                                            pre[i] = currency[i];
                                            pre[20 + i] = issuer[i];
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

    /* -------- Invoke: admin settings / GRANT / REVOKE -------- */
    if (tt == ttINVOKE)
    {
        uint8_t peek[32];
        int has_admin_cmd = 0;
        if (otxn_param(SBUF(peek), "SUBPRICE", 8) >= 0
            || otxn_param(SBUF(peek), "SUBPERIOD", 9) >= 0
            || otxn_param(SBUF(peek), "SUBSPLIT", 8) >= 0
            || otxn_param(SBUF(peek), "AUCCAP", 6) >= 0
            || otxn_param(SBUF(peek), "TREASURY", 8) >= 0
            || otxn_param(SBUF(peek), "FEE", 3) >= 0
            || otxn_param(SBUF(peek), "GRANT", 5) >= 0
            || otxn_param(SBUF(peek), "REVOKE", 6) >= 0)
            has_admin_cmd = 1;

        if (!has_admin_cmd)
            DONE("Invoke passthrough");

        uint8_t admin[20];
        if (hook_param(SBUF(admin), "ADMIN", 5) != 20)
            NOPE("ADMIN install param required");
        {
            int baked = 0;
            BUFFER_EQUAL(baked, admin, BAKED_ADMIN, 20);
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
                DONE("Invoke passthrough");
        }

        uint8_t pbuf[32];

        if (otxn_param(SBUF(pbuf), "SUBPRICE", 8) >= 0)
        {
            if (otxn_param(SBUF(pbuf), "SUBPRICE", 8) != 8)
                NOPE("SUBPRICE must be 8 bytes");
            uint64_t v = UINT64_FROM_BUF(pbuf);
            if (v == 0ULL)
                NOPE("SUBPRICE must be > 0");
            if (v > MAX_SUBPRICE_DROPS)
                NOPE("SUBPRICE above maximum");
            if (state_set(pbuf, 8, "SUBPRICE", 8) != 8)
                NOPE("SUBPRICE write failed");
            DONE("SUBPRICE updated");
        }

        if (otxn_param(SBUF(pbuf), "SUBPERIOD", 9) >= 0)
        {
            if (otxn_param(SBUF(pbuf), "SUBPERIOD", 9) != 4)
                NOPE("SUBPERIOD must be 4 bytes");
            uint32_t v = (uint32_t)UINT32_FROM_BUF(pbuf);
            if (v < SUBPERIOD_MIN_S || v > SUBPERIOD_MAX_S)
                NOPE("SUBPERIOD out of range");
            if (state_set(pbuf, 4, "SUBPERIOD", 9) != 4)
                NOPE("SUBPERIOD write failed");
            DONE("SUBPERIOD updated");
        }

        if (otxn_param(SBUF(pbuf), "SUBSPLIT", 8) >= 0)
        {
            if (otxn_param(SBUF(pbuf), "SUBSPLIT", 8) != 2)
                NOPE("SUBSPLIT must be 2 bytes");
            uint16_t v = (uint16_t)(((uint16_t)pbuf[0] << 8) | pbuf[1]);
            if (v > 100U)
                NOPE("SUBSPLIT must be 0..100");
            if (state_set(pbuf, 2, "SUBSPLIT", 8) != 2)
                NOPE("SUBSPLIT write failed");
            DONE("SUBSPLIT updated");
        }

        if (otxn_param(SBUF(pbuf), "AUCCAP", 6) >= 0)
        {
            if (otxn_param(SBUF(pbuf), "AUCCAP", 6) != 2)
                NOPE("AUCCAP must be 2 bytes");
            uint16_t v = (uint16_t)(((uint16_t)pbuf[0] << 8) | pbuf[1]);
            if (v < 1U || v > 1000U)
                NOPE("AUCCAP out of range");
            if (state_set(pbuf, 2, "AUCCAP", 6) != 2)
                NOPE("AUCCAP write failed");
            DONE("AUCCAP updated");
        }

        if (otxn_param(SBUF(pbuf), "TREASURY", 8) >= 0)
        {
            if (otxn_param(SBUF(pbuf), "TREASURY", 8) != 20)
                NOPE("TREASURY must be 20 bytes");
            {
                int bad = 0;
                BUFFER_EQUAL(bad, pbuf, hook_acc, 20);
                if (bad)
                    NOPE("TREASURY must not be the host");
            }
            if (state_set(pbuf, 20, "TREASURY", 8) != 20)
                NOPE("TREASURY write failed");
            DONE("TREASURY updated");
        }


        if (otxn_param(SBUF(pbuf), "FEE", 3) >= 0)
        {
            if (otxn_param(SBUF(pbuf), "FEE", 3) != 2)
                NOPE("FEE must be 2 bytes");
            uint16_t v = (uint16_t)(((uint16_t)pbuf[0] << 8) | pbuf[1]);
            if (v > 5000U)
                NOPE("FEE above maximum");
            if (state_set(pbuf, 2, "FEE", 3) != 2)
                NOPE("FEE write failed");
            DONE("FEE updated");
        }

        if (otxn_param(SBUF(pbuf), "GRANT", 5) >= 0)
        {
            if (otxn_param(SBUF(pbuf), "GRANT", 5) != 20)
                NOPE("GRANT must be 20 bytes (seller)");

            /* settings ready */
            {
                uint8_t chk[20];
                if (state(chk, 8, "SUBPRICE", 8) != 8)
                    NOPE("Settings incomplete");
                if (state(chk, 4, "SUBPERIOD", 9) != 4)
                    NOPE("Settings incomplete");
                if (state(chk, 2, "SUBSPLIT", 8) != 2)
                    NOPE("Settings incomplete");
                if (state(chk, 2, "AUCCAP", 6) != 2)
                    NOPE("Settings incomplete");
                if (state(chk, 20, "TREASURY", 8) != 20)
                    NOPE("Settings incomplete");
            }

            uint8_t perb[4];
            uint8_t capb[2];
            if (state(perb, 4, "SUBPERIOD", 9) != 4)
                NOPE("SUBPERIOD not set");
            if (state(capb, 2, "AUCCAP", 6) != 2)
                NOPE("AUCCAP not set");
            uint32_t period = (uint32_t)UINT32_FROM_BUF(perb);
            uint16_t cap = (uint16_t)(((uint16_t)capb[0] << 8) | capb[1]);
            if (period == 0U || cap == 0U)
                NOPE("Settings incomplete");

            uint8_t seller_ns[32];
            {
                int i;
                for (i = 0; GUARD(20), i < 20; ++i)
                    seller_ns[i] = pbuf[i];
                for (i = 20; GUARD(32), i < 32; ++i)
                    seller_ns[i] = 0;
            }

            uint8_t expb[8];
            uint8_t actb[2];
            uint64_t cur = 0ULL;
            uint16_t active = 0;
            if (state_foreign(expb, 8, KEY_SUBEXP, KEY_SUBEXP_LEN,
                              seller_ns, 32, hook_acc, 20) == 8)
                cur = UINT64_FROM_BUF(expb);
            if (state_foreign(actb, 2, KEY_ACTIVE, KEY_ACTIVE_LEN,
                              seller_ns, 32, hook_acc, 20) == 2)
                active = (uint16_t)(((uint16_t)actb[0] << 8) | actb[1]);

            int64_t now = ledger_last_time();
            if (now < 0)
                NOPE("ledger time unavailable");
            uint64_t base = cur > (uint64_t)now ? cur : (uint64_t)now;
            uint64_t neu = base + (uint64_t)period;
            if (neu < base)
                NOPE("SUBEXP would overflow");

            UINT64_TO_BUF(expb, neu);
            actb[0] = (uint8_t)((active >> 8) & 0xFFU);
            actb[1] = (uint8_t)(active & 0xFFU);
            uint8_t capw[2];
            capw[0] = (uint8_t)((cap >> 8) & 0xFFU);
            capw[1] = (uint8_t)(cap & 0xFFU);
            if (state_foreign_set(expb, 8, KEY_SUBEXP, KEY_SUBEXP_LEN,
                                  seller_ns, 32, hook_acc, 20) != 8)
                NOPE("seller SUBEXP write failed");
            if (state_foreign_set(capw, 2, KEY_CAP, KEY_CAP_LEN,
                                  seller_ns, 32, hook_acc, 20) != 2)
                NOPE("seller CAP write failed");
            if (state_foreign_set(actb, 2, KEY_ACTIVE, KEY_ACTIVE_LEN,
                                  seller_ns, 32, hook_acc, 20) != 2)
                NOPE("seller ACTIVE write failed");
            DONE("GRANT applied");
        }

        if (otxn_param(SBUF(pbuf), "REVOKE", 6) >= 0)
        {
            if (otxn_param(SBUF(pbuf), "REVOKE", 6) != 20)
                NOPE("REVOKE must be 20 bytes");
            {
                uint8_t seller_ns[32];
                int i;
                for (i = 0; GUARD(20), i < 20; ++i)
                    seller_ns[i] = pbuf[i];
                for (i = 20; GUARD(32), i < 32; ++i)
                    seller_ns[i] = 0;
                /* C04: refuse REVOKE while seller has open auctions (ACTIVE > 0). */
                {
                    uint8_t actb[2];
                    if (state_foreign(actb, 2, KEY_ACTIVE, KEY_ACTIVE_LEN,
                                      seller_ns, 32, hook_acc, 20) == 2)
                    {
                        uint16_t active =
                            (uint16_t)(((uint16_t)actb[0] << 8) | actb[1]);
                        if (active > 0)
                            NOPE("ACTIVE auctions open");
                    }
                }
                if (state_foreign_set(0, 0, KEY_SUBEXP, KEY_SUBEXP_LEN,
                                      seller_ns, 32, hook_acc, 20) < 0)
                    NOPE("REVOKE clear failed");
                if (state_foreign_set(0, 0, KEY_ACTIVE, KEY_ACTIVE_LEN,
                                      seller_ns, 32, hook_acc, 20) < 0)
                    NOPE("REVOKE clear failed");
                if (state_foreign_set(0, 0, KEY_CAP, KEY_CAP_LEN,
                                      seller_ns, 32, hook_acc, 20) < 0)
                    NOPE("REVOKE clear failed");
            }
            DONE("REVOKE applied");
        }

        NOPE("Unknown admin parameter");
    }

    if (tt != ttPAYMENT)
        DONE("Non-payment passthrough");

    uint8_t dest[20];
    if (otxn_field(SBUF(dest), sfDestination) != 20)
        NOPE("missing Destination");
    {
        int to_host = 0;
        BUFFER_EQUAL(to_host, dest, hook_acc, 20);
        if (!to_host)
            DONE("Payment passthrough");
    }

    {
        uint8_t sbuf[8];
        if (otxn_param(SBUF(sbuf), "SUB", 3) < 0)
            DONE("Payment passthrough");
    }

    /* One payment is one purpose. SUB+AID would subscribe and lock the
     * same drops as a bid, so LCK overstates what the host still holds. */
    {
        uint8_t aid_peek[32];
        if (otxn_param(SBUF(aid_peek), "AID", 3) >= 0)
            NOPE("SUB and AID both set");
    }

    /* -------- SUB payment -------- */
    uint8_t treasury[20];
    {
        uint8_t chk[20];
        if (state(chk, 8, "SUBPRICE", 8) != 8)
            NOPE("Settings incomplete");
        if (state(chk, 4, "SUBPERIOD", 9) != 4)
            NOPE("Settings incomplete");
        if (state(chk, 2, "SUBSPLIT", 8) != 2)
            NOPE("Settings incomplete");
        if (state(chk, 2, "AUCCAP", 6) != 2)
            NOPE("Settings incomplete");
        if (state(treasury, 20, "TREASURY", 8) != 20)
            NOPE("Settings incomplete");
    }
    {
        int bad = 0;
        BUFFER_EQUAL(bad, treasury, hook_acc, 20);
        if (bad)
            NOPE("TREASURY must not be the host");
    }

    uint8_t amt_buf[48];
    int64_t alen = otxn_field(SBUF(amt_buf), sfAmount);
    if (alen != 8)
        NOPE("Subscription must be XAH");

    int64_t drops_i = AMOUNT_TO_DROPS(amt_buf);
    if (drops_i <= 0)
        NOPE("Subscription must be XAH");
    uint64_t drops = (uint64_t)drops_i;

    uint8_t priceb[8];
    if (state(priceb, 8, "SUBPRICE", 8) != 8)
        NOPE("SUBPRICE not set");
    uint64_t price = UINT64_FROM_BUF(priceb);
    if (price == 0ULL)
        NOPE("SUBPRICE not set");
    if (drops != price)
        NOPE("Amount must equal SUBPRICE");

    /* extend seller one period */
    {
        uint8_t perb[4];
        uint8_t capb[2];
        if (state(perb, 4, "SUBPERIOD", 9) != 4)
            NOPE("SUBPERIOD not set");
        if (state(capb, 2, "AUCCAP", 6) != 2)
            NOPE("AUCCAP not set");
        uint32_t period = (uint32_t)UINT32_FROM_BUF(perb);
        uint16_t cap = (uint16_t)(((uint16_t)capb[0] << 8) | capb[1]);
        if (period == 0U || cap == 0U)
            NOPE("Settings incomplete");

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
        uint64_t cur = 0ULL;
        uint16_t active = 0;
        if (state_foreign(expb, 8, KEY_SUBEXP, KEY_SUBEXP_LEN,
                          seller_ns, 32, hook_acc, 20) == 8)
            cur = UINT64_FROM_BUF(expb);
        if (state_foreign(actb, 2, KEY_ACTIVE, KEY_ACTIVE_LEN,
                          seller_ns, 32, hook_acc, 20) == 2)
            active = (uint16_t)(((uint16_t)actb[0] << 8) | actb[1]);

        int64_t now = ledger_last_time();
        if (now < 0)
            NOPE("ledger time unavailable");
        uint64_t base = cur > (uint64_t)now ? cur : (uint64_t)now;
        uint64_t neu = base + (uint64_t)period;
        if (neu < base)
            NOPE("SUBEXP would overflow");

        UINT64_TO_BUF(expb, neu);
        actb[0] = (uint8_t)((active >> 8) & 0xFFU);
        actb[1] = (uint8_t)(active & 0xFFU);
        uint8_t capw[2];
        capw[0] = (uint8_t)((cap >> 8) & 0xFFU);
        capw[1] = (uint8_t)(cap & 0xFFU);
        if (state_foreign_set(expb, 8, KEY_SUBEXP, KEY_SUBEXP_LEN,
                              seller_ns, 32, hook_acc, 20) != 8)
            NOPE("seller SUBEXP write failed");
        if (state_foreign_set(capw, 2, KEY_CAP, KEY_CAP_LEN,
                              seller_ns, 32, hook_acc, 20) != 2)
            NOPE("seller CAP write failed");
        if (state_foreign_set(actb, 2, KEY_ACTIVE, KEY_ACTIVE_LEN,
                              seller_ns, 32, hook_acc, 20) != 2)
            NOPE("seller ACTIVE write failed");
    }

    uint8_t splitb[2];
    if (state(splitb, 2, "SUBSPLIT", 8) != 2)
        NOPE("SUBSPLIT not set");
    uint16_t pct = (uint16_t)(((uint16_t)splitb[0] << 8) | splitb[1]);
    uint64_t split = (price * (uint64_t)pct) / 100ULL;
    if (split > 0ULL)
    {
        etxn_reserve(1);
        uint8_t emh[32];
        uint8_t txn[PREPARE_PAYMENT_NOTAG_SIZE];
        uint32_t txn_len = 0;
        PREPARE_PAYMENT_NOTAG(txn, split, treasury, txn_len);
        int64_t fee = etxn_fee_base(txn, txn_len);
        if (fee < 0)
            NOPE("Treasury split fee quote failed");
        if (emit(SBUF(emh), txn, txn_len) != 32)
            NOPE("Treasury split emit failed");
    }

    DONE("Subscription active");
}
