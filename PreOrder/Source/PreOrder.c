//**************************************************************
// Pre-Order Hook (POH) - Xahau HandyHook Collection
// Author: @Handy_4ndy
//
// Description:
//   Time-boxed pre-order campaign. Admin sets unit price, minimum
//   order quantity, and duration (seconds). Users pay exact multiples
//   of the price. Each payer gets a unique foreign namespace with
//   timestamp, amount paid, and quantity. Paid XAH is locked until
//   the campaign ends. If the minimum quantity is met, the lock is
//   removed. If it is not met, funds stay locked and each user
//   invokes to reclaim a refund.
//
// Triggers:
//   ttINVOKE  - admin campaign setup, user refunds, LOCK passthrough
//   ttPAYMENT - incoming pre-order payments, outgoing lock enforcement
//
// Install Parameters:
//   'ADMIN' (20 bytes): Admin account ID.
//
// Admin Invoke Parameters (all required, one campaign at a time):
//   'PRICE' (8 bytes): Unit price in drops (big-endian uint64).
//   'MINQ'  (8 bytes): Minimum order quantity (big-endian uint64).
//   'DURA'  (8 bytes): Campaign duration in seconds (big-endian uint64).
//
// User Actions:
//   - Pay XAH in an exact multiple of PRICE while the campaign is open.
//   - After expiry, if MINQ was not reached, Invoke to reclaim funds.
//
// Accepts:
//   - Admin invoke that starts/restarts a campaign (only if ORDQ == 0).
//   - Incoming XAH that is an exact multiple of PRICE during the window.
//   - Incoming IOU (passthrough).
//   - Outgoing XAH that does not spend locked funds.
//   - User invoke refunds after a failed campaign.
//   - LOCK invoke passthrough for Set Hook Lock.
//
// Rejects:
//   - Unauthorised invokes.
//   - Payments that are not an exact multiple of PRICE.
//   - Orders after the campaign has expired.
//   - Outgoing XAH against locked funds.
//   - Refunds before expiry, after a successful campaign, or twice.
//**************************************************************

#include "hookapi.h"

#define DONE(x) accept(SBUF("POH:: Success :: " x), __LINE__)
#define NOPE(x) rollback(SBUF("POH:: Error :: " x), __LINE__)
#ifndef GUARD
#define GUARD(maxiter) _g(__LINE__, (maxiter) + 1)
#endif

#define STAT_ACTIVE  0
#define STAT_SUCCESS 1
#define STAT_REFUND  2

#define STORE_U64(key, keylen, value)                                          \
    {                                                                          \
        uint8_t _sb[8];                                                        \
        UINT64_TO_BUF(_sb, (value));                                           \
        if (state_set(SBUF(_sb), (key), (keylen)) < 0)                         \
            NOPE("Failed to write state.");                                    \
    }

#define LOAD_U64(dst, key, keylen)                                             \
    {                                                                          \
        uint8_t _lb[8];                                                        \
        (dst) = 0;                                                             \
        if (state(SBUF(_lb), (key), (keylen)) == 8)                            \
            (dst) = UINT64_FROM_BUF(_lb);                                      \
    }

#define USER_NS(ns, acc)                                                       \
    {                                                                          \
        uint8_t *_ns = (ns);                                                   \
        uint8_t *_acc = (acc);                                                 \
        *(uint64_t *)(_ns + 0) = *(uint64_t *)(_acc + 0);                      \
        *(uint64_t *)(_ns + 8) = *(uint64_t *)(_acc + 8);                      \
        *(uint32_t *)(_ns + 16) = *(uint32_t *)(_acc + 16);                    \
        *(uint64_t *)(_ns + 20) = 0;                                           \
        *(uint32_t *)(_ns + 28) = 0;                                           \
    }

