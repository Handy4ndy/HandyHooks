//**************************************************************
// IDO Router Hook - Xahau HandyHook Collection
// Author: @Handy_4ndy
//
// Description:
//   This hook acts as a router for IDO and rewards hooks in a hook chain.
//   It determines whether to execute the IDO hook or the rewards hook based on transaction type,
//   payment details, window status, and refund modes. Hardcoded hashes and namespace ensure
//   compatibility with specific IDO and rewards implementations.
//
// Hardcoded Configuration:
//   IDO_HOOK_HASH: Hash of the IDO hook to skip/execute.
//   REWARDS_HOOK_HASH: Hash of the rewards hook to skip/execute.
//   IDO_NAMESPACE: Namespace for IDO state data.
//
// Parameters (Previously Dynamic, Now Hardcoded):
//   IDO_HASH (32 bytes): Hash of the IDO hook (hardcoded).
//   REWARDS_HASH (32 bytes): Hash of the rewards hook (hardcoded).
//   NAMESPACE (32 bytes): Namespace for IDO data (hardcoded).
//
// Usage:
//   - Install as the first hook in a chain with IDO and rewards hooks.
//   - For outgoing transactions: Routes based on payment type (XAH skips rewards, IOU skips IDO).
//   - For incoming invokes: Allows START param (runs IDO), rewards admin params (runs rewards), or R_CLAIM (runs rewards).
//   - For incoming payments: Checks IDO window, refund mode, XAH/IOU type, and participation to decide execution.
//   - Skips hooks appropriately to ensure only relevant logic runs.
//
// Accepts:
//   - Outgoing payments and invokes.
//   - Incoming invokes with START, rewards params, or R_CLAIM.
//   - Incoming XAH payments with WP_LNK or raised funds during active IDO.
//   - Incoming IOU payments from participants during active IDO or refund mode.
//   - Incoming IOU payments for unwinding during any phase.
//
// Rejects:
//   - Incoming payments outside active window (unless refund mode).
//   - XAH deposits without WP_LNK acknowledgment or raised funds.
//   - IOU deposits without participation data.
//   - Invalid invokes without required parameters.
//**************************************************************

#include "hookapi.h"

#define UINT32_FROM_BUF(buf) \
    (((uint32_t)(buf)[0] << 24) + ((uint32_t)(buf)[1] << 16) + \
     ((uint32_t)(buf)[2] << 8) + (uint32_t)(buf)[3])

// IDO hook hash
uint8_t IDO_HOOK_HASH[32] = {0x33,0x09,0x61,0xA6,0x81,0x1A,0x03,0x13,0x1B,0x59,0x0D,0x0C,0x69,0x21,0x14,0x47,0xE7,0x8D,0xF7,0x20,0x88,0x98,0xA4,0x4F,0x8C,0xC1,0xE1,0x3C,0x62,0x9F,0x2D,0x2D};

// Rewards hook hash
uint8_t REWARDS_HOOK_HASH[32] = {0x8C,0xFC,0x9A,0xA6,0xAA,0x4A,0x85,0x8D,0xEF,0x04,0xD3,0x04,0x9D,0x4E,0x7D,0x22,0xA3,0x7F,0x96,0x8D,0x05,0x06,0x34,0x24,0x4E,0xC5,0xDA,0xCE,0xCC,0xE6,0x16,0x0D};

// namespace for the IDO hook data 
uint8_t IDO_NAMESPACE[32] = {0x51,0x6B,0xA7,0x92,0x15,0x00,0x22,0x76,0xEF,0x4C,0x38,0x1B,0x90,0x19,0x55,0xC5,0x3A,0x04,0x57,0x55,0x89,0x60,0x7E,0x7D,0xBA,0x20,0x46,0x8D,0xD3,0x43,0xDD,0x72};


#define DONE(msg)   accept(SBUF(msg), __LINE__)
#define NOPE(msg)   rollback(SBUF(msg), __LINE__)

#define SKIP()      { \
    int64_t r = hook_skip(IDO_HOOK_HASH, 32, 0); \
    if (r < 0) NOPE("Router: Skip failed"); \
}
#define SKIP_REWARDS() { \
    int64_t r = hook_skip(REWARDS_HOOK_HASH, 32, 0); \
    if (r < 0) NOPE("Router: Skip rewards failed"); \
}

#define GUARD(max)  _g(__LINE__, (max) + 1)

