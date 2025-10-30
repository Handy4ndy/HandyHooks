<h1 align="center">
 🎂 How to Send a Birthday Message 🎉
</h1>

<p align="center">
  <img src="./XahauAnniversary.png" alt="Xahau 2nd Anniversary" />
</p>

<p align="center">
Let’s make Xahau’s 2nd birthday unforgettable!  
Add your message to the on-chain birthday card in just a few fun steps.
</p>

---

## ✍️ 1. Write Your Message

Type your birthday wish for Xahau!  
_Example:_  
```
Happy 2nd Birthday, Xahau!
```

---

## 🧙 2. Convert Your Message to HEX

Use the [XRPL HEX Visualizer](https://transia-rnd.github.io/xrpl-hex-visualizer/):

- Paste your message into the **Text** field. <br> _Example:_ Happy 2nd Birthday, Xahau!
  
- Copy the HEX string from the **Hex** field. <br> _Example:_ 486170707920326E642042697274686461792C20586168617521
  
---

## 📝 3. Prepare Your Transaction

1. Go to [XRPL Win Raw Transaction Sender](https://xahau.xrplwin.com/tools/tx/raw).
2. Fill in the template below:

- Replace `YOUR_rADDRESS_HERE` with your Xahau address.
- Replace `MESSAGE_HEX_STRING_HERE` with your HEX string.

```json
{
  "TransactionType": "Invoke",
  "Account": "YOUR_rADDRESS_HERE",
  "Destination": "rD1CX9FwgPfCLBmH8k8qvFWxauGMWmWbct",
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

## 🚀 4. Submit Your Message

Sign in with Xaman and submit using your wallet.

---

## 🎊 5. Celebrate!

Your message is now part of Xahau’s on-chain birthday card!  
See your message (and others) in the [Namespace Explorer](https://xahau.xrplwin.com/account/rD1CX9FwgPfCLBmH8k8qvFWxauGMWmWbct/namespaces). <br>
** Your message is stored in the namespace beginning with your AccountID **

---

**Happy Birthday, Xahau! 🥳**
