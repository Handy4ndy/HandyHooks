//------------------------------------------------------------------------------
/*
  Forward incoming EVR (IOU) to the owner address.
  - Ignores native XAH
  - Ignores non-EVR IOUs
  - Mirrors issuer/currency and amount (same value)
  - Loop guard: ignore if sender == forward address
  - Optional issuer pin (disabled by default)
*/
//==============================================================================

#include "hookapi.h"

#ifndef FLIP_ENDIAN
#  define FLIP_ENDIAN(x) __builtin_bswap32(x)
#endif

// ======= REQUIRED: your owner r-address ======================================
#define FORWARD_RADDRESS "rhNKc3A4DarX2PZqp2jU1xqFtF2bJKWUDb"

// ======= OPTIONAL: pin EVR issuer (20-byte AccountID in HEX) =================
// Leave undefined to accept EVR from ANY issuer and mirror it.
// Example: #define EVR_ISSUER_HEX "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
// #define EVR_ISSUER_HEX ""

// --- IOU skeleton for emitted Payment (we’ll fill fields and emit) -----------
// clang-format off
uint8_t txn_iou[283] =
{
/* size,upto */
/*   3,  0 */   0x12U, 0x00U, 0x00U,                                                            /* tt = Payment */
/*   5,  3 */   0x22U, 0x80U, 0x00U, 0x00U, 0x00U,                                              /* flags = tfCanonical */
/*   5,  8 */   0x24U, 0x00U, 0x00U, 0x00U, 0x00U,                                              /* sequence = 0 */
/*   5, 13 */   0x99U, 0x99U, 0x99U, 0x99U, 0x99U,                                              /* DestinationTag placeholder */
/*   6, 18 */   0x20U, 0x1AU, 0x00U, 0x00U, 0x00U, 0x00U,                                       /* FirstLedgerSequence */
/*   6, 24 */   0x20U, 0x1BU, 0x00U, 0x00U, 0x00U, 0x00U,                                       /* LastLedgerSequence */
/*  49, 30 */   0x61U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U,                         /* Amount (IOU) */
/*   9, 79 */   0x68U, 0x40U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,                   /* Fee (float) */
/*  35, 88 */   0x73U, 0x21U, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* SigningPubKey empty */
/*  22,123 */   0x81U, 0x14U, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,                           /* Account (hook account) */
/*  22,145 */   0x83U, 0x14U, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,                           /* Destination (forward account) */
/* 116,167 */                                                                                    /* emit details */
};
// clang-format on

// TX field pointers for IOU template
#define FLS_OUT     (txn_iou + 20U)
#define LLS_OUT     (txn_iou + 26U)
#define DTAG_OUT    (txn_iou + 14U)
#define AMOUNT_OUT  (txn_iou + 30U)
#define FEE_OUT     (txn_iou + 80U)
#define HOOK_ACC    (txn_iou + 125U)
#define OTX_ACC     (txn_iou + 147U)
#define EMIT_OUT    (txn_iou + 167U)

#define CURRENCY_OFFSET 8U

// Fee writer as a macro (no extra functions = no hooks-entry-points-neg)
#define SET_FEE_FIELD(ptr, fee) do {           \
    uint8_t* b_ = (ptr);                       \
    *b_++ = 0b01000000 + (((fee) >> 56) & 0x3F); \
    *b_++ = ((fee) >> 48) & 0xFFU;             \
    *b_++ = ((fee) >> 40) & 0xFFU;             \
    *b_++ = ((fee) >> 32) & 0xFFU;             \
    *b_++ = ((fee) >> 24) & 0xFFU;             \
    *b_++ = ((fee) >> 16) & 0xFFU;             \
    *b_++ = ((fee) >>  8) & 0xFFU;             \
    *b_++ = ((fee) >>  0) & 0xFFU;             \
} while(0)

