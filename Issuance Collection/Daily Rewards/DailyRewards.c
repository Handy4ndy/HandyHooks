//**************************************************************
// Daily Rewards Hook (DRH) - Xahau HandyHook Collection
// Author: @Handy_4ndy
//
// Description:
//   This hook enables automated daily token rewards through invoke
//   transactions with configurable timing and limits. Users can claim
//   daily rewards while admins configure the system parameters.
//
// Hook Parameters:
//   'IOU' (20 bytes): Currency code to be distributed as daily rewards.
//   'W_ACC' (20 bytes): Whitelisted admin account ID for configuration.
//
// Admin Configuration Parameters:
//   'SET_DAILY' (8 bytes): Set daily claim amount (big-endian uint64).
//   'SET_INTERVAL' (4 bytes): Set claim interval in ledgers (big-endian uint32).
//   'SET_MAX_CLAIMS' (4 bytes): Set lifetime claim limit per user (big-endian uint32).
//   'SET_TREASURY' (20 bytes): Set treasury account (reserved for future use).
//
// User Claim Parameters:
//   'R_CLAIM' (20 bytes): Claim daily rewards (claimant account ID).
// 
// Usage:
//   - Admin configures daily amount, intervals, and limits via invoke transactions.
//   - Users send invoke transactions with 'R_CLAIM' parameter to claim rewards.
//   - Hook validates timing constraints and trustlines before issuing rewards.
//   - User state tracked in hierarchical namespaces for unlimited scalability.
//
// Service Fee:
//   - A service fee of 0.05 XAH is charged per successful claim, sent to a predefined account.
//**************************************************************

#include "hookapi.h"

#define FLIP_ENDIAN(n) ((uint32_t) (((n & 0xFFU) << 24U) | \
                                   ((n & 0xFF00U) << 8U) | \
                                 ((n & 0xFF0000U) >> 8U) | \
                                ((n & 0xFF000000U) >> 24U)))

#define DONE(x) accept(SBUF("DRH :: Success :: " x), __LINE__)
#define NOPE(x) rollback(SBUF("DRH :: Error :: " x), __LINE__)
#define GUARD(maxiter) _g(__LINE__, (maxiter) + 1)

#define UINT64_FROM_BUF(buf) \
    (((uint64_t)(buf)[0] << 56) + ((uint64_t)(buf)[1] << 48) + \
     ((uint64_t)(buf)[2] << 40) + ((uint64_t)(buf)[3] << 32) + \
     ((uint64_t)(buf)[4] << 24) + ((uint64_t)(buf)[5] << 16) + \
     ((uint64_t)(buf)[6] << 8) + (uint64_t)(buf)[7])

     
     uint8_t txn[278] =
     {
         /* size,upto */
    /* 3,  0, tt = Payment           */   0x12U, 0x00U, 0x00U,
    /* 5,  3, flags                  */   0x22U, 0x00U, 0x00U, 0x00U, 0x00U,
    /* 5,  8, sequence               */   0x24U, 0x00U, 0x00U, 0x00U, 0x00U,
    /* 6,  13, firstledgersequence   */   0x20U, 0x1AU, 0x00U, 0x00U, 0x00U, 0x00U,
    /* 6,  19, lastledgersequence    */   0x20U, 0x1BU, 0x00U, 0x00U, 0x00U, 0x00U,
    /* 49, 25  amount               */    0x61U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U,                         
                                          0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U,
                                          0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U,
                                          0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U,
                                          0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U,
                                          0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99,
    /* 9,   74,  fee                 */   0x68U, 0x40U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    /* 35,  83, signingpubkey        */   0x73U, 0x21U, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    /* 22,  118, account             */   0x81U, 0x14U, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    /* 22,  140, destination         */   0x83U, 0x14U, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    /* 116, 162  emit details        */ 
    /* 0,   278                      */ 
};

// TX BUILDER
#define FLS_OUT    (txn + 15U) 
#define LLS_OUT    (txn + 21U) 
#define FEE_OUT    (txn + 75U) 
#define AMOUNT_OUT (txn + 25U)
#define HOOK_ACC   (txn + 120U)
#define DEST_ACC   (txn + 142U)
#define EMIT_OUT   (txn + 162U) 

static uint8_t service_fee_acc[20] = {0xCCU, 0x41U, 0x96U, 0xC1U, 0xF2U, 0x34U, 0xDBU, 0xAAU, 0x06U, 0x13U, 0x0FU, 0xAAU, 0xF5U, 0xD2U, 0x8CU, 0x53U, 0x77U, 0xA6U, 0xFBU, 0xCAU};
#define SERVICE_FEE_DROPS 50000

