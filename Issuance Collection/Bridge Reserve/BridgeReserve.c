// Bridge Reserve Hook (BRH) - Xahau HandyHook Collection
// Author: @Handy_4ndy
//
// Description:
//   This hook enables cross-chain token bridging by implementing a burn-to-mint
//   mechanism with reserve backing for cross-chain transfers. When tokens are sent 
//   to the issuer (burned), equivalent amounts are minted to a reserve account.
//
// Hook Parameters:
//   'R_ACC' (20 bytes): Reserve account for cross-chain transfer backing.
//   'IOU' (20 bytes): Currency code to filter and bridge.
//
// Bridge Mechanism:
//   - Processes incoming IOU payments to the hook account (issuer)
//   - Tokens sent to issuer are automatically burned by the ledger
//   - Mints equivalent amount to reserve account for cross-chain backing
//   - Maintains 1:1 burn-to-mint ratio for cross-chain integrity
//   - Logs bridge events for cross-chain monitoring via Evernode
//
// Usage:
//   - Users send tokens to the hook account to initiate bridge burn
//   - Hook validates currency and issuer match configuration
//   - Equivalent tokens minted to reserve account for transfer backing
//   - Bridge event logged for off-ledger monitoring systems
//
// Development Contribution:
//   - A development contribution of 0.05 XAH is charged per successful bridge operation, sent to a predefined account.
//**************************************************************

#include "hookapi.h"

#define FLIP_ENDIAN(n) ((uint32_t) (((n & 0xFFU) << 24U) | \
                                   ((n & 0xFF00U) << 8U) | \
                                 ((n & 0xFF0000U) >> 8U) | \
                                ((n & 0xFF000000U) >> 24U)))

#define DONE(x) accept(SBUF("BRH :: Success :: " x), __LINE__)
#define NOPE(x) rollback(SBUF("BRH :: Error :: " x), __LINE__)
#define GUARD(maxiter) _g(__LINE__, (maxiter) + 1)

#ifndef FLIP_ENDIAN
#  define FLIP_ENDIAN(x) __builtin_bswap32(x)
#endif

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

#define FLS_OUT    (txn + 15U) 
#define LLS_OUT    (txn + 21U) 
#define FEE_OUT    (txn + 75U) 
#define AMOUNT_OUT (txn + 25U)
#define HOOK_ACC   (txn + 120U)
#define RESERVE_ACC (txn + 142U)
#define EMIT_OUT   (txn + 162U)
#define CURRENCY_OFFSET 8U

static uint8_t dev_contribution_acc[20] = {0xCCU, 0x41U, 0x96U, 0xC1U, 0xF2U, 0x34U, 0xDBU, 0xAAU, 0x06U, 0x13U, 0x0FU, 0xAAU, 0xF5U, 0xD2U, 0x8CU, 0x53U, 0x77U, 0xA6U, 0xFBU, 0xCAU};
#define DEV_CONTRIBUTION_DROPS 50000

int64_t hook(uint32_t reserved)
{
    TRACESTR("BRH:: Bridge Reserve Hook :: Called");

    if (otxn_type() != ttPAYMENT)
        DONE("Non-PAYMENT transaction passed through.");

    uint8_t hook_acc[20];
    if (hook_account(SBUF(hook_acc)) != 20)
        NOPE("Failed to get hook account.");

    uint8_t otxn_acc[20];
    if (otxn_field(SBUF(otxn_acc), sfAccount) != 20)
        NOPE("Failed to get origin account.");

    if (BUFFER_EQUAL_20(hook_acc, otxn_acc))
        DONE("Outgoing transaction passed through.");

    uint8_t currency[20];
    if (hook_param(SBUF(currency), "IOU", 3) != 20)
        NOPE("Misconfigured. Currency not set as Hook Parameter.");

    uint8_t reserve_acc[20];
    if (hook_param(SBUF(reserve_acc), "R_ACC", 5) != 20)
        NOPE("Misconfigured. Reserve account not set as Hook Parameter.");

    uint8_t dst_acc_in[20];
    int64_t dlen = otxn_field(SBUF(dst_acc_in), sfDestination);
    if (dlen != 20 || !BUFFER_EQUAL_20(hook_acc, dst_acc_in))
        DONE("Not destined to this account; passed through.");

    uint8_t amount_buf[48];
    int64_t amount_len = otxn_field(SBUF(amount_buf), sfAmount);

    if (amount_len == 8)
        DONE("Native XAH payment; passed through.");

    if (amount_len != 48)
        DONE("Invalid amount format; passed through.");

    if (!BUFFER_EQUAL_20(amount_buf + CURRENCY_OFFSET, currency))
        DONE("Non-matching currency; passed through.");

    if (!BUFFER_EQUAL_20(amount_buf + 28, hook_acc))
        DONE("Not our issued currency; passed through.");

    int64_t otx_slot = otxn_slot(0);
    if (otx_slot < 0) 
        NOPE("Slot otxn failed");

    int64_t amt_slot = slot_subfield(otx_slot, sfAmount, 0);
    if (amt_slot < 0) 
        NOPE("Slot amount failed");

    int64_t amount_xfl = slot_float(amt_slot);

    hook_account(HOOK_ACC, 20);

    for (int i = 0; GUARD(20), i < 20; ++i)
        RESERVE_ACC[i] = reserve_acc[i];

    if (float_sto(AMOUNT_OUT, 49, currency, 20, HOOK_ACC, 20, amount_xfl, sfAmount) < 0)
        NOPE("Failed to serialize reserve amount.");

    etxn_reserve(2);
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
    if (emit(SBUF(emithash), SBUF(txn)) != 32)
        NOPE("Failed to emit reserve transaction.");

    uint8_t contribution_txn[PREPARE_PAYMENT_SIMPLE_SIZE];
    PREPARE_PAYMENT_SIMPLE(contribution_txn, DEV_CONTRIBUTION_DROPS, dev_contribution_acc, 0, 0);
    
    uint8_t contribution_emithash[32];
    if (emit(SBUF(contribution_emithash), SBUF(contribution_txn)) != 32)
        TRACESTR("BRH :: Warning :: Failed to emit development contribution payment.");

    DONE("Tokens burned and reserve minted successfully.");
    _g(1,1);
    return 0;
}