int64_t hook(uint32_t reserved)
{
    TRACESTR("redirect_evr_only: called");

    // Compute owner (forward) AccountID
    uint8_t owner_accid[20];
    util_accid(SBUF(owner_accid), SBUF(FORWARD_RADDRESS));

    // Ensure emitted tx Account field = this hook account
    hook_account((uint32_t)(uintptr_t)HOOK_ACC, 20);

    // Origin tx sender
    uint8_t src_acc[20];
    otxn_field((uint32_t)(uintptr_t)src_acc, 20, sfAccount);

    // Guard A: ignore outgoing (we sent it)
    if (BUFFER_EQUAL_20(HOOK_ACC, src_acc)) {
        static const char m[] = "evr-only: outgoing; ignore";
        accept(SBUF(m), __LINE__); _g(1,1); return 0;
    }

    // Guard B: ignore if sender is already the forward address (loop guard)
    if (BUFFER_EQUAL_20(owner_accid, src_acc)) {
        static const char m[] = "evr-only: sender == forward addr; ignore";
        accept(SBUF(m), __LINE__); _g(1,1); return 0;
    }

    // Ensure incoming (Destination == this account)
    uint8_t dst_acc_in[20];
    int64_t dlen = otxn_field((uint32_t)(uintptr_t)dst_acc_in, 20, sfDestination);
    if (dlen == 20 && !BUFFER_EQUAL_20(HOOK_ACC, dst_acc_in)) {
        static const char m[] = "evr-only: not destined to this account";
        accept(SBUF(m), __LINE__); _g(1,1); return 0;
    }

    // Read Amount
    uint8_t amount_buf[48];
    int64_t amount_len = otxn_field(SBUF(amount_buf), sfAmount);

    // Ignore native XAH
    if (amount_len == 8) {
        static const char m[] = "evr-only: native XAH; ignore";
        accept(SBUF(m), __LINE__); _g(1,1); return 0;
    }

    // EVR currency check (20-byte std currency code at bytes 8..27)
    uint8_t evr_cc[20] = {0};
    evr_cc[12]='E'; evr_cc[13]='V'; evr_cc[14]='R';
    if (!BUFFER_EQUAL_20(amount_buf + CURRENCY_OFFSET, evr_cc)) {
        static const char m[] = "evr-only: non-EVR IOU; ignore";
        accept(SBUF(m), __LINE__); _g(1,1); return 0;
    }

#ifdef EVR_ISSUER_HEX
    // Optional issuer pin (compare issuer bytes 28..47)
    {
        uint8_t issuer[20];
        __builtin_memcpy(issuer, amount_buf + 28, 20);
        static const char hexmap[] = "0123456789abcdef";
        uint8_t pinned[20]; uint32_t ok = 1;
        const char* p = EVR_ISSUER_HEX;
        for (int i = 0; i < 20; ++i) {
            char c1 = p[2*i], c2 = p[2*i+1];
            int8_t n1=-1,n2=-1;
            for (int j=0;j<16;++j){ if (c1==hexmap[j]||c1==(hexmap[j]-32)) n1=j; if (c2==hexmap[j]||c2==(hexmap[j]-32)) n2=j; }
            if (n1<0||n2<0){ ok=0; break; }
            pinned[i]=(uint8_t)((n1<<4)|n2);
        }
        if (!ok || !BUFFER_EQUAL_20(pinned, issuer)) {
            static const char m[] = "evr-only: issuer mismatch; ignore";
            accept(SBUF(m), __LINE__); _g(1,1); return 0;
        }
    }
#endif

    // Fill forward destination in emitted IOU txn
    util_accid((uint32_t)(uintptr_t)OTX_ACC, 20, SBUF(FORWARD_RADDRESS));

    // Reserve one emit
    etxn_reserve(1);

    // Validity window
    uint32_t fls = (uint32_t)ledger_seq() + 1;
    *((uint32_t *)(FLS_OUT)) = FLIP_ENDIAN(fls);
    uint32_t lls = fls + 4;
    *((uint32_t *)(LLS_OUT)) = FLIP_ENDIAN(lls);

    // Slot origin tx -> slot Amount -> XFL value
    int64_t otx_slot = otxn_slot(0);
    if (otx_slot < 0) rollback(SBUF("evr-only: slot otxn failed"), 12);

    int64_t amt_slot = slot_subfield(otx_slot, sfAmount, 0);
    if (amt_slot < 0) rollback(SBUF("evr-only: slot amount failed"), 13);

    int64_t amount_xfl = slot_float(amt_slot);

    // Copy issuer+currency from incoming EVR and store same value
    float_sto(
        (uint32_t)(uintptr_t)AMOUNT_OUT, 49,
        (uint32_t)(uintptr_t)(amount_buf + 8),  20,   // currency (160-bit)
        (uint32_t)(uintptr_t)(amount_buf + 28), 20,   // issuer (AccountID)
        amount_xfl, sfAmount
    );

    // Mirror SourceTag -> DestinationTag if present
    if (otxn_field((uint32_t)(uintptr_t)DTAG_OUT, 4, sfSourceTag) == 4)
        *(DTAG_OUT - 1) = 0x2EU;

    // Fee & emit details
    SET_FEE_FIELD(FEE_OUT, etxn_fee_base(SBUF(txn_iou)));
    etxn_details((uint32_t)(uintptr_t)EMIT_OUT, 116U);

    // Emit IOU payment
    uint8_t emithash[32];
    int64_t r = emit(SBUF(emithash), SBUF(txn_iou));
    if (r != 32) rollback(SBUF("evr-only: emit IOU failed"), 14);

    static const char m[] = "evr-only: forwarded EVR IOU";
    accept(SBUF(m), __LINE__); _g(1,1); return 0;
}
