//**************************************************************
// Admin Issuance Hook - Xahau HandyHook Collection
// Author: @Handy_4ndy
//
// Description:
//   This hook enables controlled, dynamic token issuance through invoke
//   transactions with automatic treasury allocation. Only whitelisted
//   accounts can trigger issuance, with 5% automatically going to treasury.
//
// Hook Parameters (20 bytes each):
//   'IOU' (20 bytes): Currency code to be issued by this hook.
//   'W_ACC' (20 bytes): Whitelisted account ID that can trigger issuance.
//   'T_ACC' (20 bytes): Treasury account ID that receives 5% of all issuances.
//
// Transaction Parameters:
//   'AMT' (8 bytes): Amount of tokens to issue (big-endian uint64).
//   'DEST' (20 bytes): Destination account to receive the tokens.
// 
// Usage:
//   - Whitelisted account sends Invoke transaction with 'AMT' and 'DEST' parameters.
//   - Hook emits two transactions: main amount to DEST, 5% to treasury (T_ACC).
//   - Non-invoke transactions and unauthorized invokes pass through unchanged.
//
// Development Contribution:
//   - A development contribution of 0.05 XAH is charged per issuance, sent to a predefined account.
//**************************************************************

#include "hookapi.h"

#define FLIP_ENDIAN(n) ((uint32_t) (((n & 0xFFU) << 24U) | \
                                   ((n & 0xFF00U) << 8U) | \
                                 ((n & 0xFF0000U) >> 8U) | \
                                ((n & 0xFF000000U) >> 24U)))

#define DONE(x) accept(SBUF(x), __LINE__)
#define NOPE(x) rollback(SBUF(x), __LINE__)
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
    /* 49, 25  amount               */   0x61U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U,                         
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

static uint8_t dev_contribution_acc[20] = {0xCCU, 0x41U, 0x96U, 0xC1U, 0xF2U, 0x34U, 0xDBU, 0xAAU, 0x06U, 0x13U, 0x0FU, 0xAAU, 0xF5U, 0xD2U, 0x8CU, 0x53U, 0x77U, 0xA6U, 0xFBU, 0xCAU};
#define DEV_CONTRIBUTION_DROPS 50000

int64_t hook(uint32_t reserved) {

    TRACESTR("Admin Issuance: called");

    if (otxn_type() != 99)
        DONE("Non-INVOKE transaction passed through.");

    uint8_t hook_acc[20];
    if (hook_account(SBUF(hook_acc)) != 20)
        NOPE("Failed to get hook account.");

    uint8_t otxn_acc[20];
    if (otxn_field(SBUF(otxn_acc), sfAccount) != 20)
        NOPE("Failed to get origin account.");

    if (BUFFER_EQUAL_20(hook_acc, otxn_acc))
        DONE("Outgoing invoke transaction passed through.");

    uint8_t currency[20];
    if(hook_param(SBUF(currency), "IOU", 3) != 20)
        NOPE("Misconfigured. Currency not set as Hook Parameter.");

    uint8_t invoke_acc[20];
    if(hook_param(SBUF(invoke_acc), "W_ACC", 5) != 20)
        NOPE("Misconfigured. Whitelist account not set as Hook Parameter.");    

    uint8_t treasury_acc[20];
    if(hook_param(SBUF(treasury_acc), "T_ACC", 5) != 20)
        NOPE("Misconfigured. Treasury account not set as Hook Parameter.");    

    if (!BUFFER_EQUAL_20(otxn_acc, invoke_acc)) 
        DONE("Invoke from non-whitelisted account passed through.");

    uint8_t amount_buf[8];
    if(otxn_param(SBUF(amount_buf), "AMT", 3) != 8)
        NOPE("Misconfigured. Amount to issue not passed as otxn parameter.");            

    uint8_t dest_acc[20];
    if(otxn_param(SBUF(dest_acc), "DEST", 4) != 20)
        NOPE("Misconfigured. Destination account not passed as otxn parameter.");            
 
    uint64_t issued_amount = UINT64_FROM_BUF(amount_buf);
    if (issued_amount == 0)
        NOPE("Invalid main amount - must be positive.");

    uint64_t treasury_amount = issued_amount / 20;
    if (treasury_amount == 0)
        treasury_amount = 1;

    int64_t amount_xfl = float_set(0, issued_amount);
    int64_t treasury_amount_xfl = float_set(0, treasury_amount);

    hook_account(HOOK_ACC, 20);

    for (int i = 0; GUARD(20), i < 20; ++i)
        DEST_ACC[i] = dest_acc[i];

    if(float_sto(AMOUNT_OUT, 49, currency, 20, HOOK_ACC, 20, amount_xfl, sfAmount) < 0) 
        NOPE("Wrong AMT - < xlf 8b req amount, 20b currency, 20b issuer >");  

    etxn_reserve(3);
    uint32_t fls = (uint32_t)ledger_seq() + 1;
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

    uint8_t emithash[32]; 
    if(emit(SBUF(emithash), SBUF(txn)) != 32)
        NOPE("Failed To Emit main transaction.");    

    uint8_t treasury_txn[278];
    for (int i = 0; GUARD(278), i < 278; ++i)
        treasury_txn[i] = txn[i];

    uint8_t *treasury_amount_out = treasury_txn + 25U;
    uint8_t *treasury_dest_acc = treasury_txn + 142U;
    uint8_t *treasury_emit_out = treasury_txn + 162U;

    if(float_sto(treasury_amount_out, 49, currency, 20, HOOK_ACC, 20, treasury_amount_xfl, sfAmount) < 0) 
        NOPE("Failed to serialize treasury amount.");  

    for (int i = 0; GUARD(20), i < 20; ++i)
        treasury_dest_acc[i] = treasury_acc[i];

    etxn_details(treasury_emit_out, 116U);

    uint8_t treasury_emithash[32]; 
    if(emit(SBUF(treasury_emithash), SBUF(treasury_txn)) != 32)
        NOPE("Failed to emit treasury transaction.");    

    uint8_t contribution_txn[PREPARE_PAYMENT_SIMPLE_SIZE];
    PREPARE_PAYMENT_SIMPLE(contribution_txn, DEV_CONTRIBUTION_DROPS, dev_contribution_acc, 0, 0);
    
    uint8_t contribution_emithash[32];
    if (emit(SBUF(contribution_emithash), SBUF(contribution_txn)) != 32)
        TRACESTR("Admin Issuance :: Warning :: Failed to emit development contribution payment.");

    DONE("Admin Issuance: tokens issued to destination + 5% to treasury successfully.");   
    _g(1,1);
    return 0;    
}