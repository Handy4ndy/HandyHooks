# 🎉 BirthdayCardHook – Celebrate Xahau's 2nd Anniversary! 🎂

**Part of the HandyHooks collection**

Welcome to the **BirthdayCardHook** – a special Xahau hook for the community to celebrate Xahau’s second anniversary!  
Leave your message on-chain for all to see. Anyone can add a message, and only the hook owner can remove them.  
This is open, fun, and a perfect example of community-powered, on-chain celebration!

---

## ✨ Features

- **Open Message Board:** Anyone can add a birthday wish to Xahau’s card!
- **Owner-Only Moderation:** Only the hook owner can delete messages.
- **On-Chain Storage:** Messages are stored forever in the hook’s state.
- **Easy to Use:** Just send an Invoke transaction with your message.
- **Automatic Counting:** Tracks the total number of messages.

---

## 🚀 How It Works

1. **Add a Message:**  
   Anyone can send a message using the `MSG` parameter.
2. **Delete a Message:**  
   Only the hook owner can delete a message using the `DEL` parameter.

---

## 📝 Parameters

| Parameter | Size      | Format             | Description                                 |
|-----------|-----------|--------------------|---------------------------------------------|
| `MSG`     | Variable  | UTF-8 text (HEX)   | The birthday message to store (up to 1024 bytes) |
| `DEL`     | 8 bytes   | Big-endian uint64  | The message number to delete (owner only)   |

---

## 🗃️ State Management

| State Key         | Size    | Description                                   |
|-------------------|---------|-----------------------------------------------|
| `CNT`             | 8 bytes | Counter tracking total number of messages     |
| `{message_number}`| Variable| Individual message content, keyed by number   |

---

## 🛠️ Example Transactions

### Add a Message

```json
{
  "TransactionType": "Invoke",
  "Account": "rAnyXahauUser...",
  "Destination": "rBirthdayCardOwner...",
  "HookParameters": [
    {
      "HookParameter": {
        "HookParameterName": "4D5347",
        "HookParameterValue": "486170707920426972746864617921"
      }
    }
  ]
}
```

### Delete a Message (Owner Only)

```json
{
  "TransactionType": "Invoke",
  "Account": "rBirthdayCardOwner...",
  "Destination": "rBirthdayCardOwner...",
  "HookParameters": [
    {
      "HookParameter": {
        "HookParameterName": "44454C",
        "HookParameterValue": "0000000000000001"
      }
    }
  ]
}
```

---

## 🛡️ Security & Access

- **Anyone Can Add:** Open to all Xahau users!
- **Owner-Only Delete:** Only the hook owner can remove messages.
- **Invoke Only:** Only processes invoke transactions.
- **Parameter Validation:** Ensures correct format and size.

---

## 🎯 Success & Error Messages

- **Success:**  
  - `"Success: Message added to birthday card!"`
  - `"Success: Message deleted from birthday card!"`
- **Errors:**  
  - Only owner can delete
  - Must be an Invoke transaction
  - State update failures
  - Invalid operation

---

## 🎈 Use Cases

- **Anniversary Celebrations:** Collect birthday wishes for Xahau!
- **Community Boards:** Open, moderated message boards.
- **Event Guestbooks:** On-chain guestbooks for special occasions.
- **Learning:** Great example for hook developers.

---

## 🧰 Tools & Resources

- [HEX Visualizer](https://transia-rnd.github.io/xrpl-hex-visualizer/) – Convert your message to HEX
- [XRPL Win Raw Transaction Sender](https://xahau.xrplwin.com/tools/tx/raw) – Submit your message
- [Xahau Explorer](https://xahau-testnet.xrplwin.com/) – View your message on-chain

---

## 🎉 Join the Celebration!

1. **Write your message.**
2. **Convert it to HEX** using the HEX Visualizer.
3. **Send it** using the Raw Transaction Sender.
4. **See it on-chain** and be part of Xahau’s history!

**Happy 2nd Birthday, Xahau! 🥳**