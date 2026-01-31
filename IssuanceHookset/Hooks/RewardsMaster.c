//**************************************************************
// Issuance Rewards Hook (IRH) - Xahau HandyHook Collection
// Author: @Handy_4ndy
//
// Description:
//   This hook enables IOU holders to claim rewards on their holdings after issuance completion.
//   Interest rate and claim intervals are configurable by admins via invoke transactions.
//   Users can claim rewards while timing and trustline constraints are enforced.
//   Integrated with issuance hook for post-completion rewards distribution.
//
// Hook Parameters:
//   'CURRENCY' (20 bytes): Currency code to be distributed as daily rewards.
//   'ADMIN' (20 bytes): Admin account ID for configuration.
//   'INT_RATE' (4 bytes): Set daily interest rate (big-endian uint32, e.g., 1000 = 10%).
//   'SET_INTERVAL' (4 bytes): Set claim interval in ledgers (big-endian uint32).
//   'SET_MAX_CLAIMS' (4 bytes): Set lifetime claim limit per user (big-endian uint32). (Optional)
//
// Admin Configuration Parameters (can be set at install or via invoke):
//   'INT_RATE' (4 bytes): Set daily interest rate (big-endian uint32, e.g., 1000 = 10%).
//   'SET_INTERVAL' (4 bytes): Set claim interval in ledgers (big-endian uint32).
//   'SET_MAX_CLAIMS' (4 bytes): Set lifetime claim limit per user (big-endian uint32).
//
// User Claim Parameters:
//   'R_CLAIM' (20 bytes): Claim daily rewards (claimant account ID).
// 
// Usage:
//   - Admin configures interest rate with 'INT_RATE', intervals, and limits at install or via invoke transactions.
   - After issuance completion, IOU holders send invoke transactions with 'R_CLAIM' to claim rewards.
   - Hook validates issuance status, timing constraints, trustlines, and calculates rewards based on holdings.
//   - User state tracked in hierarchical namespaces for unlimited scalability.
//**************************************************************

#include "hookapi.h"

// Field codes for Remit transaction Amounts array
#define sfAmountEntry ((14U << 16U) + 91U)  // 0xE0 0x5B
#define sfAmounts ((15U << 16U) + 92U)      // 0xF0 0x5C

#define FLIP_ENDIAN(n) ((uint32_t) (((n & 0xFFU) << 24U) | \
                                   ((n & 0xFF00U) << 8U) | \
                                 ((n & 0xFF0000U) >> 8U) | \
                                ((n & 0xFF000000U) >> 24U)))

