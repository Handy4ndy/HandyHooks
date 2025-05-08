# ForwardSplit Hook

This hook is set on a payment to a configuration hook, that forwards a percentage of all XAH payments. **1% is reserved in the hook account to self-fund transactions.**

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
3. Retrieves the transaction amount and ensures it is in XAH (8 bytes).
4. Reserves space for three emitted transactions.
5. Converts the transaction amount to XFL (floating-point format).
6. Calculates the percentages for each account in XFL format.
7. Calculates the forwarded amounts for each account using floating-point multiplication.
8. Ensures the remaining amount in the hook account is at least 1% of the total transaction amount.
9. Converts the forwarded amounts back to integer drops and logs them in XAH format.
10. Prepares and emits the payment transactions for each account.
11. Accepts the transaction with a success message if all emissions were successful.

## Error Handling

The hook includes error handling for the following scenarios:

- If any of the required accounts (Main, Second, Third) are not set.
- If any of the required amounts (Main Amount, Secondary Amount, Third Amount) are not set.
- If the total percentage of the amounts exceeds 99%, ensuring the hook is always funded.
- If any of the accounts are matching, to avoid forwarding to the same account.
- If the remaining amount in the hook account is less than 1% of the total transaction amount.
- If any errors occur during floating-point calculations or transaction emissions.

## Example Usage

To use this hook, set the parameters as follows:

- **Main Account**:   "M" (HEX_AccountID)
- **Second Account**: "S" (HEX_AccountID)
- **Third Account**:  "T" (HEX_AccountID)
- **Main Amount**:      "MA" (HEX_FLIPPED)
- **Secondary Amount**: "SA" (HEX_FLIPPED)
- **Third Amount**:     "TA" (HEX_FLIPPED)

Ensure the total percentage of the amounts does not exceed 99%.

## Additional Information

This is a modification of the following tutorials, produced by Satish from XRPL Labs:  

- [https://www.youtube.com/watch?v=KdOHr6L0Ss4&t=380s](https://www.youtube.com/watch?v=KdOHr6L0Ss4&t=380s)  
- [https://www.youtube.com/watch?v=wM19E3whV0c](https://www.youtube.com/watch?v=wM19E3whV0c)  

GitHub Repository: [https://github.com/technotip/HookTutorials](https://github.com/technotip/HookTutorials)

For more details, refer to the source code and comments within the hook implementation.

This README provides an overview of the ForwardSplit hook, its workflow, error handling, and example usage. For further assistance, refer to the provided tools and documentation links.
