/**
 * HandyHooks - Reject Incoming XAH Payment.
 * This hook has various combinations that reject an incoming payment.
 * 
 * Uncomment sections below to customize this hook's behavior.
 * 1.  Simply reject the incoming payment.
 * 2.  Only reject incoming payments.
 * 3.  Reject XAH payments.
 * 4.  Only reject XAH payments.
 * 5.  Reject IOU payments.
 * 6.  Only reject IOU payments.
 * 7.  Reject XAH payments below a minimum amount.
 * 8.  Reject XAH payments above a maximum amount.
 * 9.  Reject if not exact amount.
 * 10. Reject if not a multiple of a fixed amount.
 *
 * Install on ttPayment.
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

int64_t hook(uint32_t reserved) {

	TRACESTR("RIP :: Reject Incoming Payment :: Called.");

	// Get the Hook account
	uint8_t hook_acc[20];
	hook_account(SBUF(hook_acc));

	TRACEHEX(hook_acc);

	// Get the Originating account of the transaction
	uint8_t otxn_acc[20];
	otxn_field(SBUF(otxn_acc), sfAccount);

	TRACEHEX(otxn_acc);

	//
	// Reject all outgoing payments
	// ** Comment out for testing purposes **
	//
	if (BUFFER_EQUAL_20(hook_acc, otxn_acc)){
		NOPE("RIP Hook :: Rejected :: Outgoing payment.");
	}

	// UNCOMMENT SECTIONS BELOW TO TEST FUNCTIONALITY

	//
	// 1. Simply reject the incoming payment.
	//
	NOPE("RIP Hook :: Rejected :: Incoming payment.");

	// //
	// // 2. Only reject incoming Payments. 
	// // ** Requires Reject all outgoing payments above to be commented **
	// //
	// if (BUFFER_EQUAL_20(hook_acc, otxn_acc)){
	//     DONE("RIP Hook :: Accepted :: Outgoing payment.");
	// } else{
	//     NOPE("RIP Hook :: Rejected :: Incoming payment.");
	// }

	// //
	// // 3. Reject incoming XAH Payment.
	// //
	// uint8_t amount[8];
	// if (otxn_field(SBUF(amount),sfAmount) == 8)
	//     NOPE("RIP Hook :: Rejected :: Incoming XAH payment.");

	// //
	// // 4. Only reject incoming XAH Payment.
	// //
	// uint8_t xah_amount[8];
	// if (otxn_field(SBUF(xah_amount),sfAmount) == 8){
	//     NOPE("RIP Hook :: Rejected :: Incoming XAH payment.");
	// } else {
	//     DONE("RIP Hook :: Accepted :: Incoming IOU payment.");
	// }
    
	// //
	// // 5. Reject incoming IOU 
	// //
	// uint8_t iou_amount[48];
	// if (otxn_field(SBUF(iou_amount),sfAmount) != 8)
	//     NOPE("RIP Hook :: Rejected :: Incoming IOU payment.");

	// //
	// // 6. Only reject incoming IOU payments
	// //
	// uint8_t IOU_amount[48];
	// if (otxn_field(SBUF(IOU_amount),sfAmount) != 8){
	//     NOPE("RIP Hook :: Rejected :: Incoming IOU payment.");
	// } else {
	//     DONE("RIP Hook :: Accepted :: Incoming XAH payment.");
	// }

	// //
	// // 7. Reject incoming XAH payment below a minimum amount
	// //

	// // Configure the minimum amount
	// uint64_t min_amount = 1; // 1 XAH
	// uint8_t amount_buffer[48];

	//     // Check the incoming amount is XAH
	//     int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
	//         if (amount_len != 8)
	//             DONE("RIP Hook :: Accepted :: Incoming IOU payment.");

	//         // Convert amount to drops to compare
	//         int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
	//         double xah_amount = (double)otxn_drops / 1000000.0;
	//         if (xah_amount < min_amount)
	//             NOPE("RIP Hook :: Rejected :: Incoming payment is below minimum amount.");

	// DONE("RIP Hook :: Accepted :: Incoming amount above the minimum amount threshold.");

	// //
	// // 8. Reject incoming XAH payment above a maximum amount
	// //

	// // Configure the maximum amount
	// uint64_t max_amount = 100; // 100 XAH

	//     // Check the incoming amount is XAH
	//     uint8_t amount_buffer[48];
	//     int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
	//         if (amount_len != 8)
	//             DONE("RIP Hook :: Accepted :: Incoming IOU payment.");

	//         // Convert amount to drops to compare
	//         int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
	//         double xah_amount = (double)otxn_drops / 1000000.0;
	//         if (xah_amount > max_amount)
	//             NOPE("RIP Hook :: Rejected :: Incoming Payment above maximum amount.");

	// DONE("RIP Hook :: Accepted :: Incoming amount below the maximum amount threshold.");


	// //
	// // 9. Reject if not exact incoming amount
	// //

	// // Configure an Exact amount
	// uint64_t exact_amount = 10; // 10 XAH
	// TRACEVAR(exact_amount);

	// // Convert the amount from drops to XAH
	// uint8_t amount_buffer[48];
	// int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
	// int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
	// double xah_amount = (double)otxn_drops / 1000000.0; // Convert to XAH
	// TRACEVAR(xah_amount);

	// // Ensure the payment is XAH
	// if (amount_len != 8){
	//     DONE("RIP Hook :: Accepted :: Incoming IOU payment.");
	// }

	// // Check if the payment is not equal to the exact amount
	// if (xah_amount != exact_amount){
	//     NOPE("RIP Hook :: Rejected :: Incoming Payment doesn't match the amount required.");
	// }

	// DONE("RIP Hook :: Accepted :: Incoming Payment matches the amount required.");


	// //
	// // 10. Reject if not a multiple of a fixed amount
	// //

	// // Configure the cost in XAH
	// uint64_t cost_xah = 10;
	// uint64_t multi_count = 0;

	// // Convert the amount from drops to XAH
	// unsigned char amount_buffer[48];
	// int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
	// int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
	// double xah_amount = (double)otxn_drops / 1000000.0; // Convert to XAH

	// TRACEVAR(xah_amount);
	// TRACEVAR(cost_xah);

	// // Ensure the payment is XAH
	// if (amount_len != 8){
	//     DONE("RIP Hook :: Accepted :: Non-XAH payment.");
	// }

	// // Check if the payment is not equal to the cost
	// if (xah_amount == cost_xah) {
	//     DONE("RIP Hook :: Accepted :: Payment matches the required amount.");
	// }

	// // Reconstruct COST value 
	// uint64_t cost_drops = cost_xah * 1000000; // Convert XAH to drops

	// // Calculate if the payment is not a multiple of the cost
	// multi_count = otxn_drops / cost_drops;
	// if (multi_count == 0) {
	//     NOPE("RIP Hook :: Rejected :: Payment is less than the required amount!");
	// }
	// if (otxn_drops % cost_drops != 0) {
	//     NOPE("RIP Hook :: Rejected :: Payment amount must be a multiple of the required!");
	// }

	// TRACEVAR(multi_count);

	// DONE("RIP Hook :: Accepted :: Incoming XAH payment is a multiple of the required amount.");

	_g(1,1);
	return 0;
}
