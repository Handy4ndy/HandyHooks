/**
 * This hook is set on payment to the ConfigHook, that forwards a percentage of all XAH payments (see README.md (*Insert URL*) ).
 * Hook Hash :CB0866E8610CD0EFE7C39BF4146B6096E8C281EB62AD49544E5370FDB2C0C775
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

int64_t hook(uint32_t reserved ) {

    uint8_t main_acc[20];
    // Get the Main account from the hook params
    if(hook_param(SBUF(main_acc), "M", 1) != 20)
        NOPE("Forwarder: Error: Main Account not set");

    uint8_t second_acc[20];
    // Get the Second account from the hook params
    if(hook_param(SBUF(second_acc), "S", 1) != 20)
        NOPE("Forwarder: Error: Second Account not set");

    uint8_t third_acc[20];
    // Get the Third account from the hook params
    if(hook_param(SBUF(third_acc), "T", 1) != 20)
        NOPE("Forwarder: Error: Third Account not set");

    uint32_t main_amount;
    // Get the Main amount from the hook params
    if(hook_param(SVAR(main_amount), "MA", 2) != 4)
        NOPE("Forwarder: Error: Main Amount not set");

    uint32_t second_amount;
    // Get the Second amount from the hook params
    if(hook_param(SVAR(second_amount), "SA", 2) != 4)
        NOPE("Forwarder: Error: Secondary amount not set");

    uint32_t third_amount;
    // Get the Third amount from the hook params
    if(hook_param(SVAR(third_amount), "TA", 2) != 4)
        NOPE("Forwarder: Error: Third amount not set");


    // Ensure the accounts are unique
    if(BUFFER_EQUAL_20(main_acc, second_acc))
                NOPE("Forwarder: Error: Matching accounts found. Bailing..");

    if(BUFFER_EQUAL_20(third_acc, second_acc) || BUFFER_EQUAL_20(third_acc, main_acc))
                NOPE("Forwarder: Error: Matching accounts found. Bailing..");


    // Ensure the total percentage does not exceed 99%
    if (main_amount + second_amount + third_amount > 99) {
        NOPE("Forwarder: Error: Total percentage exceeds 99%");
    }
    
    // Log the main account, second account, third account, main amount, second amount, and third amount
    TRACEHEX(main_acc);
    TRACEHEX(second_acc);
    TRACEHEX(third_acc);

    // Log the main amount, second amount, and third amount
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
    if(BUFFER_EQUAL_20(hook_acc, account))
        DONE("Outgoing Transaction");

    uint8_t amount[8];
    // Get the transaction amount
    if(otxn_field(SBUF(amount), sfAmount) != 8)
        DONE("Non-Xah Transaction, accepted!");

    // Reserve space for three emitted transactions
    etxn_reserve(3);

    // Calculate the amounts to be forwarded 
    uint64_t total_amount = AMOUNT_TO_DROPS(amount);

    uint64_t amount1 = total_amount * main_amount / 100; // % to forward to main_acc
    uint64_t amount2 = total_amount * second_amount / 100; // % to forward to second_acc
    uint64_t amount3 = total_amount * third_amount / 100; // % to forward to third_acc
    uint64_t remaining_amount = total_amount - amount1 - amount2 - amount3; // 10% remains in hook wallet

    // Convert drops back to Xah for logging (1 Xah = 1,000,000 drops)
    uint64_t total_amount_xah = total_amount / 1000000;
    uint64_t amount1_xah = amount1 / 1000000;
    uint64_t amount2_xah = amount2 / 1000000;
    uint64_t amount3_xah = amount3 / 1000000;
    uint64_t remaining_amount_xah = remaining_amount / 1000000;

    // Log the amounts being forwarded to each account in Xah
    trace_num(SBUF("Total Amount (Xah): "), total_amount_xah);
    trace_num(SBUF("Amount to Main Account (Xah): "), amount1_xah);
    trace_num(SBUF("Amount to Second Account (Xah): "), amount2_xah);
    trace_num(SBUF("Amount to Third Account (Xah): "), amount3_xah);
    trace_num(SBUF("Remaining Amount (Xah): "), remaining_amount_xah);

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
    if(emit(SBUF(emithash1), SBUF(txn1)) != 32 ||
       emit(SBUF(emithash2), SBUF(txn2)) != 32 ||
       emit(SBUF(emithash3), SBUF(txn3)) != 32) {
        NOPE("Failed Please try again later");
    }

    DONE("Amount forwarded Successfully");

    _g(1,1);   // every hook needs to import guard function and use it at least once
    // unreachable
    return 0;
}
