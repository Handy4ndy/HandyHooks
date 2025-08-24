/**
 * HandyHooks - Reject Outgoing XAH Payment.
 * This hook has various combinations that reject an outgoing payment.
 * 
 * Uncomment sections below to customize this hook's behavior.
 * 1.  Simply reject the outgoing payment.
 * 2.  Only reject outgoing payments.
 * 3.  Reject XAH payment.
 * 4.  Only reject XAH payment.
 * 5.  Reject IOU.
 * 6.  Only reject IOU payments.
 * 7.  Reject XAH payment below a minimum amount.
 * 8.  Reject XAH payment above a maximum amount.
 * 9.  Reject if not exact amount.
 * 10. Reject if not a multiple of a fixed amount.
 *
 * Install on ttPayment.
 */

#include "hookapi.h"

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)

int64_t hook(uint32_t reserved) {

	TRACESTR("ROP :: Reject Outgoing Payment :: Called.");

	// Get the Hook account
	uint8_t hook_acc[20];
	hook_account(SBUF(hook_acc));

	TRACEHEX(hook_acc);

	// Get the Originating account of the transaction
	uint8_t otxn_acc[20];
	otxn_field(SBUF(otxn_acc), sfAccount);

	TRACEHEX(otxn_acc);

	//
	// Reject all incoming payments
	// ** Comment out for testing purposes **
	//
	if (!BUFFER_EQUAL_20(hook_acc, otxn_acc)){
		NOPE("ROP Hook :: Rejected :: Incoming payment.");
	}

	// UNCOMMENT SECTIONS BELOW TO TEST FUNCTIONALITY

	//
	// 1. Simply reject the outgoing payment.
	//
	NOPE("ROP Hook :: Rejected :: Outgoing payment.");

	// //
	// // 2. Only reject outgoing payments. 
	// // ** Requires Reject all incoming payments above to be commented **
	// //
	// if (!BUFFER_EQUAL_20(hook_acc, otxn_acc)){
	//     DONE("ROP Hook :: Accepted :: Incoming payment.");
	// } else{
	//     NOPE("ROP Hook :: Rejected :: Outgoing payment.");
	// }

	// //
	// // 3. Reject outgoing XAH Payment.
	// //
	// uint8_t amount[8];
	// if (otxn_field(SBUF(amount),sfAmount) == 8)
	//     NOPE("ROP Hook :: Rejected :: Outgoing XAH payment.");

	// //
	// // 4. Only reject outgoing XAH Payment.
	// //
	// uint8_t xah_amount[8];
	// if (otxn_field(SBUF(xah_amount),sfAmount) == 8){
	//     NOPE("ROP Hook :: Rejected :: Outgoing XAH payment.");
	// } else {
	//     DONE("ROP Hook :: Accepted :: Outgoing IOU payment.");
	// }
    
	// //
	// // 5. Reject outgoing IOU 
	// //
	// uint8_t iou_amount[48];
	// if (otxn_field(SBUF(iou_amount),sfAmount) != 8)
	//     NOPE("ROP Hook :: Rejected :: Outgoing IOU payment.");

	// //
	// // 6. Only reject outgoing IOU payments
	// //
	// uint8_t IOU_amount[48];
	// if (otxn_field(SBUF(IOU_amount),sfAmount) != 8){
	//     NOPE("ROP Hook :: Rejected :: Outgoing IOU payment.");
	// } else {
	//     DONE("ROP Hook :: Accepted :: Outgoing XAH payment.");
	// }

	// //
	// // 7. Reject outgoing XAH payment below a minimum amount
	// //

	// // Configure the minimum amount
	// uint64_t min_amount = 1; // 1 XAH
	// uint8_t amount_buffer[48];

	//     // Check the outgoing amount is XAH
	//     int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
	//         if (amount_len != 8)
	//             DONE("ROP Hook :: Accepted :: Outgoing IOU payment.");

	//         // Convert amount to drops to compare
	//         int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
	//         double xah_amount = (double)otxn_drops / 1000000.0;
	//         if (xah_amount < min_amount)
	//             NOPE("ROP Hook :: Rejected :: Outgoing payment is below minimum amount.");

	// DONE("ROP Hook :: Accepted :: Outgoing amount above the minimum amount threshold.");

	// //
	// // 8. Reject outgoing XAH payment above a maximum amount
	// //

	// // Configure the maximum amount
	// uint64_t max_amount = 100; // 100 XAH

	//     // Check the outgoing amount is XAH
	//     uint8_t amount_buffer[48];
	//     int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
	//         if (amount_len != 8)
	//             DONE("ROP Hook :: Accepted :: Outgoing IOU payment.");

	//         // Convert amount to drops to compare
	//         int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
	//         double xah_amount = (double)otxn_drops / 1000000.0;
	//         if (xah_amount > max_amount)
	//             NOPE("ROP Hook :: Rejected :: Outgoing Payment above maximum amount.");

	// DONE("ROP Hook :: Accepted :: Outgoing amount below the maximum amount threshold.");


	// //
	// // 9. Reject if not exact outgoing amount
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
	//     DONE("ROP Hook :: Accepted :: Outgoing IOU payment.");
	// }

	// // Check if the payment is not equal to the exact amount
	// if (xah_amount != exact_amount){
	//     NOPE("ROP Hook :: Rejected :: Outgoing Payment doesn't match the amount.");
	// }

	// DONE("ROP Hook :: Accepted :: Outgoing Payment matches the amount.");


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
	//     DONE("ROP Hook :: Accepted :: Non-XAH payment.");
	// }

	// // Check if the payment is not equal to the cost
	// if (xah_amount == cost_xah) {
	//     DONE("ROP Hook :: Accepted :: Payment matches the required amount");
	// }

	// // Reconstruct COST value 
	// uint64_t cost_drops = cost_xah * 1000000; // Convert XAH to drops

	// // Calculate if the payment is not a multiple of the cost
	// multi_count = otxn_drops / cost_drops;
	// if (multi_count == 0) {
	//     NOPE("ROP Hook :: Rejected :: Payment is less than the required amount!");
	// }
	// if (otxn_drops % cost_drops != 0) {
	//     NOPE("ROP Hook :: Rejected :: Payment amount must be a multiple of the required!");
	// }

	// TRACEVAR(multi_count);

	// DONE("ROP Hook :: Accepted :: Outgoing XAH payment is a multiple of the required amount.");

	_g(1,1);
	return 0;
}
