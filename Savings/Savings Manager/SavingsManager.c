//**************************************************************
// Savings Manager Hook - Xahau HandyHook Collection
// Author: @Handy_4ndy
//
// Description:
//   This hook creates a time-released manageable escrow system for 
//   parent-controlled child savings accounts. Acts as a financial 
//   guardian enforcing spending limits and automatic savings discipline.
//   All incoming payments are automatically locked until released by
//   the designated parent/admin account through controlled intervals
//   or manual milestone adjustments.
//
// Hook Parameters (set during installation):
//   'ADMIN' (20 bytes): Parent/guardian account ID with full control.
//
// Configuration Parameters (via ttINVOKE from admin account):
//   'SET_INTERVAL' (4 bytes): Release interval in ledgers (default: 17280 = 24h).
//   'AUTO_RELEASE' (4 bytes): Automatic release percentage per interval (0-100%).
//   'RELEASE' (4 bytes): Manual release percentage for milestones (1-100%).
//   'STATUS' (1 byte): Query current locked, available, and spent balances.
//   'UNLOCK' (4 bytes): Emergency unlock code (0xDEADBEEF) - all funds available.
//
// Core Functionality:
//   - All incoming payments automatically locked in escrow (cannot be disabled)
//   - Parent controls release schedule via time intervals and manual adjustments
//   - Spending enforcement: outgoing payments blocked if insufficient available balance
//   - Real-time balance tracking: locked, available, and spent amounts
//   - Milestone releases: birthdays, achievements, rewards, etc.
//
// Child Account Behavior:
//   - Cannot access locked funds until parent releases them
//   - Can only spend up to available balance (strictly enforced)
//   - Outgoing payments deduct from available balance in real-time
//   - Complete transaction monitoring and parental oversight
//
// Usage:
//   1. Child installs hook with ADMIN parameter set to parent's account ID
//   2. Parent configures release intervals and percentages via ttINVOKE
//   3. All incoming funds automatically locked in escrow
//   4. Parent releases funds via intervals or manual milestones
//   5. Child can only spend released (available) balance
//
//**************************************************************

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

// Macro to convert XRP Amount buffer to drops (for native XAH)
#define AMOUNT_TO_DROPS(buf) \
    ({ \
        int64_t drops = 0; \
        if ((buf)[0] == 0x40) { \
            drops = (((uint64_t)(buf)[1] << 48) + ((uint64_t)(buf)[2] << 40) + \
                     ((uint64_t)(buf)[3] << 32) + ((uint64_t)(buf)[4] << 24) + \
                     ((uint64_t)(buf)[5] << 16) + ((uint64_t)(buf)[6] << 8) + \
                     (uint64_t)(buf)[7]); \
        } else { \
            drops = *((int64_t*)buf); \
        } \
        drops; \
    })

// State keys
#define ADMIN_KEY       0x41444D494E000000ULL  // "ADMIN"
#define LOCKED_KEY      0x4C4F434B45440000ULL  // "LOCKED"  
#define AVAILABLE_KEY   0x415641494C000000ULL  // "AVAIL"
#define AUTO_LOCK_KEY   0x4155544F4C4F434BULL  // "AUTOLCK"
#define INTERVAL_KEY    0x494E54455256414CULL  // "INTERVAL"
#define LAST_RELEASE_KEY 0x4C41535452454C45ULL // "LASTRELE"
#define SPENT_KEY       0x5350454E54000000ULL  // "SPENT"
#define UNLOCK_KEY      0x554E4C4F434B0000ULL  // "UNLOCK" (correct: 8 bytes)

