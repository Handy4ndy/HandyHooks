#include "hookapi.h"
#include <stdint.h>

#define GUARD(maxiter) _g(__LINE__, (maxiter)+1)

// Account builder for tx
#define ACCOUNT_TO_BUF(buf_raw, i)\
{\
    unsigned char* buf = (unsigned char*)buf_raw;\
    *(uint64_t*)(buf + 0) = *(uint64_t*)(i +  0);\
    *(uint64_t*)(buf + 8) = *(uint64_t*)(i +  8);\
    *(uint32_t*)(buf + 16) = *(uint32_t*)(i + 16);\
}

// URI buffer builder for tx
#define URI_TO_BUF(buf_raw, uri, len)\
{\
    unsigned char* buf = (unsigned char*)buf_raw;\
    for (int i = 0; GUARD(79), i < 32; ++i) \
        *(((uint64_t*)buf) + i) = *(((uint64_t*)uri) + i); \
    buf[len + 1] += 0xE1U; \
}

// clang-format off tx sizing
uint8_t txn[600000] =
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

// TX BUILDER
#define BYTES_LEN 238U
#define FLS_OUT (txn + 20U)
#define LLS_OUT (txn + 26U)
#define DTAG_OUT (txn + 14U)
#define FEE_OUT (txn + 31U)
#define HOOK_ACC (txn + 76U)
#define OTX_ACC (txn + 98U)
#define URI_OUT (txn + 237U)
#define EMIT_OUT (txn + 118U)

// clang-format off prepare remit tx
#define PREPARE_REMIT_TXN(account_buffer, dest_buffer, uri_buffer, uri_len) do { \ 
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
uint8_t state_key[32] = {
    0x45U, 0x56U, 0x52U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
};

uint8_t first_hook_acc[20] = {
   0xB9U, 0x59U, 0xD6U, 0xACU, 0x9DU, 0x15U, 0x89U, 0x17U, 
   0xECU, 0x26U, 0x9EU, 0xACU, 0x05U, 0x70U, 0x19U, 0x07U, 
   0x4FU, 0x56U, 0xD6U, 0x39U
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
rollback(SBUF("Error: This hook is missing a URIL! Please add a URIL to start building this hook."), __LINE__); 

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

accept(SBUF("Success: Set the COUNT state."), __LINE__);

}

// HookOn: Invoke Set URIL State -----------------------------------------------------------------------------------------


if (tt == 99 && isUril > 0){ 


   #define SBUF(str) (uint32_t)(str), sizeof(str)
if (state_set(SBUF(uril_buf), SBUF(ulnum_buf)) < 0)
		rollback(SBUF("Error: could not set the URIL state!"), 1);

accept(SBUF("Success: Set the URIL state."), __LINE__);

}

// HookOn: Invoke Set URI State -----------------------------------------------------------------------------------------


if (tt == 99 && isUri1 > 0){

// TRACEVAR(uri_buffer);	
// TRACEVAR(uri_buffer);	

   #define SBUF(str) (uint32_t)(str), sizeof(str)
if (state_set(SBUF(uri_buffer), SBUF(num_buf)) < 0)
		rollback(SBUF("Error: could not set the URI state!"), 1);

//add to counter and set counter state
 count++;
//  TRACEVAR(count);
 if (state_set(SBUF(&count), SBUF(conum_buf)) < 0)
		rollback(SBUF("Error: could not set the COUNT state!"), 1);

accept(SBUF("Success: Set a URI state."), __LINE__);
}

// HookOn: Invoke Delete State -----------------------------------------------------------------------------------------

if (tt == 99 && isDel > 0){

TRACEHEX(del_buf);

   #define SBUF(str) (uint32_t)(str), sizeof(str)
if (state_set(0,0, SBUF(del_buf)) < 0)
		rollback(SBUF("Error: could not delete state!"),__LINE__);

 TRACEHEX(count);
//add to counter and set counter state
if(count >= 1){
 count--;
}else {
count = 0;
}

 TRACEHEX(count);
 if (state_set(SBUF(&count), SBUF(conum_buf)) < 0)
		rollback(SBUF("Error: could not set the COUNT state!"), 1);

accept(SBUF("Success: Deleted the state."), __LINE__);

}


// // HookOn: Incoming Payment Gateway  -----------------------------------------------------------------------------------------


