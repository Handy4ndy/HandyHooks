//**************************************************************
// IDO Master Hook (IDOM) - Xahau HandyHook Collection
// Author: @Handy_4ndy
//
// Description:
//   This hook manages Initial DEX Offerings (IDOs) on Xahau, enabling issuers to launch token sales
//   with multiple phases, dynamic multipliers, soft caps, and refund mechanisms. Participants deposit
//   XAH during active phases to receive IOU tokens at varying rates. Post-sale, users can unwind
//   positions for refunds if soft cap is not met, or claim rewards via integrated rewards hook.
//   Whitepaper link validation ensures user acknowledgment of terms.
//
// Hook Parameters:
//   'ADMIN' (20 bytes): Admin account ID for configuration.
//   'CURRENCY' (20 bytes): Currency code for IOU tokens to be issued.
//   'INTERVAL' (4 bytes): Ledger interval per phase (big-endian uint32).
//   'SOFT_CAP' (8 bytes): Soft cap in XAH (big-endian uint64).
//   'WP_LNK' (variable): Whitepaper/documentation link for validation.
//
// Admin Configuration Parameters (via invoke):
//   'START' (4 bytes): Ledger offset to start the IDO window (big-endian uint32).
//   'WP_LNK' (variable): Whitepaper link to store in state.
//
// User Actions:
//   - Deposit XAH during active phases to receive IOU tokens.
//   - Provide 'WP_LNK' parameter matching stored link for validation.
//   - Unwind IOU tokens for XAH refunds during eligible periods.
//
// Phases:
//   Phase 1: 100x multiplier
//   Phase 2: 75x multiplier
//   Phase 3: 50x multiplier
//   Phase 4: 25x multiplier
//   Phase 5: Cooldown/unwind period
//
// Usage:
//   - Admin installs hook with parameters and invokes with 'START' to begin IDO.
//   - Users send XAH payments with 'WP_LNK' during active window.
//   - Hook issues IOU tokens via Remit transactions and tracks participation.
//   - After window ends, evaluates soft cap; enables refunds if not met.
//   - Users can unwind by sending exact IOU amount back for proportional XAH refund.
//******

#include "hookapi.h"

// Define NULL if not already defined
#ifndef NULL
#define NULL 0
#endif

// Field codes for Remit transaction Amounts array
#define sfAmountEntry ((14U << 16U) + 91U)  // 0xE0 0x5B
#define sfAmounts ((15U << 16U) + 92U)      // 0xF0 0x5C

// Utility macros
#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)
#define GUARD(maxiter) _g(__LINE__, (maxiter) + 1)

// Convert 8-byte buffer to uint64 (big-endian)
#define UINT64_FROM_BUF(buf) \
    (((uint64_t)(buf)[0] << 56) + ((uint64_t)(buf)[1] << 48) + \
     ((uint64_t)(buf)[2] << 40) + ((uint64_t)(buf)[3] << 32) + \
     ((uint64_t)(buf)[4] << 24) + ((uint64_t)(buf)[5] << 16) + \
     ((uint64_t)(buf)[6] << 8) + (uint64_t)(buf)[7])

// Base Remit transaction template (229 bytes)
// clang-format off
uint8_t txn[350] =
{
/* size,upto */
/*   3,   0 */   0x12U, 0x00U, 0x5FU,                                           /* ttREMIT */
/*   5,   3 */   0x22U, 0x80U, 0x00U, 0x00U, 0x00U,                            /* Flags */
/*   5,   8 */   0x24U, 0x00U, 0x00U, 0x00U, 0x00U,                            /* Sequence */
/*   6,  13 */   0x20U, 0x1AU, 0x00U, 0x00U, 0x00U, 0x00U,                     /* FirstLedgerSequence */
/*   6,  19 */   0x20U, 0x1BU, 0x00U, 0x00U, 0x00U, 0x00U,                     /* LastLedgerSequence */
/*   9,  25 */   0x68U, 0x40U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, /* Fee */
/*  35,  34 */   0x73U, 0x21U, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* SigningPubKey */
/*  22,  69 */   0x81U, 0x14U, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,        /* Account */
/*  22,  91 */   0x83U, 0x14U, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,        /* Destination */
/* 116, 113 */   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* EmitDetails */
                 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/*   0, 229 */   /* Amounts array appended here */
};
// clang-format on

#define BASE_SIZE 229U
#define FLS_OUT (txn + 15U)
#define LLS_OUT (txn + 21U)
#define FEE_OUT (txn + 26U)
#define HOOK_ACC (txn + 71U)
#define DEST_ACC (txn + 93U)
#define EMIT_OUT (txn + 113U)
#define AMOUNTS_OUT (txn + 229U)