#define EVAL_CAMPAIGN(now, stat)                                               \
    {                                                                          \
        uint8_t _sk[4] = {'S', 'T', 'A', 'T'};                                 \
        uint8_t _sv[1];                                                        \
        (stat) = STAT_ACTIVE;                                                  \
        if (state(SBUF(_sv), SBUF(_sk)) == 1)                                  \
            (stat) = _sv[0];                                                   \
        if ((stat) == STAT_ACTIVE)                                             \
        {                                                                      \
            uint8_t _ek[3] = {'E', 'N', 'D'};                                  \
            uint8_t _eb[8];                                                    \
            if (state(SBUF(_eb), SBUF(_ek)) == 8)                              \
            {                                                                  \
                uint64_t _end = UINT64_FROM_BUF(_eb);                          \
                if ((now) >= _end)                                             \
                {                                                              \
                    uint64_t _ordq = 0;                                        \
                    uint64_t _minq = 0;                                        \
                    uint8_t _ok[4] = {'O', 'R', 'D', 'Q'};                     \
                    uint8_t _mk[4] = {'M', 'I', 'N', 'Q'};                     \
                    LOAD_U64(_ordq, _ok, 4);                                   \
                    LOAD_U64(_minq, _mk, 4);                                   \
                    if (_ordq >= _minq)                                        \
                    {                                                          \
                        (stat) = STAT_SUCCESS;                                 \
                        uint8_t _lk[6] = {'L', 'O', 'C', 'K', 'E', 'D'};       \
                        STORE_U64(_lk, 6, 0);                                  \
                    }                                                          \
                    else                                                       \
                    {                                                          \
                        (stat) = STAT_REFUND;                                  \
                    }                                                          \
                    _sv[0] = (stat);                                           \
                    if (state_set(SBUF(_sv), SBUF(_sk)) < 0)                   \
                        NOPE("Failed to store campaign status.");              \
                }                                                              \
            }                                                                  \
        }                                                                      \
    }