int64_t hook(int32_t reserved) {

    uint8_t hookacc[20];
    hook_account(SBUF(hookacc));

    uint8_t sender[20];
    if (otxn_field(SBUF(sender), sfAccount) != 20) {
        NOPE("Router: Cannot read sender account");
    }

    int outgoing = 1;
    int i;
    for (i = 0; GUARD(20), i < 20; ++i) {
        if (sender[i] != hookacc[i]) {
            outgoing = 0;
            break;
        }
    }

    int64_t ttype = otxn_type();

    if (outgoing) {
        if (ttype == 99) {
            SKIP();
            DONE("Router: Outgoing invoke → skip IDO");
        } else if (ttype == 0) {
            // For outgoing payment, check if XAH
            uint8_t amount[48];
            int64_t alen = otxn_field(SBUF(amount), sfAmount);
            if (alen == 8) {
                SKIP_REWARDS();
                DONE("Router: Outgoing XAH → run IDO");
            } else {
                SKIP();
                DONE("Router: Outgoing non-XAH → skip IDO");
            }
        }
    }

    // Invoke: allow START param or rewards admin params
    if (ttype == 99) {
        uint8_t dummy[20];
        if (otxn_param(SBUF(dummy), "START", 5) == 4) {
            SKIP_REWARDS();
            DONE("Router: START param → run IDO, skip rewards");
        }
        // Check for rewards admin params - skip IDO for these
        if (otxn_param(SBUF(dummy), "INT_RATE", 8) == 8 ||
            otxn_param(SBUF(dummy), "SET_INTERVAL", 12) == 4 ||
            otxn_param(SBUF(dummy), "SET_MAX_CLAIMS", 14) == 4) {
            SKIP();
            DONE("Router: Rewards admin params → skip IDO");
        }
        // Check for user claim param - skip IDO for this
        if (otxn_param(SBUF(dummy), "R_CLAIM", 7) == 20) {
            SKIP();
            DONE("Router: R_CLAIM param → skip IDO");
        }
        SKIP();
        NOPE("Router: Invoke without START or rewards params → invalid");
    }

    // Incoming Payment

    // Get current ledger
    int64_t current_ledger = ledger_seq();

    // Query IDO window state
    uint8_t start_key[5] = {'S', 'T', 'A', 'R', 'T'};
    uint8_t end_key[3] = {'E', 'N', 'D'};
    uint8_t start_buf[4];
    uint8_t end_buf[4];
    int64_t start_len = state_foreign(SBUF(start_buf), SBUF(start_key), SBUF(IDO_NAMESPACE), SBUF(hookacc));
    int64_t end_len = state_foreign(SBUF(end_buf), SBUF(end_key), SBUF(IDO_NAMESPACE), SBUF(hookacc));
    int window_set = (start_len == 4 && end_len == 4);
    int window_active = 0;
    if (window_set) {
        uint32_t start_ledger = UINT32_FROM_BUF(start_buf);
        uint32_t end_ledger = UINT32_FROM_BUF(end_buf);
        uint32_t curr = (uint32_t)current_ledger;
        if (curr >= start_ledger && curr < end_ledger) {
            window_active = 1;
        }
    }

    // REFUND flag (post-phase-4)
    uint8_t refund_key[6] = {'R','E','F','U','N','D'};
    uint8_t refund_flag = 0;
    int64_t refund_len = state_foreign(&refund_flag, 1, SBUF(refund_key), SBUF(IDO_NAMESPACE), SBUF(hookacc));
    int refund_mode = (refund_len == 1 && refund_flag == 1);

    // If refund mode active, only allow incoming IOU payments
    if (refund_mode) {
        if (ttype == 99) {
            NOPE("Router: Refund mode + invoke → invalid");
        } else if (ttype == 0) {
            if (outgoing) {
                NOPE("Router: Refund mode + outgoing → invalid");
            }
            uint8_t amount[48];
            int64_t alen = otxn_field(SBUF(amount), sfAmount);
            if (alen != 48) {
                NOPE("Router: Refund mode + not IOU → invalid");
            }
            SKIP_REWARDS();
            DONE("Router: Refund mode + incoming IOU → run IDO");
        }
    }

    // Skip if IDO has ended
    if (window_set && (uint32_t)current_ledger > UINT32_FROM_BUF(end_buf)) {
        SKIP();
        DONE("Router: IDO ended → skip IDO");
    }

    // If window not active and not refund mode, skip
    if (!window_active && !refund_mode) {
        SKIP();
        DONE("Router: Window not active and not refund → skip IDO");
    }

    // Detect XAH vs IOU
    uint8_t amount[48];
    int64_t alen = otxn_field(SBUF(amount), sfAmount);
    int is_xah = (alen == 8);

    if (is_xah) {
        uint8_t wp_dummy[256];
        int64_t has_wp = otxn_param(SBUF(wp_dummy), "WP_LNK", 6);

        if (has_wp >= 0) {
            SKIP_REWARDS();
            DONE("Router: XAH + WP_LNK → run IDO");
        }

        // Permissive: if raised XAH exists
        uint8_t xah_key[3] = {'X','A','H'};
        uint8_t dummy_buf[8];
        if (state(SBUF(dummy_buf), SBUF(xah_key)) == 8) {
            SKIP_REWARDS();
            DONE("Router: XAH + raised exists → run IDO");
        }

        SKIP();
        NOPE("Router: XAH without WP_LNK / no raised → invalid");
    }

    // IOU unwind
    uint8_t ns[32] = {0};
    for (i = 0; GUARD(20), i < 20; ++i) {
        ns[i] = sender[i];
    }

    uint8_t user_key[8] = {'I','D','O','_','D','A','T','A'};
    uint8_t user_data[16];
    int64_t has_part = state_foreign(SBUF(user_data), SBUF(user_key), SBUF(ns), SBUF(hookacc));

    if (has_part == 16) {
        SKIP_REWARDS();
        DONE("Router: IOU + participation → run IDO");
    }

    SKIP();
    NOPE("Router: IOU without participation → invalid");

    return 0;
}