#define DONE(x) accept(SBUF("IRH :: Success :: " x), __LINE__)
#define NOPE(x) rollback(SBUF("IRH :: Error :: " x), __LINE__)
#define GUARD(maxiter) _g(__LINE__, (maxiter) + 1)

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

    TRACESTR("IRH :: Issuance Rewards Hook :: Called.");

    // Only process ttINVOKE transactions (type 99)
    if (otxn_type() != 99)
        DONE("Non-INVOKE transaction passed through.");

    // Get accounts first for validation
    uint8_t hook_acc[20];
    if (hook_account(SBUF(hook_acc)) != 20)
        NOPE("Failed to get hook account.");

    uint8_t otxn_acc[20];
    if (otxn_field(SBUF(otxn_acc), sfAccount) != 20)
        NOPE("Failed to get origin account.");

    // Ignore outgoing transactions (hook account invoking itself)
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc))
        DONE("Outgoing invoke transaction passed through.");

    uint8_t currency[20];
    if(hook_param(SBUF(currency), "CURRENCY", 8) != 20)
        NOPE("Misconfigured. CURRENCY not set as Hook Parameter.");

    uint8_t invoke_acc[20];
    if(hook_param(SBUF(invoke_acc), "ADMIN", 5) != 20)
        NOPE("Misconfigured. ADMIN not set as Hook Parameter.");    

    // State keys for configuration
    uint8_t interest_rate_key[8] = "INT_RATE";
    uint8_t interval_key[8] = "CLAIM_INT";
    uint8_t max_claims_key[8] = "MAX_CLM\0";

    // Optional install-time configuration parameters
    uint8_t install_int_rate[4];
    if(hook_param(SBUF(install_int_rate), "INT_RATE", 8) == 4) {
        if(state_set(SBUF(install_int_rate), SBUF(interest_rate_key)) != 4)
            NOPE("Failed to set install-time interest rate.");
    }

    uint8_t install_interval[4];
    if(hook_param(SBUF(install_interval), "SET_INTERVAL", 12) == 4) {
        if(state_set(SBUF(install_interval), SBUF(interval_key)) != 4)
            NOPE("Failed to set install-time claim interval.");
    }

    uint8_t install_max_claims[4];
    if(hook_param(SBUF(install_max_claims), "SET_MAX_CLAIMS", 14) == 4) {
        if(state_set(SBUF(install_max_claims), SBUF(max_claims_key)) != 4)
            NOPE("Failed to set install-time max claims.");
    }

    // Check transaction type - admin configuration, admin issuance, or daily claim
    if (BUFFER_EQUAL_20(otxn_acc, invoke_acc)) {
        // ADMIN COMMANDS - from whitelisted account only
        
        // Check for configuration commands first
        uint8_t set_interest_param[4];
        if(otxn_param(SBUF(set_interest_param), "INT_RATE", 8) == 4) {
            // Set daily interest rate
            if(state_set(SBUF(set_interest_param), SBUF(interest_rate_key)) != 4)
                NOPE("Failed to set interest rate.");
            DONE("Interest rate configured successfully.");
        }

        uint8_t set_interval_param[4];
        if(otxn_param(SBUF(set_interval_param), "SET_INTERVAL", 12) == 4) {
            // Set claim interval
            if(state_set(SBUF(set_interval_param), SBUF(interval_key)) != 4)
                NOPE("Failed to set claim interval.");
            DONE("Claim interval configured successfully.");
        }

        uint8_t set_max_claims_param[4];
        if(otxn_param(SBUF(set_max_claims_param), "SET_MAX_CLAIMS", 14) == 4) {
            // Set max claims limit
            if(state_set(SBUF(set_max_claims_param), SBUF(max_claims_key)) != 4)
                NOPE("Failed to set max claims limit.");
            DONE("Max claims limit configured successfully.");
        }

        // No valid admin configuration parameters provided
        DONE("Admin configuration: No valid parameters provided.");
        
    } else {
        // Check for daily claim parameter
        uint8_t claim_param[20];
        if(otxn_param(SBUF(claim_param), "R_CLAIM", 7) == 20) {
            // DAILY CLAIM PATH - from any non-whitelisted account
            
            // Load daily interest rate configuration from state
            uint8_t interest_rate_buf[8];
            if(state(SBUF(interest_rate_buf), SBUF(interest_rate_key)) != 8)
                NOPE("INT_RATE not configured - admin must use SET_INTEREST_RATE first.");
            
            uint64_t interest_rate = UINT64_FROM_BUF(interest_rate_buf);
            if (interest_rate == 0)
                NOPE("Invalid interest rate - must be positive.");

            // Load claim interval (required configuration)
            uint8_t interval_buf[4];
            if(state(SBUF(interval_buf), SBUF(interval_key)) != 4)
                NOPE("SET_INTERVAL not configured - admin must set claim interval first.");
            
            uint32_t claim_interval = (uint32_t)((interval_buf[0] << 24) | (interval_buf[1] << 16) | 
                                               (interval_buf[2] << 8) | interval_buf[3]);

            // Load max claims limit (default unlimited if not set)
            uint32_t max_claims = 0; // Default unlimited
            uint8_t max_claims_buf[4];
            if(state(SBUF(max_claims_buf), SBUF(max_claims_key)) == 4) {
                max_claims = (uint32_t)((max_claims_buf[0] << 24) | (max_claims_buf[1] << 16) | 
                                       (max_claims_buf[2] << 8) | max_claims_buf[3]);
            }

            // CRITICAL: Check trustline exists for claimant account BEFORE any processing
            uint8_t keylet[34];
            if (util_keylet(SBUF(keylet), KEYLET_LINE, SBUF(hook_acc), SBUF(otxn_acc), SBUF(currency)) != 34)
                NOPE("Could not generate trustline keylet.");
            
            if (slot_set(SBUF(keylet), 1) != 1)
                NOPE("Claimant account does not have required trustline.");
            
            // Get user's IOU balance for calculation
            if (slot_subfield(1, sfBalance, 1) != 1)
                NOPE("Could not load trustline balance.");
            
            int64_t balance_xfl = slot_float(1);

            // Take absolute value for rewards calculation
            if (float_compare(balance_xfl, float_set(0, 0), COMPARE_LESS) == 1)
                balance_xfl = float_negate(balance_xfl);
            
            // Calculate claim amount: balance * interest_rate / 10000 (for percentage)
            int64_t rate_xfl = float_set(0, interest_rate);
            int64_t percent_xfl = float_set(0, 10000); // 100.00%
            int64_t rate_fraction = float_divide(rate_xfl, percent_xfl);
            if (rate_fraction < 0)
                NOPE("Invalid rate calculation.");
            
            int64_t claim_amount_xfl = float_multiply(balance_xfl, rate_fraction);
            if (claim_amount_xfl < 0)
                NOPE("Invalid claim amount calculation.");
            
            // Generate user-specific namespace from their account ID
            uint8_t user_namespace[32];
            // Copy user account ID to first 20 bytes
            for (int i = 0; GUARD(20), i < 20; ++i)
                user_namespace[i] = otxn_acc[i];
            // Pad remaining 12 bytes with zeros
            for (int i = 20; GUARD(32), i < 32; ++i)
                user_namespace[i] = 0;
            
            // Check for issuance participation bonus
            uint8_t ido_data_key[8] = {'I', 'D', 'O', '_', 'D', 'A', 'T', 'A'};
            uint8_t ido_user_data[16] = {0};
            int64_t ido_result = state_foreign(SBUF(ido_user_data), SBUF(ido_data_key), SBUF(user_namespace), SBUF(hook_acc));
            if (ido_result == 16) {
                uint64_t user_total_iou = UINT64_FROM_BUF(ido_user_data + 8);
                if (user_total_iou > 0) {
                    // Issuance participant bonus: recalculate with +5% interest
                    uint64_t bonus_rate = interest_rate + 500;
                    int64_t bonus_rate_xfl = float_set(0, bonus_rate);
                    int64_t bonus_rate_fraction = float_divide(bonus_rate_xfl, percent_xfl);
                    if (bonus_rate_fraction >= 0) {
                        claim_amount_xfl = float_multiply(balance_xfl, bonus_rate_fraction);
                    }
                }
            }
            
            // Simple state key for claim data
            uint8_t claim_key[32] = "CLAIM_DATA";
            // Pad remainder with zeros  
            for (int i = 10; GUARD(32), i < 32; ++i)
                claim_key[i] = 0;
            
            // Load user claim state from user-specific namespace on hook account
            uint8_t user_state[8] = {0}; // {last_claim_ledger:4, total_claims:4}
            int64_t state_result = state_foreign(SBUF(user_state), SBUF(claim_key), 
                                                SBUF(user_namespace), SBUF(hook_acc));
            
            uint32_t current_ledger = (uint32_t)ledger_seq();
            uint32_t last_claim_ledger = 0;
            uint32_t total_claims = 0;
            
            if (state_result == 8) {
                // Existing user - parse state
                last_claim_ledger = (uint32_t)((user_state[0] << 24) | (user_state[1] << 16) | 
                                              (user_state[2] << 8) | user_state[3]);
                total_claims = (uint32_t)((user_state[4] << 24) | (user_state[5] << 16) | 
                                         (user_state[6] << 8) | user_state[7]);
            }
            
            // Check timing constraint
            if (last_claim_ledger > 0) {
                uint32_t ledgers_elapsed = current_ledger - last_claim_ledger;
                if (ledgers_elapsed < claim_interval) {
                    NOPE("Too soon - wait more ledgers before next claim.");
                }
            }
            
            // Check lifetime claim limit
            if (max_claims > 0 && total_claims >= max_claims)
                NOPE("Maximum lifetime claims reached.");
            
            // Set hook account and claimant as destination
            hook_account(HOOK_ACC, 20);
            for (int i = 0; GUARD(20), i < 20; ++i)
                DEST_ACC[i] = otxn_acc[i];
            
            // Build main claim transaction
            // Build Amounts array for Remit transaction
            uint8_t* amounts_ptr = AMOUNTS_OUT;
            
            *amounts_ptr++ = 0xF0U;  // sfAmounts array start
            *amounts_ptr++ = 0x5CU;
            
            *amounts_ptr++ = 0xE0U;  // sfAmountEntry object start
            *amounts_ptr++ = 0x5BU;
            
            int32_t amount_len = float_sto(
                amounts_ptr, 49,
                currency, 20,
                HOOK_ACC, 20,
                claim_amount_xfl,
                sfAmount
            );
            
            if (amount_len < 0)
                NOPE("Failed to serialize claim amount.");
            
            amounts_ptr += amount_len;
            
            *amounts_ptr++ = 0xE1U;  // End AmountEntry
            *amounts_ptr++ = 0xF1U;  // End Amounts array
            
            int32_t amounts_len = amounts_ptr - AMOUNTS_OUT;
                
            etxn_reserve(1); // Reserve space for claim
            int32_t total_size = BASE_SIZE + amounts_len;
            
            // Encode ledger sequences
            int64_t seq = current_ledger + 1;
            txn[15] = (seq >> 24U) & 0xFFU;
            txn[16] = (seq >> 16U) & 0xFFU;
            txn[17] = (seq >>  8U) & 0xFFU;
            txn[18] = seq & 0xFFU;
            
            seq += 4;
            txn[21] = (seq >> 24U) & 0xFFU;
            txn[22] = (seq >> 16U) & 0xFFU;
            txn[23] = (seq >>  8U) & 0xFFU;
            txn[24] = seq & 0xFFU;
            
            etxn_details(EMIT_OUT, 116U);
            int64_t fee = etxn_fee_base(txn, total_size);
            {
                uint8_t *b = FEE_OUT;
                *b++ = 0b01000000 + ((fee >> 56) & 0b00111111);
                *b++ = (fee >> 48) & 0xFFU;
                *b++ = (fee >> 40) & 0xFFU;
                *b++ = (fee >> 32) & 0xFFU;
                *b++ = (fee >> 24) & 0xFFU;
                *b++ = (fee >> 16) & 0xFFU;
                *b++ = (fee >> 8) & 0xFFU;
                *b++ = (fee >> 0) & 0xFFU;
            }
            
            // Emit main claim transaction
            uint8_t claim_emithash[32]; 
            if(emit(SBUF(claim_emithash), txn, total_size) < 0)
                NOPE("Failed to emit claim transaction.");
            
            // Update user state
            uint8_t new_state[8];
            new_state[0] = (current_ledger >> 24) & 0xFF;
            new_state[1] = (current_ledger >> 16) & 0xFF;
            new_state[2] = (current_ledger >> 8) & 0xFF;
            new_state[3] = current_ledger & 0xFF;
            
            uint32_t new_total_claims = total_claims + 1;
            new_state[4] = (new_total_claims >> 24) & 0xFF;
            new_state[5] = (new_total_claims >> 16) & 0xFF;
            new_state[6] = (new_total_claims >> 8) & 0xFF;
            new_state[7] = new_total_claims & 0xFF;
            
            if(state_foreign_set(SBUF(new_state), SBUF(claim_key), 
                                 SBUF(user_namespace), SBUF(hook_acc)) != 8)
                NOPE("Failed to update user state.");
            
            // Note: User state stored in hierarchical namespace derived from account ID
            // This provides unlimited scalability without namespace congestion
            
            DONE("Tokens claimed successfully.");
            
        } else {
            // Not admin issuance and no R_CLAIM parameter - pass through
            DONE("Invoke from non-whitelisted account passed through.");
        }
    }
    
    _g(1,1);
    return 0;    
}