int64_t hook(uint32_t reserved)
{
    TRACESTR("Savings Manager: Called");

    uint8_t hook_acc[20];
    uint8_t otxn_acc[20];
    hook_account(SBUF(hook_acc));
    otxn_field(SBUF(otxn_acc), sfAccount);

    int64_t tt = otxn_type();

    // Handle ttINVOKE for admin configuration
    if (tt == ttINVOKE)
    {
         // LOCK toggle - From Admin Hook Locker, pass through without processing
        uint8_t lock_param[1];
        if (otxn_param(SBUF(lock_param), "LOCK", 4) == 1)
        {
            // Early exit: If LOCK param exists, it's for the Admin Hook Locker (first hook)
            // Pass it through without processing - Savings Manager doesn't use LOCK toggle
            DONE("LOCK parameter passed to Admin Hook Locker");
        }

        // Load admin account from Hook Parameters (set during installation)
        uint8_t admin_acc[20];
        if (hook_param(SBUF(admin_acc), "ADMIN", 5) != 20)
            NOPE("Misconfigured: Admin account not set as Hook Parameter during installation");
        
        // Only admin can configure via ttINVOKE
        if (!BUFFER_EQUAL_20(admin_acc, otxn_acc))
            NOPE("Only admin can configure");

        // Set AUTO_RELEASE percentage for interval releases
        uint8_t auto_release_param[4];
        if (otxn_param(SBUF(auto_release_param), "AUTO_RELEASE", 12) == 4)
        {
            uint32_t auto_percent = UINT32_FROM_BUF(auto_release_param);
            if (auto_percent > 100)
                NOPE("AUTO_RELEASE percentage must be 0-100");
            
            uint8_t auto_lock_key_buf[8];
            UINT64_TO_BUF(auto_lock_key_buf, AUTO_LOCK_KEY);
            state_set(SBUF(auto_release_param), SBUF(auto_lock_key_buf));
            DONE("Auto-release percentage configured");
        }

        // Set release interval
        uint8_t interval_param[4];
        if (otxn_param(SBUF(interval_param), "SET_INTERVAL", 12) == 4)
        {
            uint8_t interval_key_buf[8];
            UINT64_TO_BUF(interval_key_buf, INTERVAL_KEY);
            state_set(SBUF(interval_param), SBUF(interval_key_buf));
            DONE("Release interval configured");
        }

        // Manual RELEASE parameter (percentage to release)
        uint8_t release_param[4];
        if (otxn_param(SBUF(release_param), "RELEASE", 7) == 4)
        {
            uint32_t release_percent = UINT32_FROM_BUF(release_param);
            if (release_percent == 0 || release_percent > 100)
                NOPE("Release percentage must be 1-100");

            // Check release interval timing (prevent spam)
            uint32_t current_ledger = (uint32_t)ledger_seq();
            uint32_t release_interval = 17280; // Default 24 hours
            
            uint8_t interval_key_buf[8];
            UINT64_TO_BUF(interval_key_buf, INTERVAL_KEY);
            uint8_t interval_data[4];
            if (state(SBUF(interval_data), SBUF(interval_key_buf)) == 4)
                release_interval = UINT32_FROM_BUF(interval_data);

            uint8_t last_release_key_buf[8];
            UINT64_TO_BUF(last_release_key_buf, LAST_RELEASE_KEY);
            uint8_t last_release_data[4];
            uint32_t last_release_ledger = 0;
            if (state(SBUF(last_release_data), SBUF(last_release_key_buf)) == 4)
                last_release_ledger = UINT32_FROM_BUF(last_release_data);

            if (last_release_ledger > 0)
            {
                uint32_t ledgers_elapsed = current_ledger - last_release_ledger;
                if (ledgers_elapsed < release_interval)
                    NOPE("Release too soon - wait for interval");
            }

            // Get current locked amount
            uint8_t locked_key_buf[8];
            UINT64_TO_BUF(locked_key_buf, LOCKED_KEY);
            uint8_t locked_data[8];
            uint64_t locked_amount = 0;
            if (state(SBUF(locked_data), SBUF(locked_key_buf)) == 8)
                locked_amount = UINT64_FROM_BUF(locked_data);

            if (locked_amount == 0)
                NOPE("No locked funds to release");

            // Calculate release amount
            uint64_t release_amount = (locked_amount * release_percent) / 100;
            uint64_t new_locked = locked_amount - release_amount;

            // Update locked amount
            UINT64_TO_BUF(locked_data, new_locked);
            state_set(SBUF(locked_data), SBUF(locked_key_buf));

            // Get current available amount
            uint8_t avail_key_buf[8];
            UINT64_TO_BUF(avail_key_buf, AVAILABLE_KEY);
            uint8_t avail_data[8];
            uint64_t available_amount = 0;
            if (state(SBUF(avail_data), SBUF(avail_key_buf)) == 8)
                available_amount = UINT64_FROM_BUF(avail_data);

            // Add to available amount
            available_amount += release_amount;
            UINT64_TO_BUF(avail_data, available_amount);
            state_set(SBUF(avail_data), SBUF(avail_key_buf));

            // Update last release time
            UINT64_TO_BUF(last_release_data, current_ledger);
            state_set(SBUF(last_release_data), SBUF(last_release_key_buf));

            // Convert drops to XAH for display (amounts are stored as drops)
            int64_t release_xah = release_amount / 1000000;
            TRACEVAR(release_percent);
            TRACEVAR(release_xah);
            DONE("Funds released to available balance");
        }

        // STATUS query - return account balance information
        uint8_t status_param[1];
        if (otxn_param(SBUF(status_param), "STATUS", 6) == 1)
        {
            // Get current balances
            uint8_t locked_key_buf[8], avail_key_buf[8];
            UINT64_TO_BUF(locked_key_buf, LOCKED_KEY);
            UINT64_TO_BUF(avail_key_buf, AVAILABLE_KEY);

            uint8_t locked_data[8], avail_data[8];
            uint64_t locked_amount = 0, available_amount = 0;
            
            if (state(SBUF(locked_data), SBUF(locked_key_buf)) == 8)
                locked_amount = UINT64_FROM_BUF(locked_data);
            if (state(SBUF(avail_data), SBUF(avail_key_buf)) == 8)
                available_amount = UINT64_FROM_BUF(avail_data);

            // Convert drops to XAH for display (amounts are stored as drops)
            int64_t locked_xah = locked_amount / 1000000;
            int64_t available_xah = available_amount / 1000000;
            int64_t total_xah = locked_xah + available_xah;
            
            TRACEVAR(locked_xah);
            TRACEVAR(available_xah);
            TRACEVAR(total_xah);
            DONE("Status query completed");
        }

        // UNLOCK toggle - permanently changes escrow behavior
        uint8_t unlock_param[1];
        if (otxn_param(SBUF(unlock_param), "UNLOCK", 6) == 1)
        {
            uint8_t unlock_key_buf[8];
            UINT64_TO_BUF(unlock_key_buf, UNLOCK_KEY);
            
            // Read current unlock state
            uint8_t unlock_state[1];
            int32_t state_len = state(SBUF(unlock_state), SBUF(unlock_key_buf));
            
            uint8_t new_state[1];
            if (state_len == 1 && unlock_state[0] == 1)
            {
                // Unlock is ON, toggle it OFF
                new_state[0] = 0;
                state_set(SBUF(new_state), SBUF(unlock_key_buf));
                DONE("Lock re-enabled - incoming funds will be locked again");
            }
            else
            {
                // Unlock is OFF, toggle it ON - move all locked to available
                new_state[0] = 1;
                state_set(SBUF(new_state), SBUF(unlock_key_buf));
                
                // Move all locked funds to available
                uint8_t locked_key_buf[8];
                uint8_t avail_key_buf[8];
                UINT64_TO_BUF(locked_key_buf, LOCKED_KEY);
                UINT64_TO_BUF(avail_key_buf, AVAILABLE_KEY);
                
                uint8_t locked_data[8];
                uint64_t locked_amount = 0;
                if (state(SBUF(locked_data), SBUF(locked_key_buf)) == 8)
                    locked_amount = UINT64_FROM_BUF(locked_data);
                
                if (locked_amount > 0)
                {
                    uint8_t avail_data[8];
                    uint64_t available_amount = 0;
                    if (state(SBUF(avail_data), SBUF(avail_key_buf)) == 8)
                        available_amount = UINT64_FROM_BUF(avail_data);
                    
                    // Move all locked to available
                    available_amount += locked_amount;
                    UINT64_TO_BUF(avail_data, available_amount);
                    state_set(SBUF(avail_data), SBUF(avail_key_buf));
                    
                    // Clear locked amount
                    UINT64_TO_BUF(locked_data, 0);
                    state_set(SBUF(locked_data), SBUF(locked_key_buf));
                    
                    int64_t unlocked_xah = locked_amount / 1000000;
                    TRACEVAR(unlocked_xah);
                }

                DONE("Lock disabled - all funds unlocked and available for spending");
            }
        }

        NOPE("No valid parameters");
    }

    // Control outgoing transactions from hook account (child spending)
    if (BUFFER_EQUAL_20(hook_acc, otxn_acc))
    {
        // Only control ttPAYMENT transactions for spending enforcement
        if (tt == ttPAYMENT)
        {
            uint8_t amount[8];
            if (otxn_field(SBUF(amount), sfAmount) == 8)
            {
                // Convert XAH amount to drops using proper macro
                int64_t payment_drops = AMOUNT_TO_DROPS(amount);
                
                // Get current available balance
                uint8_t avail_key_buf[8];
                UINT64_TO_BUF(avail_key_buf, AVAILABLE_KEY);
                uint8_t avail_data[8];
                uint64_t available_amount = 0;
                if (state(SBUF(avail_data), SBUF(avail_key_buf)) == 8)
                    available_amount = UINT64_FROM_BUF(avail_data);
                
                // CHECK FOR AUTO-RELEASE FIRST (before payment validation)
                uint32_t current_ledger = (uint32_t)ledger_seq();
                uint32_t release_interval = 17280; // Default 24 hours
                
                // Get configured release interval
                uint8_t interval_key_buf[8];
                UINT64_TO_BUF(interval_key_buf, INTERVAL_KEY);
                uint8_t interval_data[4];
                if (state(SBUF(interval_data), SBUF(interval_key_buf)) == 4)
                    release_interval = UINT32_FROM_BUF(interval_data);

                // Get last release timestamp
                uint8_t last_release_key_buf[8];
                UINT64_TO_BUF(last_release_key_buf, LAST_RELEASE_KEY);
                uint8_t last_release_data[4];
                uint32_t last_release_ledger = 0;
                if (state(SBUF(last_release_data), SBUF(last_release_key_buf)) == 4)
                    last_release_ledger = UINT32_FROM_BUF(last_release_data);

                // Auto-release if interval has passed and auto-release percentage is set
                if (release_interval > 0 && (current_ledger - last_release_ledger) >= release_interval)
                {
                    // Get auto-release percentage
                    uint8_t auto_lock_key_buf[8];
                    UINT64_TO_BUF(auto_lock_key_buf, AUTO_LOCK_KEY);
                    uint8_t auto_release_data[4];
                    if (state(SBUF(auto_release_data), SBUF(auto_lock_key_buf)) == 4)
                    {
                        uint32_t auto_percent = UINT32_FROM_BUF(auto_release_data);
                        if (auto_percent > 0)
                        {
                            // Get current locked amount
                            uint8_t locked_key_buf[8];
                            UINT64_TO_BUF(locked_key_buf, LOCKED_KEY);
                            uint8_t locked_data[8];
                            uint64_t locked_amount = 0;
                            if (state(SBUF(locked_data), SBUF(locked_key_buf)) == 8)
                                locked_amount = UINT64_FROM_BUF(locked_data);
                            
                            if (locked_amount > 0)
                            {
                                // Calculate auto-release amount
                                uint64_t auto_release_amount = (locked_amount * auto_percent) / 100;
                                
                                // Move from locked to available
                                locked_amount -= auto_release_amount;
                                UINT64_TO_BUF(locked_data, locked_amount);
                                state_set(SBUF(locked_data), SBUF(locked_key_buf));
                                
                                // Update available balance
                                available_amount += auto_release_amount;
                                UINT64_TO_BUF(avail_data, available_amount);
                                state_set(SBUF(avail_data), SBUF(avail_key_buf));
                                
                                // Update last release timestamp
                                UINT32_TO_BUF(last_release_data, current_ledger);
                                state_set(SBUF(last_release_data), SBUF(last_release_key_buf));
                                
                                // Convert drops to XAH for display
                                int64_t released_xah = auto_release_amount / 1000000;
                                TRACEVAR(auto_percent);
                                TRACEVAR(released_xah);
                                TRACESTR("Auto-release triggered by payment attempt");
                            }
                        }
                    }
                }
                
                // NOW check if payment is allowed with updated available balance
                if (payment_drops > available_amount)
                {
                    int64_t payment_xah = payment_drops / 1000000;
                    int64_t available_xah = available_amount / 1000000;
                    TRACEVAR(payment_xah);
                    TRACEVAR(available_xah);
                    NOPE("Insufficient available balance - wait for parent release or timer interval");
                }
                
                // Payment approved - deduct from available balance
                available_amount -= payment_drops;
                UINT64_TO_BUF(avail_data, available_amount);
                state_set(SBUF(avail_data), SBUF(avail_key_buf));
                
                // Update total spent tracking
                uint8_t spent_key_buf[8];
                UINT64_TO_BUF(spent_key_buf, SPENT_KEY);
                uint8_t spent_data[8];
                uint64_t total_spent = 0;
                if (state(SBUF(spent_data), SBUF(spent_key_buf)) == 8)
                    total_spent = UINT64_FROM_BUF(spent_data);
                
                total_spent += payment_drops;
                UINT64_TO_BUF(spent_data, total_spent);
                state_set(SBUF(spent_data), SBUF(spent_key_buf));
                
                int64_t payment_xah = payment_drops / 1000000;
                int64_t available_xah = available_amount / 1000000;
                TRACEVAR(payment_xah);
                TRACEVAR(available_xah);
            }
        }
        DONE("Outgoing transaction approved");
    }

    // Handle incoming ttPAYMENT - always lock all incoming funds (unless unlocked)
    if (tt == ttPAYMENT)
    {
        uint8_t amount[8];
        if (otxn_field(SBUF(amount), sfAmount) != 8)
            DONE("Non-native currency");

        // Check if escrow is disabled via UNLOCK toggle
        uint8_t unlock_key_buf[8];
        UINT64_TO_BUF(unlock_key_buf, UNLOCK_KEY);
        uint8_t unlock_state[1];
        int32_t unlock_len = state(SBUF(unlock_state), SBUF(unlock_key_buf));
        
        int32_t escrow_disabled = (unlock_len == 1 && unlock_state[0] == 1);
        
        // Convert XAH amount to drops using proper macro
        int64_t incoming_drops = AMOUNT_TO_DROPS(amount);
        
        if (incoming_drops <= 0)
            DONE("Zero or invalid amount payment");

        if (escrow_disabled)
        {
            // Escrow OFF: Move directly to available (no locking)
            uint8_t avail_key_buf[8];
            UINT64_TO_BUF(avail_key_buf, AVAILABLE_KEY);
            uint8_t avail_data[8];
            uint64_t available_amount = 0;
            if (state(SBUF(avail_data), SBUF(avail_key_buf)) == 8)
                available_amount = UINT64_FROM_BUF(avail_data);
            
            available_amount += incoming_drops;
            UINT64_TO_BUF(avail_data, available_amount);
            state_set(SBUF(avail_data), SBUF(avail_key_buf));
            
            DONE("Payment accepted directly to available balance (Lock disabled)");
        }
        else
        {
            // Escrow ON: Lock all incoming (original behavior)
            // Always lock incoming payments - core escrow functionality
            uint8_t locked_key_buf[8];
            UINT64_TO_BUF(locked_key_buf, LOCKED_KEY);
            uint8_t locked_data[8];
            uint64_t locked_amount = 0;
            if (state(SBUF(locked_data), SBUF(locked_key_buf)) == 8)
                locked_amount = UINT64_FROM_BUF(locked_data);

            locked_amount += incoming_drops;
            UINT64_TO_BUF(locked_data, locked_amount);
            state_set(SBUF(locked_data), SBUF(locked_key_buf));

            // Check for automatic interval-based release
            uint32_t current_ledger = (uint32_t)ledger_seq();
            uint32_t release_interval = 17280; // Default 24 hours
            
            uint8_t interval_key_buf[8];
            UINT64_TO_BUF(interval_key_buf, INTERVAL_KEY);
            uint8_t interval_data[4];
            if (state(SBUF(interval_data), SBUF(interval_key_buf)) == 4)
                release_interval = UINT32_FROM_BUF(interval_data);

            uint8_t last_release_key_buf[8];
            UINT64_TO_BUF(last_release_key_buf, LAST_RELEASE_KEY);
            uint8_t last_release_data[4];
            uint32_t last_release_ledger = 0;
            if (state(SBUF(last_release_data), SBUF(last_release_key_buf)) == 4)
                last_release_ledger = UINT32_FROM_BUF(last_release_data);

            // Auto-release if interval has passed and auto-release percentage is set
            if (release_interval > 0 && (current_ledger - last_release_ledger) >= release_interval)
            {
                uint8_t auto_lock_key_buf[8];
                UINT64_TO_BUF(auto_lock_key_buf, AUTO_LOCK_KEY);
                uint8_t auto_release_data[4];
                if (state(SBUF(auto_release_data), SBUF(auto_lock_key_buf)) == 4)
                {
                    uint32_t auto_percent = UINT32_FROM_BUF(auto_release_data);
                    if (auto_percent > 0)
                    {
                        // Calculate auto-release amount
                        uint64_t auto_release_amount = (locked_amount * auto_percent) / 100;
                        
                        // Move from locked to available
                        locked_amount -= auto_release_amount;
                        UINT64_TO_BUF(locked_data, locked_amount);
                        state_set(SBUF(locked_data), SBUF(locked_key_buf));
                        
                        uint8_t avail_key_buf[8];
                        UINT64_TO_BUF(avail_key_buf, AVAILABLE_KEY);
                        uint8_t avail_data[8];
                        uint64_t available_amount = 0;
                        if (state(SBUF(avail_data), SBUF(avail_key_buf)) == 8)
                            available_amount = UINT64_FROM_BUF(avail_data);
                        
                        available_amount += auto_release_amount;
                        UINT64_TO_BUF(avail_data, available_amount);
                        state_set(SBUF(avail_data), SBUF(avail_key_buf));
                        
                        // Update last release timestamp
                        UINT32_TO_BUF(last_release_data, current_ledger);
                        state_set(SBUF(last_release_data), SBUF(last_release_key_buf));
                        
                        // Convert drops to XAH for display (amounts are stored as drops)
                        int64_t released_xah = auto_release_amount / 1000000;
                        TRACEVAR(auto_percent);
                        TRACEVAR(released_xah);
                        TRACESTR("Auto-release triggered by interval");
                    }
                }
            }

            int64_t incoming_xah = incoming_drops / 1000000;
            int64_t total_locked_xah = locked_amount / 1000000;
            TRACEVAR(incoming_xah);
            TRACEVAR(total_locked_xah);
            DONE("Payment Added to locked balance");
        }
    }

    // Pass through all other transaction types
    DONE("Transaction passed through");

    _g(1,1);
    return 0;
}