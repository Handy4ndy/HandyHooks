/**
 * HandyHooks - 017 - Invoke URI Emit.  ***WORK IN PROGRESS MISSING DIGEST***
 * This hook mint's a URI token for any payment recieved.
 * Install on ttPayment & ttInvoke.
 * 
 * Invoke the URIL (URI Length uint64_t).
 * Example hooksBuilder: "URIL": "000000000000004E"
 * Example Mainnet: "5552494C": "000000000000004E"
 * 
 * Invoke the URI (URI string) and NUM (URI Listing number uint64_t).
 * Example hooksBuilder: "URI": "697066733A2F2F626166796265696865706C667076363362366368667635646F62723774703365356D6879346B65666673727A7935676E336B68336D7667646662792F3030303030312E6A736F6E", "NUM": "0000000000000001"
 * 
 * Example Mainnet: "555249": "697066733A2F2F626166796265696865706C667076363362366368667635646F62723774703365356D6879346B65666673727A7935676E336B68336D7667646662792F3030303030312E6A736F6E", "4E554D": "0000000000000001"
 * 
 * Send a payment to the hook account to mint a URI token
 */

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
    for (int i = 0; GUARD(32), i < 32; ++i) \
        *(((uint64_t*)buf) + i) = *(((uint64_t*)uri) + i); \
    buf[len + 1] += 0xE1U; \
}

// clang-format off tx sizing
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
    etxn_reserve(1); \ 
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

// START OF THE HOOK ACTIVATION -----------------------------------------------------------------------------------------

int64_t hook(uint32_t reserved) {

TRACESTR("uri_token_remit.c: Called.");

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
    accept(SBUF("Success: Outgoing Payment Transaction"), __LINE__);
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
   rollback(SBUF("Error: Only the owner of this hook can change its settings!"), 1);
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

TRACEVAR(uri_buffer);	
TRACEVAR(uri_buffer);	

   #define SBUF(str) (uint32_t)(str), sizeof(str)
if (state_set(SBUF(uri_buffer), SBUF(num_buf)) < 0)
		rollback(SBUF("Error: could not set the URI state!"), 1);

//add to counter and set counter state
 count++;
 TRACEVAR(count);
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

//check the 
if(count <= 0){
 rollback(SBUF("Error: This hook has no more URI tokens to mint. Contact the owner of this hook for more information!"), __LINE__);   
}

// fetch the sent Amount
unsigned char amount_buffer[48];
int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);
int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
double xah_amount = (double)otxn_drops / 1000000.0;  // Convert to XRP
TRACEVAR(xah_amount);

//Ensure the payment is XAH
if (amount_len != 8)
{
rollback(SBUF("Error: This hook only accepts XAH!"), __LINE__);
}

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
		rollback(SBUF("Could not check state!"), 1);
TRACESTR(suri);



// Prepare TX
PREPARE_REMIT_TXN(hook_acct, otx_acc, suri, reconstructed_uril_value);

    // TXN: Emit/Send Txn
    uint8_t emithash[32];
    int64_t emit_result = emit(SBUF(emithash), txn, BYTES_LEN + reconstructed_uril_value + 1);
    if (emit_result > 0)
    {
       // delete state
         #define SBUF(str) (uint32_t)(str), sizeof(str)
         if (state_set(0,0, SBUF(count_buf)) < 0)
		  rollback(SBUF("Error: could not delete state!"),__LINE__);

      //add to counter and set counter state
      count--;
      TRACEVAR(count);
      if (state_set(SBUF(&count), SBUF(conum_buf)) < 0)
	  rollback(SBUF("Error: could not set the COUNT state!"), 1);


        accept(SBUF("Success:Tx emitted success."), __LINE__);
    }
    accept(SBUF("Error: Tx emitted failure."), __LINE__);

    }
}

//final gaurds

    _g(1,1);
    return 0;
}