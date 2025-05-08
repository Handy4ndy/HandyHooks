/**
 * This hook is set on payment to the ConfigHook, that forwards a percentage of all XAH payments. 
 * See README.md https://github.com/Handy4ndy/HandyHooks/blob/main/ForwardSplit/README.md).
 * Testnet Hook Hash: 876D545054F1D1F4B4C59D5D407A8B86C390B8E836D791D6930D30B1854D7454
 * Mainnet Hook Hash: ****************************************************************
 */

 #include "hookapi.h"

 #define DONE(x) accept(SBUF(x), __LINE__)
 #define NOPE(x) rollback(SBUF(x), __LINE__)
 
 int64_t hook(uint32_t reserved) {
 
     uint8_t main_acc[20];
     // Get the Main account from the hook params
     if (hook_param(SBUF(main_acc), "M", 1) != 20)
         NOPE("Forwarder: Error: Main Account not set");
 
     uint8_t second_acc[20];
     // Get the Second account from the hook params
     if (hook_param(SBUF(second_acc), "S", 1) != 20)
         NOPE("Forwarder: Error: Second Account not set");
 
     uint8_t third_acc[20];
     // Get the Third account from the hook params
     if (hook_param(SBUF(third_acc), "T", 1) != 20)
         NOPE("Forwarder: Error: Third Account not set");
 
     uint32_t main_amount;
     // Get the Main amount from the hook params
     if (hook_param(SVAR(main_amount), "MA", 2) != 4)
         NOPE("Forwarder: Error: Main Amount not set");
 
     uint32_t second_amount;
     // Get the Second amount from the hook params
     if (hook_param(SVAR(second_amount), "SA", 2) != 4)
         NOPE("Forwarder: Error: Secondary amount not set");
 
     uint32_t third_amount;
     // Get the Third amount from the hook params
     if (hook_param(SVAR(third_amount), "TA", 2) != 4)
         NOPE("Forwarder: Error: Third amount not set");
 
     // Ensure the accounts are unique
     if (BUFFER_EQUAL_20(main_acc, second_acc))
         NOPE("Forwarder: Error: Matching accounts found. Bailing..");
 
     if (BUFFER_EQUAL_20(third_acc, second_acc) || BUFFER_EQUAL_20(third_acc, main_acc))
         NOPE("Forwarder: Error: Matching accounts found. Bailing..");
 
     // Ensure the total percentage does not exceed 99%
     if (main_amount + second_amount + third_amount > 99) {
         NOPE("Forwarder: Error: Total percentage exceeds 99%");
     }
 
     // Log the main account, second account, third account, main amount, second amount, and third amount
     TRACEHEX(main_acc);
     TRACEHEX(second_acc);
     TRACEHEX(third_acc);
     TRACEHEX(main_amount);
     TRACEHEX(second_amount);
     TRACEHEX(third_amount);
 
     uint8_t hook_acc[20];
     // Get the hook account
     hook_account(SBUF(hook_acc));
 
     uint8_t account[20];
     // Get the originating account of the transaction
     otxn_field(SBUF(account), sfAccount);
 
     // If the transaction is outgoing from the hook account, accept it
     if (BUFFER_EQUAL_20(hook_acc, account))
         DONE("Outgoing Transaction");
 
     uint8_t amount[8];
     // Get the transaction amount
     if (otxn_field(SBUF(amount), sfAmount) != 8)
         DONE("Non-Xah Transaction, accepted!");
 
     // Reserve space for three emitted transactions
     etxn_reserve(3);
 
     // Convert transaction amount to XFL
     int64_t total_xfl = float_sto_set(amount, 8);
     if (total_xfl < 0)
         NOPE("Forwarder: Error converting total amount to XFL");
 
     // Create XFL for 100
     int64_t hundred_xfl = float_set(0, 100);
     if (hundred_xfl < 0)
         NOPE("Forwarder: Error creating hundred_xfl");
 
     // Convert percentages to XFL fractions
     int64_t main_amount_xfl = float_set(0, main_amount);
     if (main_amount_xfl < 0)
         NOPE("Forwarder: Error creating main_amount_xfl");
 
     int64_t main_percent_xfl = float_divide(main_amount_xfl, hundred_xfl);
     if (main_percent_xfl < 0)
         NOPE("Forwarder: Error calculating main percent");
 
     int64_t second_amount_xfl = float_set(0, second_amount);
     if (second_amount_xfl < 0)
         NOPE("Forwarder: Error creating second_amount_xfl");
 
     int64_t second_percent_xfl = float_divide(second_amount_xfl, hundred_xfl);
     if (second_percent_xfl < 0)
         NOPE("Forwarder: Error calculating second percent");
 
     int64_t third_amount_xfl = float_set(0, third_amount);
     if (third_amount_xfl < 0)
         NOPE("Forwarder: Error creating third_amount_xfl");
 
     int64_t third_percent_xfl = float_divide(third_amount_xfl, hundred_xfl);
     if (third_percent_xfl < 0)
         NOPE("Forwarder: Error calculating third percent");
 
     // Calculate forwarded amounts using float_multiply
     int64_t amount1_xfl = float_multiply(total_xfl, main_percent_xfl);
     if (amount1_xfl < 0)
         NOPE("Forwarder: Error calculating amount1");
 
     int64_t amount2_xfl = float_multiply(total_xfl, second_percent_xfl);
     if (amount2_xfl < 0)
         NOPE("Forwarder: Error calculating amount2");
 
     int64_t amount3_xfl = float_multiply(total_xfl, third_percent_xfl);
     if (amount3_xfl < 0)
         NOPE("Forwarder: Error calculating amount3");
 
     // Calculate total forwarded amount
     int64_t sum_forwarded = float_sum(amount1_xfl, amount2_xfl);
     if (sum_forwarded < 0)
         NOPE("Forwarder: Error summing amounts");
 
     sum_forwarded = float_sum(sum_forwarded, amount3_xfl);
     if (sum_forwarded < 0)
         NOPE("Forwarder: Error summing amounts");
 
     // Calculate remaining amount
     int64_t remaining_xfl = float_sum(total_xfl, float_negate(sum_forwarded));
     if (remaining_xfl < 0)
         NOPE("Forwarder: Error calculating remaining amount");
 
     // Check if remaining amount is at least 1% of total
     int64_t one_percent_xfl = float_divide(total_xfl, hundred_xfl);
     if (one_percent_xfl < 0)
         NOPE("Forwarder: Error calculating 1% of total");
 
     if (float_compare(remaining_xfl, one_percent_xfl, COMPARE_LESS) == 1)
         NOPE("Forwarder: Error: Remaining amount less than 1%");
 
     // Convert forwarded amounts back to integer drops
     uint64_t amount1 = (uint64_t)float_int(amount1_xfl, 0, 0);
     uint64_t amount2 = (uint64_t)float_int(amount2_xfl, 0, 0);
     uint64_t amount3 = (uint64_t)float_int(amount3_xfl, 0, 0);
     uint64_t remaining_amount = (uint64_t)float_int(remaining_xfl, 0, 0);
 
     // Convert drops back to XAH for logging (1 XAH = 1,000,000 drops)
     uint64_t total_amount_xah = (uint64_t)float_int(total_xfl, 0, 0) / 1000000;
     uint64_t amount1_xah = amount1 / 1000000;
     uint64_t amount2_xah = amount2 / 1000000;
     uint64_t amount3_xah = amount3 / 1000000;
     uint64_t remaining_amount_xah = remaining_amount / 1000000;
 
     // Log the amounts being forwarded to each account in XAH
     trace_num(SBUF("Total Amount (XAH): "), total_amount_xah);
     trace_num(SBUF("Amount to Main Account (XAH): "), amount1_xah);
     trace_num(SBUF("Amount to Second Account (XAH): "), amount2_xah);
     trace_num(SBUF("Amount to Third Account (XAH): "), amount3_xah);
     trace_num(SBUF("Remaining Amount (XAH): "), remaining_amount_xah);
 
     uint8_t txn1[PREPARE_PAYMENT_SIMPLE_SIZE];
     uint8_t txn2[PREPARE_PAYMENT_SIMPLE_SIZE];
     uint8_t txn3[PREPARE_PAYMENT_SIMPLE_SIZE];
 
     // Prepare the payment transactions
     PREPARE_PAYMENT_SIMPLE(txn1, amount1, main_acc, 0, 0);
     PREPARE_PAYMENT_SIMPLE(txn2, amount2, second_acc, 0, 0);
     PREPARE_PAYMENT_SIMPLE(txn3, amount3, third_acc, 0, 0);
 
     uint8_t emithash1[32];
     uint8_t emithash2[32];
     uint8_t emithash3[32];
 
     // Emit the transactions and check if they were successful
     if (emit(SBUF(emithash1), SBUF(txn1)) != 32 ||
         emit(SBUF(emithash2), SBUF(txn2)) != 32 ||
         emit(SBUF(emithash3), SBUF(txn3)) != 32) {
         NOPE("Forwarder: Failed to emit transactions");
     }
 
     DONE("Amount forwarded Successfully");
 
     _g(1,1);   // every hook needs to import guard function and use it at least once
     // unreachable
     return 0;
 }