int64_t hook(uint32_t reserved)
{
    TRACESTR("POH :: Pre-Order Hook :: Called");

    uint8_t hook_acc[20];
    if (hook_account(SBUF(hook_acc)) != 20)
        NOPE("Failed to get hook account.");

    uint8_t otxn_acc[20];
    if (otxn_field(SBUF(otxn_acc), sfAccount) != 20)
        NOPE("Failed to get origin account.");

    int64_t tt = otxn_type();
    uint64_t now = (uint64_t)ledger_last_time();

    uint8_t price_key[5] = {'P', 'R', 'I', 'C', 'E'};
    uint8_t minq_key[4] = {'M', 'I', 'N', 'Q'};
    uint8_t dura_key[4] = {'D', 'U', 'R', 'A'};
    uint8_t start_key[5] = {'S', 'T', 'A', 'R', 'T'};
    uint8_t end_key[3] = {'E', 'N', 'D'};
    uint8_t locked_key[6] = {'L', 'O', 'C', 'K', 'E', 'D'};
    uint8_t ordq_key[4] = {'O', 'R', 'D', 'Q'};
    uint8_t rcnt_key[4] = {'R', 'C', 'N', 'T'};
    uint8_t key_ts[2] = {'T', 'S'};
    uint8_t key_amt[3] = {'A', 'M', 'T'};
    uint8_t key_qty[3] = {'Q', 'T', 'Y'};
    uint8_t key_rfnd[4] = {'R', 'F', 'N', 'D'};

    // ========================================================================
    // INVOKE: admin setup, user refund, LOCK passthrough
    // ========================================================================
    if (tt == ttINVOKE)
    {
        uint8_t lock_param[1];
        if (otxn_param(SBUF(lock_param), "LOCK", 4) == 1)
            DONE("LOCK parameter passed to Set Hook Lock.");

        uint8_t admin_acc[20];
        if (hook_param(SBUF(admin_acc), "ADMIN", 5) != 20)
            NOPE("ADMIN parameter not set at install.");

        if (BUFFER_EQUAL_20(otxn_acc, admin_acc))
        {
            uint8_t price_buf[8];
            uint8_t minq_buf[8];
            uint8_t dura_buf[8];
            if (otxn_param(SBUF(price_buf), SBUF(price_key)) != 8)
                NOPE("PRICE must be 8 bytes (drops).");
            if (otxn_param(SBUF(minq_buf), SBUF(minq_key)) != 8)
                NOPE("MINQ must be 8 bytes.");
            if (otxn_param(SBUF(dura_buf), SBUF(dura_key)) != 8)
                NOPE("DURA must be 8 bytes (seconds).");

            uint64_t price = UINT64_FROM_BUF(price_buf);
            uint64_t minq = UINT64_FROM_BUF(minq_buf);
            uint64_t dura = UINT64_FROM_BUF(dura_buf);
            if (price == 0)
                NOPE("PRICE must be greater than zero.");
            if (minq == 0)
                NOPE("MINQ must be greater than zero.");
            if (dura == 0)
                NOPE("DURA must be greater than zero.");

            uint64_t ordq = 0;
            LOAD_U64(ordq, ordq_key, 4);
            if (ordq != 0)
                NOPE("Campaign already has orders and cannot be reset.");

            uint64_t end_ts = now + dura;
            if (end_ts < now)
                NOPE("DURA overflow.");

            if (state_set(SBUF(price_buf), SBUF(price_key)) < 0 ||
                state_set(SBUF(minq_buf), SBUF(minq_key)) < 0 ||
                state_set(SBUF(dura_buf), SBUF(dura_key)) < 0)
                NOPE("Failed to store campaign parameters.");

            STORE_U64(start_key, 5, now);
            STORE_U64(end_key, 3, end_ts);
            STORE_U64(locked_key, 6, 0);
            STORE_U64(ordq_key, 4, 0);
            STORE_U64(rcnt_key, 4, 0);

            uint8_t stat_key[4] = {'S', 'T', 'A', 'T'};
            uint8_t stat_buf[1] = {STAT_ACTIVE};
            if (state_set(SBUF(stat_buf), SBUF(stat_key)) < 0)
                NOPE("Failed to store campaign status.");

            DONE("Campaign configured.");
        }

        uint8_t price_chk[8];
        if (state(SBUF(price_chk), SBUF(price_key)) != 8)
            NOPE("Campaign is not configured.");

        uint8_t stat = STAT_ACTIVE;
        EVAL_CAMPAIGN(now, stat);
        if (stat != STAT_REFUND)
            NOPE("Refunds are not available.");

        uint8_t user_ns[32];
        USER_NS(user_ns, otxn_acc);

        uint8_t amt_buf[8];
        uint8_t qty_buf[8];
        if (state_foreign(SBUF(amt_buf), SBUF(key_amt), SBUF(user_ns), SBUF(hook_acc)) != 8)
        {
            uint8_t rfnd_chk[24];
            if (state_foreign(SBUF(rfnd_chk), SBUF(key_rfnd), SBUF(user_ns), SBUF(hook_acc)) > 0)
                NOPE("Order already refunded.");
            NOPE("No order to refund.");
        }

        uint64_t amt = UINT64_FROM_BUF(amt_buf);
        uint64_t qty = 0;
        if (state_foreign(SBUF(qty_buf), SBUF(key_qty), SBUF(user_ns), SBUF(hook_acc)) == 8)
            qty = UINT64_FROM_BUF(qty_buf);
        if (amt == 0)
            NOPE("Nothing to refund.");

        etxn_reserve(1);
        uint8_t txn[PREPARE_PAYMENT_SIMPLE_SIZE];
        PREPARE_PAYMENT_SIMPLE(txn, amt, otxn_acc, 0, 0);
        uint8_t emithash[32];
        if (emit(SBUF(emithash), SBUF(txn)) != 32)
            NOPE("Failed to emit refund.");

        uint8_t empty[1] = {0};
        if (state_foreign_set(empty, 0, SBUF(key_ts), SBUF(user_ns), SBUF(hook_acc)) < 0 ||
            state_foreign_set(empty, 0, SBUF(key_amt), SBUF(user_ns), SBUF(hook_acc)) < 0 ||
            state_foreign_set(empty, 0, SBUF(key_qty), SBUF(user_ns), SBUF(hook_acc)) < 0)
            NOPE("Failed to clear user order state.");

        uint8_t rfnd_buf[24];
        UINT64_TO_BUF(rfnd_buf, amt);
        UINT64_TO_BUF(rfnd_buf + 8, qty);
        UINT64_TO_BUF(rfnd_buf + 16, now);
        if (state_foreign_set(SBUF(rfnd_buf), SBUF(key_rfnd), SBUF(user_ns), SBUF(hook_acc)) < 0)
            NOPE("Failed to store refunded state.");

        uint64_t locked = 0;
        LOAD_U64(locked, locked_key, 6);
        if (locked < amt)
            locked = 0;
        else
            locked -= amt;
        STORE_U64(locked_key, 6, locked);

        uint64_t ordq = 0;
        LOAD_U64(ordq, ordq_key, 4);
        if (ordq < qty)
            ordq = 0;
        else
            ordq -= qty;
        STORE_U64(ordq_key, 4, ordq);

        uint64_t rcnt = 0;
        LOAD_U64(rcnt, rcnt_key, 4);
        if (rcnt + 1 < rcnt)
            NOPE("Refund counter overflow.");
        STORE_U64(rcnt_key, 4, rcnt + 1);

        DONE("Refund emitted.");
    }

    // ========================================================================
    // PAYMENT: incoming orders and outgoing lock
    // ========================================================================
    if (tt == ttPAYMENT)
    {
        uint8_t amount_buffer[48];
        int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);

        if (BUFFER_EQUAL_20(hook_acc, otxn_acc))
        {
            if (amount_len != 8)
                DONE("Outgoing non-XAH payment.");

            uint8_t stat = STAT_ACTIVE;
            EVAL_CAMPAIGN(now, stat);
            uint64_t locked_drops = 0;
            if (stat != STAT_SUCCESS)
                LOAD_U64(locked_drops, locked_key, 6);

            uint8_t acct_kl[34];
            util_keylet(SBUF(acct_kl), KEYLET_ACCOUNT, SBUF(hook_acc), 0, 0, 0, 0);
            if (slot_set(SBUF(acct_kl), 1) != 1)
                NOPE("Could not load account keylet.");
            if (slot_subfield(1, sfBalance, 1) != 1)
                NOPE("Could not load sfBalance.");

            int64_t balance_xfl = slot_float(1);
            int64_t balance_drops = float_int(balance_xfl, 6, 0);
            int64_t outgoing_drops = AMOUNT_TO_DROPS(amount_buffer);

            if (balance_drops - (int64_t)locked_drops >= outgoing_drops)
                DONE("Outgoing XAH payment.");

            NOPE("Insufficient unlocked balance.");
        }

        if (amount_len != 8)
            DONE("Incoming IOU accepted.");

        uint8_t price_buf[8];
        uint8_t end_buf[8];
        if (state(SBUF(price_buf), SBUF(price_key)) != 8)
            NOPE("Campaign is not configured.");
        if (state(SBUF(end_buf), SBUF(end_key)) != 8)
            NOPE("Campaign end time is not set.");

        uint8_t stat = STAT_ACTIVE;
        EVAL_CAMPAIGN(now, stat);
        if (stat != STAT_ACTIVE)
            NOPE("Campaign is closed. Orders are not accepted.");

        uint64_t end_ts = UINT64_FROM_BUF(end_buf);
        if (now >= end_ts)
            NOPE("Campaign duration has expired.");

        uint64_t price = UINT64_FROM_BUF(price_buf);
        if (price == 0)
            NOPE("PRICE is invalid.");

        int64_t otxn_drops_i = AMOUNT_TO_DROPS(amount_buffer);
        if (otxn_drops_i <= 0)
            NOPE("Invalid XAH amount.");
        uint64_t otxn_drops = (uint64_t)otxn_drops_i;

        if (otxn_drops < price)
            NOPE("Payment is less than the unit price.");
        if (otxn_drops % price != 0)
            NOPE("Payment must be an exact multiple of PRICE.");

        uint64_t qty = otxn_drops / price;
        if (qty == 0)
            NOPE("Payment is less than the unit price.");

        uint8_t user_ns[32];
        USER_NS(user_ns, otxn_acc);

        uint8_t ts_buf[8];
        uint8_t amt_buf[8];
        uint8_t qty_buf[8];
        uint64_t user_amt = 0;
        uint64_t user_qty = 0;
        uint64_t user_ts = now;

        if (state_foreign(SBUF(ts_buf), SBUF(key_ts), SBUF(user_ns), SBUF(hook_acc)) == 8)
            user_ts = UINT64_FROM_BUF(ts_buf);
        if (state_foreign(SBUF(amt_buf), SBUF(key_amt), SBUF(user_ns), SBUF(hook_acc)) == 8)
            user_amt = UINT64_FROM_BUF(amt_buf);
        if (state_foreign(SBUF(qty_buf), SBUF(key_qty), SBUF(user_ns), SBUF(hook_acc)) == 8)
            user_qty = UINT64_FROM_BUF(qty_buf);

        if (user_amt + otxn_drops < user_amt)
            NOPE("User amount overflow.");
        if (user_qty + qty < user_qty)
            NOPE("User quantity overflow.");

        user_amt += otxn_drops;
        user_qty += qty;

        UINT64_TO_BUF(ts_buf, user_ts);
        UINT64_TO_BUF(amt_buf, user_amt);
        UINT64_TO_BUF(qty_buf, user_qty);

        if (state_foreign_set(SBUF(ts_buf), SBUF(key_ts), SBUF(user_ns), SBUF(hook_acc)) < 0 ||
            state_foreign_set(SBUF(amt_buf), SBUF(key_amt), SBUF(user_ns), SBUF(hook_acc)) < 0 ||
            state_foreign_set(SBUF(qty_buf), SBUF(key_qty), SBUF(user_ns), SBUF(hook_acc)) < 0)
            NOPE("Failed to store user order.");

        uint64_t locked = 0;
        uint64_t ordq = 0;
        LOAD_U64(locked, locked_key, 6);
        LOAD_U64(ordq, ordq_key, 4);
        if (locked + otxn_drops < locked)
            NOPE("Locked pool overflow.");
        if (ordq + qty < ordq)
            NOPE("Order quantity overflow.");

        STORE_U64(locked_key, 6, locked + otxn_drops);
        STORE_U64(ordq_key, 4, ordq + qty);

        DONE("Pre-order recorded.");
    }

    DONE("Transaction passed through.");

    _g(1, 1);
    return 0;
}
