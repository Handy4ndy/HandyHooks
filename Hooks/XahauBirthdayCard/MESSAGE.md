# How to Send a Birthday Message to Xahau's Second Anniversary Card

Celebrate Xahau's second anniversary by leaving your message on-chain!  
Follow these simple steps to convert your message to HEX and submit it using XRPL Win's Raw Transaction Sender.

---

## 1. Write Your Message

Write your birthday wish or message for Xahau.  
Example:  
```
Happy 2nd Birthday, Xahau! 🎉
```

---

## 2. Convert Your Message to HEX

Use the [XRPL HEX Visualizer](https://transia-rnd.github.io/xrpl-hex-visualizer/) to convert your message:

- Paste your message into the "Text" field.
- Copy the resulting HEX string from the "Hex" field.

---

## 3. Prepare the Transaction JSON

Copy the template below and fill in:

- `YOUR_rADDRESS_HERE` with your Xahau account address.
- `MESSAGE_HEX_STRING_HERE` with your HEX string from step 2.

```json
{
  "TransactionType": "Invoke",
  "Account": "YOUR_rADDRESS_HERE",
  "Destination": "rGe24P5aZckhpfsXSsSwRa68pgtaio4yZw",
  "HookParameters": [
    {
      "HookParameter": {
        "HookParameterName": "4D5347",
        "HookParameterValue": "MESSAGE_HEX_STRING_HERE"
      }
    }
  ],
  "NetworkID": 21337
}
```
---

## 4. Submit Your Message

1. Go to [XRPL Win Raw Transaction Sender](https://xahau.xrplwin.com/tools/tx/raw).
2. Paste your filled-out JSON into the transaction field.
3. Sign in then submit the transaction using your wallet.

---

## 5. Celebrate!

Your message will be stored on-chain as part of Xahau's second birthday card.  
Check [XRPLWin's Namespace Explorer](https://xahau.xrplwin.com/account/rGe24P5aZckhpfsXSsSwRa68pgtaio4yZw/namespaces/FBE697429F16141BC71E3B91F3823641C8DD258DD58BF076241514754954CB8C) to see your message included!

---

**Happy Birthday, Xahau! 🎂**