int64_t hook(uint32_t reserved) {

    // TRACESTR("IDO :: Initial Dex Offering :: Called");

    // Get hook account (always needed)
    uint8_t hook_acc[20];
    if (hook_account(SBUF(hook_acc)) != 20)
        rollback(SBUF("IDO :: Error :: Failed to get hook account."), __LINE__);

    // Get transaction type immediately for fast branching
    int64_t tt = otxn_type();

    // ========================================================================
    // INVOKE PATH: Set the sale window
    // ========================================================================
    if (tt == ttINVOKE) {
        // Get originating account
        uint8_t otxn_acc[20];
        if (otxn_field(SBUF(otxn_acc), sfAccount) != 20)
            rollback(SBUF("IDO :: Error :: Failed to get origin account."), __LINE__);

        // Check authorization (load admin only when needed)
        if (!BUFFER_EQUAL_20(otxn_acc, hook_acc)) {
            uint8_t admin_acc[20];
            if (hook_param(SBUF(admin_acc), "ADMIN", 5) != 20)
                rollback(SBUF("IDO :: Error :: ADMIN parameter not set."), __LINE__);
            if (!BUFFER_EQUAL_20(otxn_acc, admin_acc))
                rollback(SBUF("IDO :: Error :: Unauthorized invoke."), __LINE__);
        }

        // Check if window already started (one-shot)
        uint8_t start_key[5] = {'S', 'T', 'A', 'R', 'T'};
        uint8_t existing_start_buf[4];
        if (state(SBUF(existing_start_buf), SBUF(start_key)) == 4) {
            uint32_t existing_start = UINT32_FROM_BUF(existing_start_buf);
            int64_t current_ledger = ledger_seq();
            if ((uint32_t)current_ledger >= existing_start)
                rollback(SBUF("IDO :: Error :: Window has already started, cannot restart."), __LINE__);
        }

        // Validate WP_LNK parameter exists
        uint8_t wp_buf[256];
        int64_t wp_len = hook_param(SBUF(wp_buf), "WP_LNK", 6);
        if (wp_len < 1)
            rollback(SBUF("IDO :: Error :: WP_LNK parameter not set."), __LINE__);

        // Validate INTERVAL parameter
        uint8_t interval_key[8] = {'I','N','T','E','R','V','A','L'};
        uint8_t interval_buf[4];
        if (hook_param(SBUF(interval_buf), SBUF(interval_key)) != 4)
            rollback(SBUF("IDO :: Error :: INTERVAL not set on install."), __LINE__);
        uint32_t interval_offset = UINT32_FROM_BUF(interval_buf);

        // Validate START parameter
        uint8_t start_buf[4];
        int64_t start_len = otxn_param(SBUF(start_buf), SBUF(start_key));
        if (start_len != 4)
            rollback(SBUF("IDO :: Error :: Invalid START parameter."), __LINE__);
        uint32_t start_offset = UINT32_FROM_BUF(start_buf);

        // Calculate window ledgers
        int64_t current_ledger = ledger_seq();
        uint32_t current_ledger_u = (uint32_t)current_ledger;
        // TRACE_num(SBUF("Current ledger at invoke = "), (uint64_t)current_ledger_u);

        uint32_t start_ledger = current_ledger_u + start_offset;
        uint32_t end_ledger = start_ledger + 5 * interval_offset;

        // TRACESTR("IDO :: Setting window");
        // TRACE_num(SBUF("START offset = "), (uint64_t)start_offset);
        // TRACE_num(SBUF("INTERVAL offset (per phase) = "), (uint64_t)interval_offset);
        // TRACE_num(SBUF("Calculated start ledger = "), (uint64_t)start_ledger);
        // TRACE_num(SBUF("Calculated end ledger = "), (uint64_t)end_ledger);

        // Store WP_LNK
        uint8_t wp_lnk_key[6] = {'W', 'P', '_', 'L', 'N', 'K'};
        if (state_set(wp_buf, wp_len, wp_lnk_key, 6) < 0)
            rollback(SBUF("IDO :: Error :: Failed to store WP_LNK in state."), __LINE__);

        // Store window ledgers
        uint8_t start_state[4];
        UINT32_TO_BUF(start_state, start_ledger);
        uint8_t end_state[4];
        UINT32_TO_BUF(end_state, end_ledger);
        uint8_t end_key[3] = {'E', 'N', 'D'};
        if (state_set(start_state, 4, start_key, 5) < 0 ||
            state_set(interval_buf, 4, interval_key, 8) < 0 ||
            state_set(end_state, 4, end_key, 3) < 0)
            rollback(SBUF("IDO :: Error :: Failed to set state."), __LINE__);

        // Store soft cap
        uint8_t soft_cap_buf[8];
        if (hook_param(SBUF(soft_cap_buf), "SOFT_CAP", 8) != 8)
            rollback(SBUF("IDO :: Error :: SOFT_CAP parameter not set."), __LINE__);
        uint8_t soft_cap_key[8] = {'S', 'O', 'F', 'T', '_', 'C', 'A', 'P'};
        if (state_set(SBUF(soft_cap_buf), soft_cap_key, 8) < 0)
            rollback(SBUF("IDO :: Error :: Failed to store SOFT_CAP in state."), __LINE__);

        accept(SBUF("IDO :: Success :: Window set."), __LINE__);
    }

    // ========================================================================
    // PAYMENT PATH: Handle deposits and unwinds
    // ========================================================================
    
    // Get origin account
    uint8_t otxn_acc[20];
    if (otxn_field(SBUF(otxn_acc), sfAccount) != 20)
        rollback(SBUF("IDO :: Error :: Failed to get origin account."), __LINE__);

    // Read amount field ONCE to determine payment type
    uint8_t amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);

    // Handle outgoing payments from hook
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc)) {
        if (amount_len == 48) {
            // Outgoing IOU payment - accept
            accept(SBUF("IDO :: Accepted :: Outgoing IOU payment."), __LINE__);
        } else if (amount_len == 8) {
            // Outgoing XAH payment - check balance protection
            // Get account balance
            uint8_t acct_kl[34];
            util_keylet(SBUF(acct_kl), KEYLET_ACCOUNT, SBUF(hook_acc), 0, 0, 0, 0);
            if (slot_set(SBUF(acct_kl), 1) != 1)
                rollback(SBUF("IDO :: Error :: Could not load account keylet."), __LINE__);
            if (slot_subfield(1, sfBalance, 1) != 1)
                rollback(SBUF("IDO :: Error :: Could not load sfBalance."), __LINE__);
            int64_t balance_xfl = slot_float(1);
            int64_t balance_drops = float_int(balance_xfl, 6, 0);

            // Get outgoing amount
            int64_t outgoing_drops = AMOUNT_TO_DROPS(amount_buffer);

            // Get locked balance
            uint8_t xah_key[3] = {'X', 'A', 'H'};
            uint8_t xah_buf[8];
            uint64_t total_xah = 0;
            if (state(SBUF(xah_buf), SBUF(xah_key)) == 8)
                total_xah = UINT64_FROM_BUF(xah_buf);

            // Check if we need to evaluate soft cap (in case no deposit/unwind triggered it)
            uint8_t start_key[5] = {'S', 'T', 'A', 'R', 'T'};
            uint8_t interval_key[8] = {'I','N','T','E','R','V','A','L'};
            uint8_t start_buf[4];
            uint8_t interval_buf[4];
            if (state(SBUF(start_buf), SBUF(start_key)) == 4 &&
                state(SBUF(interval_buf), SBUF(interval_key)) == 4) {
                uint32_t start_ledger = UINT32_FROM_BUF(start_buf);
                uint32_t interval_offset = UINT32_FROM_BUF(interval_buf);
                uint32_t phase4_end = start_ledger + (4 * interval_offset);
                int64_t current_ledger = ledger_seq();
                uint32_t current_ledger_u = (uint32_t)current_ledger;
                if (current_ledger_u >= phase4_end) {
                    uint8_t refund_key[6] = {'R', 'E', 'F', 'U', 'N', 'D'};
                    uint8_t refund_flag[1];
                    int64_t refund_check = state(SBUF(refund_flag), SBUF(refund_key));
                    if (refund_check < 0) {
                        // Evaluate soft cap
                        uint8_t soft_cap_buf[8];
                        if (hook_param(SBUF(soft_cap_buf), "SOFT_CAP", 8) == 8) {
                            uint64_t soft_cap_xah = UINT64_FROM_BUF(soft_cap_buf);
                            uint8_t total_xah_buf[8];
                            uint64_t total_xah_eval = 0;
                            if (state(SBUF(total_xah_buf), SBUF(xah_key)) == 8)
                                total_xah_eval = UINT64_FROM_BUF(total_xah_buf);
                            if (total_xah_eval >= soft_cap_xah) {
                                uint8_t refund_inactive[1] = {0};
                                state_set(SBUF(refund_inactive), SBUF(refund_key));
                                uint8_t total_raised_key[12] = {'T', 'O', 'T', 'A', 'L', '_', 'R', 'A', 'I', 'S', 'E', 'D'};
                                state_set(SBUF(total_xah_buf), SBUF(total_raised_key));
                            } else {
                                uint8_t refund_active[1] = {1};
                                state_set(SBUF(refund_active), SBUF(refund_key));
                            }
                        }
                    }
                }
            }

            uint64_t locked_drops = total_xah * 1000000ULL;

            // Check if sale is over and soft cap met (locked balance should be zero)
            uint8_t refund_key[6] = {'R', 'E', 'F', 'U', 'N', 'D'};
            uint8_t refund_flag[1];
            int64_t refund_mode = state(SBUF(refund_flag), SBUF(refund_key));
            // refund_mode == 1 && refund_flag[0] == 0 means sale successful, not refund mode
            uint8_t end_key[3] = {'E', 'N', 'D'};
            uint8_t end_buf[4];
            int sale_over = 0;
            if (state(SBUF(end_buf), SBUF(end_key)) == 4) {
                uint32_t end_ledger = UINT32_FROM_BUF(end_buf);
                int64_t current_ledger = ledger_seq();
                uint32_t current_ledger_u = (uint32_t)current_ledger;
                if (current_ledger_u >= end_ledger)
                    sale_over = 1;
            }

            if (sale_over && refund_mode == 1 && refund_flag[0] == 0) {
                // Sale successful, all funds unlocked
                locked_drops = 0;
                // Ensure XAH state is set to zero for future checks
                if (total_xah != 0) {
                    uint8_t zero_buf[8] = {0};
                    state_set(SBUF(zero_buf), SBUF(xah_key));
                }
            }

            // Check if sufficient unlocked balance
            if (balance_drops - locked_drops >= outgoing_drops) {
                accept(SBUF("IDO :: Accepted :: Outgoing XAH payment."), __LINE__);
            } else {
                rollback(SBUF("IDO :: Rejected :: Insufficient unlocked balance."), __LINE__);
            }
        } else {
            // Other amount types - accept
            accept(SBUF("IDO :: Accepted :: Outgoing payment."), __LINE__);
        }
    }

    // ========================================================================
    // IOU PAYMENT PATH: Unwind (return IOU for XAH)
    // ========================================================================
    if (amount_len == 48) {
        // TRACESTR("IDO :: Checking unwind");

        // Validate issuer (offset 28 in amount)
        if (!BUFFER_EQUAL_20(amount_buffer + 28, hook_acc))
            rollback(SBUF("IDO :: Unwind :: Wrong issuer."), __LINE__);

        // Get IOU amount
        int64_t iou_xfl = -INT64_FROM_BUF(amount_buffer);
        int64_t iou_amount = float_int(iou_xfl, 0, 1);
        // TRACEVAR(iou_amount);

        // Get user namespace
        uint8_t user_namespace[32];
        for (int i = 0; GUARD(20), i < 20; ++i)
            user_namespace[i] = otxn_acc[i];
        for (int i = 20; GUARD(32), i < 32; ++i)
            user_namespace[i] = 0;

        // Get user participation data
        uint8_t ido_data_key[8] = {'I', 'D', 'O', '_', 'D', 'A', 'T', 'A'};
        uint8_t user_data[16];
        state_foreign(SBUF(user_data), ido_data_key, 8, user_namespace, 32, hook_acc, 20);

        uint64_t user_total_xah = UINT64_FROM_BUF(user_data);
        uint64_t user_total_iou = UINT64_FROM_BUF(user_data + 8);
        // TRACEVAR(user_total_xah);
        // TRACEVAR(user_total_iou);

        // Ensure soft cap evaluation has occurred
        uint8_t start_key[5] = {'S', 'T', 'A', 'R', 'T'};
        uint8_t interval_key[8] = {'I','N','T','E','R','V','A','L'};
        uint8_t start_buf[4];
        uint8_t interval_buf[4];
        if (state(SBUF(start_buf), SBUF(start_key)) == 4 &&
            state(SBUF(interval_buf), SBUF(interval_key)) == 4) {
            uint32_t start_ledger = UINT32_FROM_BUF(start_buf);
            uint32_t interval_offset = UINT32_FROM_BUF(interval_buf);
            uint32_t phase4_end = start_ledger + (4 * interval_offset);
            int64_t current_ledger = ledger_seq();
            uint32_t current_ledger_u = (uint32_t)current_ledger;
            if (current_ledger_u >= phase4_end) {
                uint8_t refund_key[6] = {'R', 'E', 'F', 'U', 'N', 'D'};
                uint8_t refund_flag[1];
                int64_t refund_check = state(SBUF(refund_flag), SBUF(refund_key));
                if (refund_check < 0) {
                    // Evaluate soft cap
                    uint8_t soft_cap_buf[8];
                    if (hook_param(SBUF(soft_cap_buf), "SOFT_CAP", 8) == 8) {
                        uint64_t soft_cap_xah = UINT64_FROM_BUF(soft_cap_buf);
                        uint8_t xah_key[3] = {'X', 'A', 'H'};
                        uint8_t total_xah_buf[8];
                        uint64_t total_xah = 0;
                        if (state(SBUF(total_xah_buf), SBUF(xah_key)) == 8)
                            total_xah = UINT64_FROM_BUF(total_xah_buf);
                        // TRACEVAR(total_xah);
                        // TRACEVAR(soft_cap_xah);
                        if (total_xah < soft_cap_xah) {
                            uint8_t refund_active[1] = {1};
                            state_set(SBUF(refund_active), SBUF(refund_key));
                            // TRACESTR("IDO :: Soft cap NOT met. Phase 5 is now REFUND period.");
                        } else {
                            uint8_t refund_inactive[1] = {0};
                            state_set(SBUF(refund_inactive), SBUF(refund_key));
                            // TRACESTR("IDO :: Soft cap MET. Sale successful!");
                            // Preserve total raised for records (funds unlock after cooldown period)
                            uint8_t total_raised_key[12] = {'T', 'O', 'T', 'A', 'L', '_', 'R', 'A', 'I', 'S', 'E', 'D'};
                            state_set(SBUF(total_xah_buf), SBUF(total_raised_key));
                            // TRACESTR("IDO :: Soft cap met. Funds will unlock after cooldown period.");
                        }
                    }
                }
            }
        }

        // Check refund mode
        uint8_t refund_key[6] = {'R', 'E', 'F', 'U', 'N', 'D'};
        uint8_t refund_flag[1];
        int64_t refund_mode = state(SBUF(refund_flag), SBUF(refund_key));
        int is_refund_active = (refund_mode == 1 && refund_flag[0] == 1);

        if (is_refund_active) {
            // TRACESTR("IDO :: Refund mode - accepting any IOU amount for proportional refund.");
        } else {
            // Normal mode requires exact amount
            if (iou_amount != user_total_iou)
                rollback(SBUF("IDO :: Unwind :: Amount not exact to total IOU."), __LINE__);
            // Check if window has ended (successful IDO, no more unwinds)
            uint8_t end_key[3] = {'E', 'N', 'D'};
            uint8_t end_buf[4];
            if (state(SBUF(end_buf), SBUF(end_key)) == 4) {
                uint32_t end_ledger = UINT32_FROM_BUF(end_buf);
                int64_t current_ledger = ledger_seq();
                uint32_t current_ledger_u = (uint32_t)current_ledger;
                if (current_ledger_u >= end_ledger) {
                    rollback(SBUF("IDO :: Unwind :: Sale successful and cooldown period has ended, unwind's are no longer possible."), __LINE__);
                }
            }
        }

        // Build and emit XAH payment to user
        etxn_reserve(1);

        uint8_t pay_txn[PREPARE_PAYMENT_SIMPLE_SIZE];
        uint64_t xah_drops = user_total_xah * 1000000;
        PREPARE_PAYMENT_SIMPLE(pay_txn, xah_drops, otxn_acc, 0, 0);

        uint8_t emithash[32];
        if (emit(SBUF(emithash), SBUF(pay_txn)) < 0)
            rollback(SBUF("IDO :: Unwind :: Emit failed."), __LINE__);

        // Update global counters
        uint8_t exec_key[4] = {'E', 'X', 'E', 'C'};
        uint8_t xah_key[3] = {'X', 'A', 'H'};
        uint8_t iou_key[3] = {'I', 'O', 'U'};
        
        uint8_t exec_buf[8];
        if (state(SBUF(exec_buf), SBUF(exec_key)) == 8) {
            uint64_t executions = UINT64_FROM_BUF(exec_buf) - 1;
            UINT64_TO_BUF(exec_buf, executions);
            state_set(SBUF(exec_buf), SBUF(exec_key));
        }

        uint8_t xah_buf[8];
        if (state(SBUF(xah_buf), SBUF(xah_key)) == 8) {
            uint64_t total_xah = UINT64_FROM_BUF(xah_buf) - user_total_xah;
            UINT64_TO_BUF(xah_buf, total_xah);
            state_set(SBUF(xah_buf), SBUF(xah_key));
        }

        uint8_t iou_buf[8];
        if (state(SBUF(iou_buf), SBUF(iou_key)) == 8) {
            uint64_t total_iou = UINT64_FROM_BUF(iou_buf) - user_total_iou;
            UINT64_TO_BUF(iou_buf, total_iou);
            state_set(SBUF(iou_buf), SBUF(iou_key));
        }

        // Remove user data
        state_foreign_set(0, 0, ido_data_key, 8, user_namespace, 32, hook_acc, 20);

        accept(SBUF("IDO :: Unwind :: XAH returned."), __LINE__);
    }

    // ========================================================================
    // XAH PAYMENT PATH: Accept deposits during active phases
    // ========================================================================
    
    // TRACESTR("IDO :: Checking XAH deposit");

    // Validate WP_LNK parameter
    uint8_t otxn_wp_buf[256];
    int64_t otxn_wp_len = otxn_param(SBUF(otxn_wp_buf), "WP_LNK", 6);

    // Get and validate stored WP_LNK
    uint8_t wp_lnk_key[6] = {'W', 'P', '_', 'L', 'N', 'K'};
    uint8_t stored_wp_buf[256];
    int64_t stored_wp_len = state(SBUF(stored_wp_buf), SBUF(wp_lnk_key));
    if (stored_wp_len < 1)
        rollback(SBUF("IDO :: Error :: WP_LNK not found in state, awaiting issuer initialization."), __LINE__);

    // WP_LNK must match exactly
    if (otxn_wp_len != stored_wp_len)
        rollback(SBUF("IDO :: Rejected :: WP_LNK parameter does not match. Verify whitepaper link."), __LINE__);

    for (int i = 0; GUARD(256), i < otxn_wp_len; i++) {
        if (otxn_wp_buf[i] != stored_wp_buf[i])
            rollback(SBUF("IDO :: Rejected :: WP_LNK parameter does not match. Verify whitepaper link."), __LINE__);
    }
    
    // TRACESTR("IDO :: WP_LNK validated - user acknowledged documentation.");

    // Get INTERVAL offset (only read hook param once)
    uint8_t interval_key[8] = {'I','N','T','E','R','V','A','L'};
    uint8_t interval_param_buf[4];
    if (hook_param(SBUF(interval_param_buf), SBUF(interval_key)) != 4)
        rollback(SBUF("IDO :: Error :: INTERVAL not set."), __LINE__);
    uint32_t interval_offset = UINT32_FROM_BUF(interval_param_buf);

    // Read window state
    uint8_t start_key[5] = {'S', 'T', 'A', 'R', 'T'};
    uint8_t end_key[3] = {'E', 'N', 'D'};
    uint8_t start_buf[4];
    uint8_t end_buf[4];
    if (state(SBUF(start_buf), SBUF(start_key)) != 4 ||
        state(SBUF(end_buf), SBUF(end_key)) != 4)
        rollback(SBUF("IDO :: Error :: Window not set."), __LINE__);

    uint32_t start_ledger = UINT32_FROM_BUF(start_buf);
    uint32_t end_ledger = UINT32_FROM_BUF(end_buf);
    int64_t current_ledger = ledger_seq();
    uint32_t current_ledger_u = (uint32_t)current_ledger;

    // Check if we're past Phase 4 (soft cap evaluation time)
    uint32_t phase4_end = start_ledger + (4 * interval_offset);
    
    if (current_ledger_u >= phase4_end) {
        uint8_t refund_key[6] = {'R', 'E', 'F', 'U', 'N', 'D'};
        uint8_t refund_flag[1];
        int64_t refund_check = state(SBUF(refund_flag), SBUF(refund_key));
        
        // First time past Phase 4 - evaluate soft cap
        if (refund_check < 0) {
            uint8_t soft_cap_buf[8];
            if (hook_param(SBUF(soft_cap_buf), "SOFT_CAP", 8) != 8)
                rollback(SBUF("IDO :: Error :: SOFT_CAP parameter not set."), __LINE__);
            uint64_t soft_cap_xah = UINT64_FROM_BUF(soft_cap_buf);
            
            uint8_t xah_key[3] = {'X', 'A', 'H'};
            uint8_t total_xah_buf[8];
            uint64_t total_xah = 0;
            if (state(SBUF(total_xah_buf), SBUF(xah_key)) == 8)
                total_xah = UINT64_FROM_BUF(total_xah_buf);
            
            // TRACEVAR(total_xah);
            // TRACEVAR(soft_cap_xah);
            
            if (total_xah < soft_cap_xah) {
                uint8_t refund_active[1] = {1};
                state_set(SBUF(refund_active), SBUF(refund_key));
                // TRACESTR("IDO :: Soft cap NOT met. Phase 5 is now REFUND period.");
            } else {
                uint8_t refund_inactive[1] = {0};
                state_set(SBUF(refund_inactive), SBUF(refund_key));
                // TRACESTR("IDO :: Soft cap MET. Sale successful!");
                // Preserve total raised for records (funds unlock after cooldown period)
                uint8_t total_raised_key[11] = {'T', 'O', 'T', 'A', 'L', '_', 'R', 'A', 'I', 'S', 'E', 'D'};
                state_set(SBUF(total_xah_buf), SBUF(total_raised_key));
                // TRACESTR("IDO :: Soft cap met. Funds will unlock after cooldown period.");
            }
        }
    }

    // Check if window has ended
    if (current_ledger_u >= end_ledger) {
        uint8_t refund_key[6] = {'R', 'E', 'F', 'U', 'N', 'D'};
        uint8_t refund_flag[1];
        int64_t refund_mode = state(SBUF(refund_flag), SBUF(refund_key));
        
        if (refund_mode == 1 && refund_flag[0] == 1)
            rollback(SBUF("IDO :: Rejected :: Window ended. Soft cap not met. Send IOU to unwind for refund."), __LINE__);
        rollback(SBUF("IDO :: Rejected :: Window has ended."), __LINE__);
    }

    // TRACE_num(SBUF("Current ledger = "), (uint64_t)current_ledger_u);
    // TRACE_num(SBUF("Start ledger = "), (uint64_t)start_ledger);
    // TRACE_num(SBUF("End ledger = "), (uint64_t)end_ledger);

    // Extract received XAH amount (amount_buffer already read earlier)
    int64_t received_drops = AMOUNT_TO_DROPS(amount_buffer);
    int64_t received_xah = received_drops / 1000000;
    // TRACEVAR(received_drops);
    // TRACEVAR(received_xah);

    // Calculate phase and multiplier
    uint32_t elapsed = current_ledger_u - start_ledger;
    uint32_t phase = (elapsed / interval_offset) + 1;
    int64_t multiplier = 0;

    if (phase == 1) {
        multiplier = 100;
        // TRACESTR("IDO :: Phase 1 active.");
    } else if (phase == 2) {
        multiplier = 75;
        // TRACESTR("IDO :: Phase 2 active.");
    } else if (phase == 3) {
        multiplier = 50;
        // TRACESTR("IDO :: Phase 3 active.");
    } else if (phase == 4) {
        multiplier = 25;
        // TRACESTR("IDO :: Phase 4 active.");
    } else if (phase == 5) {
        // Phase 5: Unwinding only
        // TRACESTR("IDO :: Phase 5 active (unwinding only).");
        rollback(SBUF("IDO :: Rejected :: Phase 5 is unwinding only, no new deposits."), __LINE__);
    } else {
        rollback(SBUF("IDO :: Rejected :: Invalid phase."), __LINE__);
    }

    // TRACEVAR(phase);

    int64_t issued_amount = received_xah * multiplier;
    if (issued_amount == 0)
        rollback(SBUF("IDO :: Issued amount is zero."), __LINE__);
    // TRACEVAR(issued_amount);

    // Update global counters
    uint8_t exec_key[4] = {'E', 'X', 'E', 'C'};
    uint8_t xah_key[3] = {'X', 'A', 'H'};
    uint8_t iou_key[3] = {'I', 'O', 'U'};
    
    uint8_t exec_buf[8] = {0};
    uint64_t executions = 0;
    if (state(SBUF(exec_buf), SBUF(exec_key)) == 8)
        executions = UINT64_FROM_BUF(exec_buf);
    executions++;
    UINT64_TO_BUF(exec_buf, executions);
    if (state_set(SBUF(exec_buf), SBUF(exec_key)) < 0)
        rollback(SBUF("IDO :: Failed to update executions counter."), __LINE__);

    uint8_t xah_buf[8] = {0};
    uint64_t total_xah = 0;
    if (state(SBUF(xah_buf), SBUF(xah_key)) == 8)
        total_xah = UINT64_FROM_BUF(xah_buf);
    total_xah += received_xah;
    UINT64_TO_BUF(xah_buf, total_xah);
    if (state_set(SBUF(xah_buf), SBUF(xah_key)) < 0)
        rollback(SBUF("IDO :: Failed to update XAH total."), __LINE__);

    uint8_t iou_buf[8] = {0};
    uint64_t total_iou = 0;
    if (state(SBUF(iou_buf), SBUF(iou_key)) == 8)
        total_iou = UINT64_FROM_BUF(iou_buf);
    total_iou += issued_amount;
    UINT64_TO_BUF(iou_buf, total_iou);
    if (state_set(SBUF(iou_buf), SBUF(iou_key)) < 0)
        rollback(SBUF("IDO :: Failed to update IOU total."), __LINE__);

    // TRACEVAR(executions);
    // TRACEVAR(total_xah);
    // TRACEVAR(total_iou);

    // Update phase-specific counter
    uint8_t phase_key[6];
    phase_key[0] = 'P';
    phase_key[1] = 'H';
    phase_key[2] = 'A';
    phase_key[3] = 'S';
    phase_key[4] = 'E';
    phase_key[5] = '0' + (uint8_t)phase;
    
    uint8_t phase_buf[8] = {0};
    uint64_t phase_exec = 0;
    if (state(SBUF(phase_buf), phase_key, 6) == 8)
        phase_exec = UINT64_FROM_BUF(phase_buf);
    phase_exec++;
    UINT64_TO_BUF(phase_buf, phase_exec);
    if (state_set(SBUF(phase_buf), phase_key, 6) < 0)
        rollback(SBUF("IDO :: Failed to update phase executions counter."), __LINE__);
    // TRACEVAR(phase_exec);

    // Record user participation data
    uint8_t user_namespace[32];
    for (int i = 0; GUARD(20), i < 20; ++i)
        user_namespace[i] = otxn_acc[i];
    for (int i = 20; GUARD(32), i < 32; ++i)
        user_namespace[i] = 0;

    uint8_t ido_data_key[8] = {'I', 'D', 'O', '_', 'D', 'A', 'T', 'A'};
    uint8_t user_data[16] = {0};
    state_foreign(SBUF(user_data), ido_data_key, 8, user_namespace, 32, hook_acc, 20);

    uint64_t user_total_xah = UINT64_FROM_BUF(user_data);
    uint64_t user_total_iou = UINT64_FROM_BUF(user_data + 8);

    user_total_xah += received_xah;
    user_total_iou += issued_amount;

    UINT64_TO_BUF(user_data, user_total_xah);
    UINT64_TO_BUF(user_data + 8, user_total_iou);

    if (state_foreign_set(user_data, 16, ido_data_key, 8, user_namespace, 32, hook_acc, 20) < 0)
        rollback(SBUF("IDO :: Failed to update user data."), __LINE__);

    // Build Amounts array for Remit transaction
    uint8_t* amounts_ptr = AMOUNTS_OUT;
    
    *amounts_ptr++ = 0xF0U;  // sfAmounts array start
    *amounts_ptr++ = 0x5CU;
    
    *amounts_ptr++ = 0xE0U;  // sfAmountEntry object start
    *amounts_ptr++ = 0x5BU;
    
    // Load currency only when needed
    uint8_t currency[20];
    if (hook_param(SBUF(currency), "CURRENCY", 8) != 20)
        rollback(SBUF("IDO :: Error :: CURRENCY parameter not set."), __LINE__);
    
    int64_t amount_xfl = float_set(0, issued_amount);
    int32_t amount_len_remit = float_sto(
        amounts_ptr, 49,
        currency, 20,
        hook_acc, 20,
        amount_xfl,
        sfAmount
    );
    
    if (amount_len_remit < 0)
        rollback(SBUF("IDO :: Failed to serialize amount."), __LINE__);
    
    amounts_ptr += amount_len_remit;
    
    *amounts_ptr++ = 0xE1U;  // End AmountEntry
    *amounts_ptr++ = 0xF1U;  // End Amounts array
    
    int32_t amounts_len = amounts_ptr - AMOUNTS_OUT;

    // Fill transaction fields
    hook_account(HOOK_ACC, 20);
    
    for (int i = 0; GUARD(20), i < 20; ++i)
        DEST_ACC[i] = otxn_acc[i];

    // Prepare for emission
    etxn_reserve(1);
    
    int32_t total_size = BASE_SIZE + amounts_len;
    
    etxn_details(EMIT_OUT, 116U);
    
    // Encode ledger sequences
    int64_t seq = ledger_seq() + 1;
    txn[15] = (seq >> 24U) & 0xFFU;
    txn[16] = (seq >> 16U) & 0xFFU;
    txn[17] = (seq >>  8U) & 0xFFU;
    txn[18] = seq & 0xFFU;
    
    seq += 4;
    txn[21] = (seq >> 24U) & 0xFFU;
    txn[22] = (seq >> 16U) & 0xFFU;
    txn[23] = (seq >>  8U) & 0xFFU;
    txn[24] = seq & 0xFFU;
    
    // Calculate and encode fee
    int64_t fee = etxn_fee_base(txn, total_size);
    
    if (fee < 0)
        rollback(SBUF("IDO :: Fee calculation failed."), __LINE__);
    
    uint64_t fee_tmp = fee;
    uint8_t* fee_ptr = (uint8_t*)&fee;
    *fee_ptr++ = 0b01000000 + ((fee_tmp >> 56) & 0b00111111);
    *fee_ptr++ = (fee_tmp >> 48) & 0xFFU;
    *fee_ptr++ = (fee_tmp >> 40) & 0xFFU;
    *fee_ptr++ = (fee_tmp >> 32) & 0xFFU;
    *fee_ptr++ = (fee_tmp >> 24) & 0xFFU;
    *fee_ptr++ = (fee_tmp >> 16) & 0xFFU;
    *fee_ptr++ = (fee_tmp >>  8) & 0xFFU;
    *fee_ptr++ = (fee_tmp >>  0) & 0xFFU;
    
    *((uint64_t*)(txn + 26)) = fee;
    
    // Emit transaction
    uint8_t emithash[32];
    int64_t emit_result = emit(SBUF(emithash), txn, total_size);
    
    if (emit_result < 0)
        rollback(SBUF("IDO :: Emit failed."), __LINE__);

    accept(SBUF("IDO :: Accepted :: Incoming payment during active phase."), __LINE__);

    _g(1,1); // Guard
    return 0;
}