int64_t hook(uint32_t reserved) {

    TRACESTR("DRH :: Daily Rewards Hook :: Called.");

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
    if(hook_param(SBUF(currency), "IOU", 3) != 20)
        NOPE("Misconfigured. Currency not set as Hook Parameter.");

    uint8_t invoke_acc[20];
    if(hook_param(SBUF(invoke_acc), "W_ACC", 5) != 20)
        NOPE("Misconfigured. Whitelist account not set as Hook Parameter.");    

    // State keys for configuration
    uint8_t treasury_key[8] = "TREASURY";
    uint8_t daily_amt_key[8] = "DAILY_AM";
    uint8_t interval_key[8] = "INTERVAL";
    uint8_t max_claims_key[8] = "MAX_CLM\0";

    // Check transaction type - admin configuration, admin issuance, or daily claim
    if (BUFFER_EQUAL_20(otxn_acc, invoke_acc)) {
        // ADMIN COMMANDS - from whitelisted account only
        
        // Check for configuration commands first
        uint8_t set_treasury_param[20];
        if(otxn_param(SBUF(set_treasury_param), "SET_TREASURY", 12) == 20) {
            // Set treasury account
            if(state_set(SBUF(set_treasury_param), SBUF(treasury_key)) != 20)
                NOPE("Failed to set treasury account.");
            DONE("Treasury account configured successfully.");
        }

        uint8_t set_daily_param[8];
        if(otxn_param(SBUF(set_daily_param), "SET_DAILY", 9) == 8) {
            // Set daily claim amount
            if(state_set(SBUF(set_daily_param), SBUF(daily_amt_key)) != 8)
                NOPE("Failed to set daily amount.");
            DONE("Daily claim amount configured successfully.");
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
            
            // Load daily claim configuration from state
            uint8_t daily_amt_buf[8];
            if(state(SBUF(daily_amt_buf), SBUF(daily_amt_key)) != 8)
                NOPE("DAILY_AMT not configured - admin must use SET_DAILY first.");
            
            uint64_t daily_amount = UINT64_FROM_BUF(daily_amt_buf);
            if (daily_amount == 0)
                NOPE("Invalid daily amount - must be positive.");

            // Load claim interval (default 24 hours in ledgers)
            uint32_t claim_interval = 17280; // Default 24 hours
            uint8_t interval_buf[4];
            if(state(SBUF(interval_buf), SBUF(interval_key)) == 4) {
                claim_interval = (uint32_t)((interval_buf[0] << 24) | (interval_buf[1] << 16) | 
                                           (interval_buf[2] << 8) | interval_buf[3]);
            }

            // Load max claims limit (default unlimited)
            uint32_t max_claims = 0; // Default unlimited
            uint8_t max_claims_buf[4];
            if(state(SBUF(max_claims_buf), SBUF(max_claims_key)) == 4) {
                max_claims = (uint32_t)((max_claims_buf[0] << 24) | (max_claims_buf[1] << 16) | 
                                       (max_claims_buf[2] << 8) | max_claims_buf[3]);
            }

            // CRITICAL: Check trustline exists for claimant account BEFORE any processing
            // This prevents treasury from getting tokens when claims fail due to no trustline
            uint8_t keylet[34];
            if (util_keylet(SBUF(keylet), KEYLET_LINE, SBUF(hook_acc), SBUF(otxn_acc), SBUF(currency)) != 34)
                NOPE("Could not generate trustline keylet.");
            
            if (slot_set(SBUF(keylet), 1) != 1)
                NOPE("Claimant account does not have required trustline.");
            
            // Generate user-specific namespace from their account ID
            uint8_t user_namespace[32];
            // Copy user account ID to first 20 bytes
            for (int i = 0; GUARD(20), i < 20; ++i)
                user_namespace[i] = otxn_acc[i];
            // Pad remaining 12 bytes with zeros
            for (int i = 20; GUARD(32), i < 32; ++i)
                user_namespace[i] = 0;
            
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
            
            // Convert daily amount to XFL
            int64_t daily_amount_xfl = float_set(0, daily_amount);
            
            // Set hook account and claimant as destination
            hook_account(HOOK_ACC, 20);
            for (int i = 0; GUARD(20), i < 20; ++i)
                DEST_ACC[i] = otxn_acc[i];
            
            // Build main claim transaction
            if(float_sto(AMOUNT_OUT, 49, currency, 20, HOOK_ACC, 20, daily_amount_xfl, sfAmount) < 0) 
                NOPE("Failed to serialize claim amount.");
                
            etxn_reserve(2); // Reserve space for claim + service fee
            uint32_t fls = current_ledger + 1;
            *((uint32_t *)(FLS_OUT)) = FLIP_ENDIAN(fls);
            uint32_t lls = fls + 4;
            *((uint32_t *)(LLS_OUT)) = FLIP_ENDIAN(lls);
            etxn_details(EMIT_OUT, 116U);
            {
                int64_t fee = etxn_fee_base(SBUF(txn));
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
            if(emit(SBUF(claim_emithash), SBUF(txn)) != 32)
                NOPE("Failed to emit claim transaction.");
                
            // Prepare and emit service fee payment
            uint8_t fee_txn[PREPARE_PAYMENT_SIMPLE_SIZE];
            PREPARE_PAYMENT_SIMPLE(fee_txn, SERVICE_FEE_DROPS, service_fee_acc, 0, 0);
            
            uint8_t fee_emithash[32];
            if (emit(SBUF(fee_emithash), SBUF(fee_txn)) != 32)
                TRACESTR("Daily Claim :: Warning :: Failed to emit service fee payment.");
            
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