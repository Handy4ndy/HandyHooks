# ForwardSplit Hook

This hook is set on a payment to a configuration hook, that forwards a percentage of all XAH payments. ** 1% is reserved in the hook account to self fund transactions **

## Overview

The hook is designed to forward incoming payments to three predefined accounts:

- **Main Account** - "M" (HEX_AccountID)
- **Second Account** - "S" (HEX_AccountID)
- **Third Account** - "T" (HEX_AccountID)

You can convert r-addresses to account IDs using the following tool:
[https://hooks.services/tools/raddress-to-accountid](https://hooks.services/tools/raddress-to-accountid)

The percentages are set with the following hook parameters:

- **Main Amount** - "MA" (HEX_FLIPPED)
- **Secondary Amount** - "SA" (HEX_FLIPPED)
- **Third Amount** - "TA" (HEX_FLIPPED)

You can convert decimal percentages to hex and flip them using the following tool:
[https://hooks.services/tools/decimal-to-hex-to-fliphex](https://hooks.services/tools/decimal-to-hex-to-fliphex)

## Hook Workflow

The hook performs the following steps:

1. Retrieves the hook account and the originating account of the transaction.
2. Checks if the transaction is outgoing from the hook account and accepts it if true.
3. Retrieves the transaction amount and ensures it is in Xah (8 bytes).
4. Reserves space for three emitted transactions.
5. Calculates the amounts to be forwarded to each account.
6. Prepares the payment transactions for each account.
7. Emits the transactions and checks if they were successful.
8. Accepts the transaction with a success message if all emissions were successful.

## Error Handling

The hook includes error handling for the following scenarios:

- If any of the required accounts (Main, Second, Third) are not set.
- If any of the required amounts (Main Amount, Secondary Amount, Third Amount) are not set.
- If the total percentage of the amounts exceeds 99%, ensuring the hook is always funded.
- If any of the accounts are matching, to avoid forwarding to the same account.

## Example Usage

To use this hook, set the parameters as follows:

- Main Account:   "M" (HEX_AccountID)
- Second Account: "S" (HEX_AccountID)
- Third Account:  "T" (HEX_AccountID)
- Main Amount:      "MA" (HEX_FLIPPED)
- Secondary Amount: "SA" (HEX_FLIPPED)
- Third Amount:     "TA" (HEX_FLIPPED)

Ensure the total percentage of the amounts does not exceed 99%.

## Additional Information

This is a modification of the following tutorial's, produced by Satish from XRPL Labs 

https://www.youtube.com/watch?v=KdOHr6L0Ss4&t=380s
https://www.youtube.com/watch?v=wM19E3whV0c

https://github.com/technotip/HookTutorials

For more details, refer to the source code and comments within the hook implementation.

This README provides an overview of the ForwardSplit hook, its workflow, error handling, and example usage. For further assistance, refer to the provided tools and documentation links.
