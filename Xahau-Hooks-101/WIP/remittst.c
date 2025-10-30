#include "hookapi.h"
#include <stdint.h>

#define GUARD(maxiter) _g(__LINE__, (maxiter)+1)

#define ACCOUNT_TO_BUF(buf_raw, i)\
{\
    unsigned char* buf = (unsigned char*)buf_raw;\
    *(uint64_t*)(buf + 0) = *(uint64_t*)(i +  0);\
    *(uint64_t*)(buf + 8) = *(uint64_t*)(i +  8);\
    *(uint32_t*)(buf + 16) = *(uint32_t*)(i + 16);\
}

#define URI_TO_BUF(buf_raw, uri, len)\
{\
    unsigned char* buf = (unsigned char*)buf_raw;\
    for (int i = 0; GUARD(32), i < 32; ++i) \
        *(((uint64_t*)buf) + i) = *(((uint64_t*)uri) + i); \
    buf[len + 1] += 0xE1U; \
}

uint8_t txn[60000] =
{
/* size,upto */
/*   3,   0 */   0x12U, 0x00U, 0x5FU,                                                           /* tt = Remit       */
/*   5,   3 */   0x22U, 0x80U, 0x00U, 0x00U, 0x00U,                                          /* flags = tfCanonical */
/*   5,   8 */   0x24U, 0x00U, 0x00U, 0x00U, 0x00U,                                                 /* sequence = 0 */
/*   5,  13 */   0x99U, 0x99U, 0x99U, 0x99U, 0x99U,                                                /* dtag, flipped */
/*   6,  18 */   0x20U, 0x1AU, 0x00U, 0x00U, 0x00U, 0x00U,                                      /* first ledger seq */
/*   6,  24 */   0x20U, 0x1BU, 0x00U, 0x00U, 0x00U, 0x00U,                                       /* last ledger seq */
/*   9,  30 */   0x68U, 0x40U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,                         /* fee      */
/*  35,  39 */   0x73U, 0x21U, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,       /* pubkey   */
/*  22,  74 */   0x81U, 0x14U, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,                                  /* srcacc  */
/*  22,  96 */   0x83U, 0x14U, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,                                  /* dstacc  */
/* 116, 118 */   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,    /* emit detail */
                 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,

/*   3, 234 */  0xE0U, 0x5CU, 0x75U,
/*   1, 237 */  0xE1U,
/*   0, 238 */                
};
// clang-format on

#define BYTES_LEN 238U
#define FLS_OUT (txn + 20U)
#define LLS_OUT (txn + 26U)
#define DTAG_OUT (txn + 14U)
#define FEE_OUT (txn + 31U)
#define HOOK_ACC (txn + 76U)
#define OTX_ACC (txn + 98U)
#define URI_OUT (txn + 237U)
#define EMIT_OUT (txn + 118U)

#define PREPARE_REMIT_TXN(account_buffer, dest_buffer, uri_buffer, uri_len) do { \ 
    etxn_reserve(multi_count); \ 
    if (otxn_field(DTAG_OUT, 4, sfSourceTag) == 4) \
        *(DTAG_OUT - 1) = 0x2EU; \
    uint32_t fls = (uint32_t)ledger_seq() + 1; \ 
    *((uint32_t *)(FLS_OUT)) = FLIP_ENDIAN_32(fls); \ 
    uint32_t lls = fls + 4; \ 
    *((uint32_t *)(LLS_OUT)) = FLIP_ENDIAN_32(lls); \
    ACCOUNT_TO_BUF(HOOK_ACC, account_buffer); \ 
    ACCOUNT_TO_BUF(OTX_ACC, dest_buffer); \ 
    URI_TO_BUF(URI_OUT, uri_buffer, uri_len) \
    etxn_details(EMIT_OUT, 116U); \ 
    int64_t fee = etxn_fee_base(txn, BYTES_LEN + uri_len + 1); \ 
    uint8_t *b = FEE_OUT; \ 
    *b++ = 0b01000000 + ((fee >> 56) & 0b00111111); \ 
    *b++ = (fee >> 48) & 0xFFU; \ 
    *b++ = (fee >> 40) & 0xFFU; \ 
    *b++ = (fee >> 32) & 0xFFU; \ 
    *b++ = (fee >> 24) & 0xFFU; \ 
    *b++ = (fee >> 16) & 0xFFU; \ 
    *b++ = (fee >> 8) & 0xFFU; \ 
    *b++ = (fee >> 0) & 0xFFU; \
} while(0) 
// clang-format on