if (tt == 00){ 


// Check if the payment is not from first_hook_acc
int not_from_first_hook = 0;
BUFFER_EQUAL(not_from_first_hook, otx_acc, first_hook_acc, 20);
if (!not_from_first_hook) {
    accept(SBUF("Vendor URI Remit: Payment not from Merchant Skipping..."), __LINE__);
}

// Retrieve otxn_acc from first Hook's state
uint8_t ftxn_acc[20];
int64_t state_result = state_foreign(SBUF(ftxn_acc), SBUF(state_key), SBUF(namespace), SBUF(first_hook_acc));
if (state_result != 20) {
    rollback(SBUF("Vendor URI Remit: Error: Failed to retrieve foreign state."), __LINE__);
}

// TRACEHEX(ftxn_acc);


// Configure the fixed cost in XAH
    uint64_t cost_xah = 10; // Fixed cost of 10 XAH
    uint64_t cost_drops = cost_xah * 1000000; // Convert XAH to drops
    double min_xah = cost_xah * 0.9; // 10% tolerance below
    double max_xah = cost_xah * 1.1; // 10% tolerance above
    uint64_t min_drops = (uint64_t)(min_xah * 1000000.0);
    uint64_t max_drops = (uint64_t)(max_xah * 1000000.0);

    // Get the incoming payment amount
    unsigned char amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
    if (amount_len != 8)
        rollback(SBUF("Vendor URI Remit: Error: This hook only accepts XAH!"), __LINE__);
    int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
    
    double xah_amount = (double)otxn_drops / 1000000.0;
    // TRACEVAR(xah_amount);
    // TRACEVAR(cost_xah);
    TRACEVAR(cost_drops);
    TRACEVAR(otxn_drops);
    // TRACEVAR(min_drops);
    // TRACEVAR(max_drops);

    // Guard: Check if payment is a valid multiple within ±10% tolerance
    uint64_t multi_count = (uint64_t)((xah_amount / cost_xah) + 0.5); // round to nearest
    double total_cost_xah = multi_count * cost_xah;
    double min_total_xah = total_cost_xah * 0.9;
    double max_total_xah = total_cost_xah * 1.1;

    TRACEVAR(multi_count);
    // TRACEVAR(total_cost_xah);
    // TRACEVAR(min_total_xah);
    // TRACEVAR(max_total_xah);



// HookOn: URI Token Mint
if (tt == ttPAYMENT) {

    //  // Calculate what you can actually emit
    // uint64_t emit_count = multi_count;
    // if (emit_count > count) emit_count = count;
    // if (emit_count > 10) emit_count = 10;
    
    // Calculate the number of tokens to emit (cannot exceed count or 10)
    uint64_t emit_count = multi_count;
    if (emit_count > count) emit_count = count;
    if (emit_count > 10) emit_count = 10;
    if (emit_count == 0) {
        rollback(SBUF("Vendor URI Remit: Error: No URI tokens available."), __LINE__);
    }
    
    // Reserve only what you'll emit
    etxn_reserve(emit_count);


    // Emit up to emit_count transactions, always using the current top count and decrementing after each emission
    
    uint64_t actually_emitted = 0;
    uint64_t state_key_idx = count;
    for (uint64_t i = 0; GUARD(10), i < emit_count && state_key_idx > 0; ++i, --state_key_idx) {
        uint8_t count_buf[8] = {0};
        UINT64_TO_BUF(count_buf, state_key_idx);
        TRACEHEX(count_buf);

        // Retrieve the next URI from state
        uint8_t suri[256];
        suri[0] = reconstructed_uril_value;
        int state_result = state(SBUF(suri), SBUF(count_buf));
        if (state_result < 0) {
            continue; // skip missing state, but always use a unique state key
        }
        TRACESTR(suri);

        // Prepare and emit the transaction
        PREPARE_REMIT_TXN(hook_acct, ftxn_acc, suri, reconstructed_uril_value);
        uint8_t emithash[32];
        int64_t emit_result = emit(SBUF(emithash), txn, BYTES_LEN + reconstructed_uril_value + 1);
        if (emit_result <= 0)
            rollback(SBUF("Vendor URI Remit: Error: Tx emitted failure."), __LINE__);

        // Clear the specific URI from state after successful emission
        if (state_set(0, 0, SBUF(count_buf)) < 0)
            rollback(SBUF("Vendor URI Remit: Error: Could not delete state!"), __LINE__);

        actually_emitted++;
        count--; // only decrement count when a valid emission occurs
    }

    TRACEVAR(count);
    if (state_set(SBUF(&count), SBUF(conum_buf)) < 0)
        rollback(SBUF("Vendor URI Remit: Error: Could not set the COUNT state!"), __LINE__);

    if (actually_emitted == 0) {
        rollback(SBUF("Vendor URI Remit: Error: No valid URI tokens found for emission."), __LINE__);
    }

    accept(SBUF("Vendor URI Remit: Success: All Tx emitted successfully."), __LINE__);
}
}

    GUARD(1);
    return 0;
}