uint8_t namespace[32] = {
    0x01U, 0xEAU, 0xF0U, 0x93U, 0x26U, 0xB4U, 0x91U, 0x15U,
    0x54U, 0x38U, 0x41U, 0x21U, 0xFFU, 0x56U, 0xFAU, 0x8FU,
    0xECU, 0xC2U, 0x15U, 0xFDU, 0xDEU, 0x2EU, 0xC3U, 0x5DU,
    0x9EU, 0x59U, 0xF2U, 0xC5U, 0x3EU, 0xC6U, 0x65U, 0xA0U
};
uint8_t state_key_acc[32] = {
    0x45U, 0x56U, 0x52U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
};

uint8_t first_hook_acc[20] = {
   0xA3U, 0xADU, 0x91U, 0x18U, 0x4BU, 0x1CU, 
   0x57U, 0x1DU, 0x79U, 0xC6U, 0xDCU, 0xC5U, 
   0xC9U, 0x2EU, 0x2EU, 0x58U, 0x3DU, 0xCCU, 
   0x59U, 0x9CU
};

// START OF THE HOOK ACTIVATION -----------------------------------------------------------------------------------------

int64_t hook(uint32_t reserved) {

TRACESTR("vendor_uri_remit.c: Called.");

// ACCOUNT: Hook Account
uint8_t hook_acct[20];
hook_account(hook_acct, 20);

// ACCOUNT: Origin Tx Account
uint8_t otx_acc[20];
otxn_field(otx_acc, 20, sfAccount);

// To know the type of origin txn
int64_t tt = otxn_type();

// If the transaction is outgoing from the hook account, accept it
 if (BUFFER_EQUAL_20(hook_acct, otx_acc) && tt == ttPAYMENT) {
    accept(SBUF("Vendor URI Remit: Outgoing Payment Transaction"), __LINE__);
}


// Configure State Storage Numbers -----------------------------------------------------------------------------------------

// URIL state number
uint64_t ulnum = 0x00000000000F423F;
uint8_t ulnum_buf[8] = {0};
UINT64_TO_BUF(ulnum_buf, ulnum);

// URI state number
uint64_t unum = 0x00000000000F423E;
uint8_t unum_buf[8] = {0};
UINT64_TO_BUF(unum_buf, unum);

// COUNT state number
uint64_t conum = 0x00000000000F423B;
uint8_t conum_buf[8] = {0};
UINT64_TO_BUF(conum_buf, conum);

// Set up the counter
int64_t count = 0;
int8_t hasCount = state(&count, sizeof(count), conum_buf, sizeof(conum_buf));

// Set up count param
uint8_t count_param_buf[8] = {0};  // Use a buffer for fetching the parameter
uint8_t count_key[5] = {'C', 'O', 'U', 'N', 'T'};  // Parameter key

// Fetch the parameter into the buffer
int8_t isCount = otxn_param(count_param_buf, sizeof(count_param_buf), SBUF(count_key));
TRACEHEX(count_param_buf);  // Debug the raw buffer

// Convert the buffer to an integer
uint64_t count_param = UINT64_FROM_BUF(count_param_buf);
TRACEVAR(count_param);  // Log the converted integer

uint8_t num_buf[8];
uint8_t num_key[3] = { 'N', 'U', 'M'};
int8_t isNum = otxn_param(SBUF(num_buf), SBUF(num_key));

uint8_t del_buf[8];
uint8_t del_key[3] = { 'D', 'E', 'L'};
int8_t isDel = otxn_param(SBUF(del_buf), SBUF(del_key));

// Configure URIL and URI ----------------------------------------------------------------
uint8_t uril_buf[8];
uint8_t uril_key[4] = { 'U', 'R', 'I', 'L' };
int8_t isUril = otxn_param(SBUF(uril_buf), SBUF(uril_key));
uint64_t uri_len = UINT64_FROM_BUF(uril_buf);

// URIL state buffer
 uint8_t ulbuf[8]={0};
 
// Check if hook URIL state
int8_t hasUril = state(SBUF(ulbuf), SBUF(ulnum_buf));
uint64_t reconstructed_uril_value = UINT64_FROM_BUF(ulbuf);

uint8_t uri_buffer[256];
int8_t uri_key[3] = { 'U', 'R', 'I' };
int8_t isUri1 = otxn_param(SBUF(uri_buffer), SBUF(uri_key));

//check if has a uril prior to adding uri
if (hasUril < 0 && isUri1 > 0)
rollback(SBUF("Vendor URI Remit: Error: This hook is missing a URIL! Please add a URIL to start building this hook."), __LINE__); 

//fix uri buffer
uri_buffer[0] = reconstructed_uril_value;
int8_t isUri2 = otxn_param(uri_buffer + 1,reconstructed_uril_value, SBUF(uri_key));

// HOOK LOCK -----------------------------------------------------------------------------------------  

// Check if hook_accid and account_field are the same
int equal = 0;
BUFFER_EQUAL(equal, otx_acc, hook_acct, 20);
if(tt == 99 && !equal){
    TRACEVAR(tt);
   rollback(SBUF("Vendor URI Remit: Error: Only the owner of this hook can change its settings!"), 1);
};

// HookOn: Invoke Set COUNT State ----------------------------------------------------------------------------------------

if (tt == 99 && isCount > 0){ 


                      //data         //number key
   #define SBUF(str) (uint32_t)(str), sizeof(str)
if (state_set(SBUF(&count_param), SBUF(conum_buf)) < 0)
		rollback(SBUF("Error: Could not set COUNT state!"), 1);

accept(SBUF("Vendor URI Remit: Set the COUNT state."), __LINE__);

}

// HookOn: Invoke Set URIL State -----------------------------------------------------------------------------------------


if (tt == 99 && isUril > 0){ 


   #define SBUF(str) (uint32_t)(str), sizeof(str)
if (state_set(SBUF(uril_buf), SBUF(ulnum_buf)) < 0)
		rollback(SBUF("Error: could not set the URIL state!"), 1);

accept(SBUF("Vendor URI Remit: Set the URIL state."), __LINE__);

}

// HookOn: Invoke Set URI State -----------------------------------------------------------------------------------------


if (tt == 99 && isUri1 > 0){

TRACEVAR(uri_buffer);	
TRACEVAR(uri_buffer);	

   #define SBUF(str) (uint32_t)(str), sizeof(str)
if (state_set(SBUF(uri_buffer), SBUF(num_buf)) < 0)
		rollback(SBUF("Vendor URI Remit: Error: could not set the URI state!"), 1);

//add to counter and set counter state
 count++;
 TRACEVAR(count);
 if (state_set(SBUF(&count), SBUF(conum_buf)) < 0)
		rollback(SBUF("Vendor URI Remit: Error: could not set the COUNT state!"), 1);

accept(SBUF("Success: Set a URI state."), __LINE__);
}

// HookOn: Invoke Delete State -----------------------------------------------------------------------------------------


if (tt == 99 && isDel > 0){

TRACEHEX(del_buf);

   #define SBUF(str) (uint32_t)(str), sizeof(str)
if (state_set(0,0, SBUF(del_buf)) < 0)
		rollback(SBUF("Vendor URI Remit: Error: could not delete state!"),__LINE__);

 TRACEHEX(count);
//add to counter and set counter state
if(count >= 1){
 count--;
}else {
count = 0;
}

 TRACEHEX(count);
 if (state_set(SBUF(&count), SBUF(conum_buf)) < 0)
		rollback(SBUF("Vendor URI Remit: Error: could not set the COUNT state!"), 1);

accept(SBUF("Vendor URI Remit: Deleted the state."), __LINE__);

}

// // HookOn: Incoming Payment Gateway  -----------------------------------------------------------------------------------------


if (tt == 00){ 

// Get order counter to match the latest slot
    uint8_t counter_buf[8];
    uint8_t counter_key[4] = {0x43U, 0x4EU, 0x54U, 0x00U}; // "CNT"
    uint64_t order_counter = 1; // Default to 1 if not found
    if (state(SBUF(counter_buf), SBUF(counter_key)) >= 0) {
        order_counter = UINT64_FROM_BUF(counter_buf);
    }
    TRACEVAR(order_counter);
    uint8_t slot = (uint8_t)(order_counter % 3 + 1); // Slot 1-3
    state_key_acc[5] = slot; // Update slot dynamically
    TRACEVAR(slot);
    TRACEHEX(state_key_acc);

    // Retrieve stored otxn_acc for the latest slot
    uint8_t ftxn_acc[20];
    int64_t acc_len = state_foreign(SBUF(ftxn_acc), SBUF(state_key_acc), SBUF(namespace), SBUF(hook_acct));
    if (acc_len != 20) {
        TRACESTR("Vendor URI Remit: Warning: No data in primary slot, trying fallbacks.");
        for (int i = 1; i <= 3; i++) {
            if (i != slot) {
                state_key_acc[5] = (uint8_t)i;
                acc_len = state_foreign(SBUF(ftxn_acc), SBUF(state_key_acc), SBUF(namespace), SBUF(hook_acct));
                if (acc_len == 20) {
                    TRACESTR("Vendor URI Remit: Found data in fallback slot.");
                    break;
                }
            }
        }
        if (acc_len != 20)
            rollback(SBUF("Vendor URI Remit: Error: Failed to retrieve valid origin account."), __LINE__);
    }
    if (!BUFFER_EQUAL_20(otx_acc, ftxn_acc))
        rollback(SBUF("Vendor URI Remit: Error: Mismatched origin account."), __LINE__);
    TRACEHEX(ftxn_acc);

// // Check if the payment is not from first_hook_acc
// int not_from_first_hook = 0;
// BUFFER_EQUAL(not_from_first_hook, otx_acc, first_hook_acc, 20);
//     if (!not_from_first_hook) {
//         accept(SBUF("Vendor URI Remit: Payment not from Merchant Skipping..."), __LINE__);
//     }

// // Retrieve otxn_acc from first Hook's state
// uint8_t ftxn_acc[20];
// int64_t state_result = state_foreign(SBUF(ftxn_acc), SBUF(state_key), SBUF(namespace), SBUF(first_hook_acc));
// if (state_result != 20) {
//     rollback(SBUF("Vendor URI Remit: Error: Failed to retrieve foreign state."), __LINE__);
// }


//check the 
if(count <= 0){
 accept(SBUF("Vendor URI Remit: Error: This hook has no more URI tokens to mint. Contact the owner of this hook for more information!"), __LINE__);   
}

// Configure the fixed cost in XAH
    uint64_t cost_xah = 10; // Fixed cost of 10 XAH
    uint64_t cost_drops = cost_xah * 1000000; // Convert XAH to drops
    double min_xah = cost_xah * 0.9;
    double max_xah = cost_xah * 1.1;
    uint64_t min_drops = (uint64_t)(min_xah * 1000000.0);
    uint64_t max_drops = (uint64_t)(max_xah * 1000000.0);

    // Fetch the sent amount
    unsigned char amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    if (amount_len != 8)
        rollback(SBUF("Vendor URI Remit: Error: This hook only accepts XAH!"), __LINE__);
    int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    double xah_amount = (double)otxn_drops / 1000000.0;
    TRACEVAR(xah_amount);
    TRACEVAR(cost_xah);
    TRACEVAR(cost_drops);
    TRACEVAR(otxn_drops);
    TRACEVAR(min_drops);
    TRACEVAR(max_drops);

    // Count valid multiples
    uint64_t multi_count = 0;
    int64_t remaining_drops = otxn_drops;
    for (int64_t i = 0; GUARD(10), i < 10; i++) {
        if (remaining_drops < min_drops) break; // No more valid multiples possible
        if (remaining_drops >= min_drops && remaining_drops <= max_drops) {
            multi_count++;
        }
        remaining_drops -= cost_drops; // Subtract fixed cost
    }
    TRACEVAR(multi_count);

    if (multi_count == 0)
        rollback(SBUF("Vendor URI Remit: Error: No valid multiples found within 10% tolerance."), __LINE__);
        
// HookOn: URI Token Mint  -----------------------------------------------------------------------------------------

if (tt == 00){ 

// Convert number to a byte buffer
uint8_t count_buf[8] = {0};
UINT64_TO_BUF(count_buf, count);
TRACEHEX(count_buf);



// STATE URI BUFFER
 uint8_t suri[256];
 suri[0] = reconstructed_uril_value; 
if (state(SBUF(suri), SBUF(count_buf)) < 0)
		rollback(SBUF("Vendor URI Remit: Error: Could not check state! namespace maybe misconfigured!"), 1);
TRACESTR(suri);



// Prepare TX
PREPARE_REMIT_TXN(hook_acct, ftxn_acc, suri, reconstructed_uril_value);

    // TXN: Emit/Send Txn
    uint8_t emithash[32];
    int64_t emit_result = emit(SBUF(emithash), txn, BYTES_LEN + reconstructed_uril_value + 1);
    if (emit_result > 0)
    {
       // delete state
         #define SBUF(str) (uint32_t)(str), sizeof(str)
         if (state_set(0,0, SBUF(count_buf)) < 0)
		  rollback(SBUF("Vendor URI Remit: Error: could not delete state!"),__LINE__);

      //add to counter and set counter state
      count--;
      TRACEVAR(count);
      if (state_set(SBUF(&count), SBUF(conum_buf)) < 0)
	  rollback(SBUF("Vendor URI Remit: Error: could not set the COUNT state!"), 1);


        accept(SBUF("Vendor URI Remit: Success:Tx emitted success."), __LINE__);
    }
    rollback(SBUF("Vendor URI Remit: Error: Tx emitted failure."), __LINE__);

    }
}

//final gaurds

    _g(1,1);
    return 0;
}