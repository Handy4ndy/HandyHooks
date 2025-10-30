# BirthdayCardHook

**Part of the HandyHooks collection**

A celebratory Xahau hook that allows anyone to leave a message for Xahau's second anniversary—on-chain! Only the hook owner can delete messages. This hook demonstrates open message collection, basic state management, and owner-only moderation.

## Overview

This hook provides on-chain birthday card functionality with the following features:

- **Open Message Board**: Anyone can add a message to Xahau's birthday card
- **Owner-Only Moderation**: Only the hook account owner can delete messages
- **On-Chain Storage**: Messages are stored directly in the hook's state
- **Dynamic Message Management**: Add new messages or delete existing ones by message number
- **Automatic Counter Management**: Tracks the total number of messages stored
- **Simple Invoke Interface**: Uses invoke transactions for all operations

## Hook Parameters

This hook does not require any installation parameters. All functionality is controlled through transaction parameters.

## Transaction Parameters

When invoking the hook, use one of these parameters:

### Adding Messages
| Parameter | Size | Format | Description |
|-----------|------|--------|-------------|
| `MSG` | Variable | UTF-8 text | The birthday message to store (up to 1024 bytes) |

### Deleting Messages
| Parameter | Size | Format | Description |
|-----------|------|--------|-------------|
| `DEL` | 8 bytes | Big-endian uint64 | The message number to delete (owner only) |

## State Management

The hook manages the following state data:

| State Key | Size | Description |
|-----------|------|-------------|
| `CNT` | 8 bytes | Counter tracking total number of messages stored |
| `{message_number}` | Variable | Individual message content, keyed by message number |

## Installation Example

```json
{
  "Account": "rBirthdayCardOwner...",
  "TransactionType": "SetHook",
  "Hooks": [
    {
      "Hook": {
        "CreateCode": "0061736D01000000...",
        "Flags": 1,
        "HookApiVersion": 0,
        "HookNamespace": "4FF9961269BF7630D32E15276569C94470174A5DA79FA567C0F62251AA9A36B9",
        "HookOn": "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFFFFFFFFFFFFFBFFFFF"
      }
    }
  ]
}
```

## Usage Examples

### Adding a Message

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

### Deleting a Message (Hook Owner Only)

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

## Technical Implementation

### Message Addition Process
1. Anyone can invoke to add a message
2. Retrieves current message counter from state
3. Increments counter to generate new message number
4. Stores message content using message number as key
5. Updates counter in state
6. Returns success message

### Message Deletion Process
1. Validates that invoker is the hook owner
2. Deletes message with specified number from state
3. Decrements message counter
4. Updates counter in state
5. Returns success message

### Security Features
- **Owner Verification**: Only hook account owner can delete messages
- **Transaction Type Validation**: Only processes invoke transactions
- **Buffer Management**: Safe handling of message content up to 1024 bytes
- **State Consistency**: Proper counter management for message tracking

## Access Control

- **Anyone Can Add**: Any account can add a message to the birthday card
- **Owner Only Delete**: Only the hook account owner can delete messages
- **Invoke Only**: Only invoke transactions are processed; all other transaction types are rejected
- **Parameter Validation**: Ensures proper parameter format and size

## Success Messages

- `"Success: Message added to birthday card!"` - Message successfully stored
- `"Success: Message deleted from birthday card!"` - Message successfully removed

## Error Messages

| Error Message | Cause |
|---------------|-------|
| `"Error: Only hook owner can delete messages from birthday card"` | Transaction not from hook owner |
| `"Error: Transaction must be an Invoke"` | Non-invoke transaction type |
| `"Error: Could not add message to birthday card"` | Failed to store message in state |
| `"Error: Could not update message count on birthday card"` | Failed to update message counter |
| `"Error: Could not delete message from birthday card"` | Failed to remove message from state |
| `"Error: No valid birthday card operation specified"` | Neither MSG nor DEL parameter provided |

## State Storage Details

### Message Storage Format
- **Key**: 8-byte message number (big-endian uint64)
- **Value**: Raw message content (UTF-8 text)
- **Size Limit**: 1024 bytes per message

### Counter Management
- **Key**: "CNT" (3 bytes)
- **Value**: 8-byte counter (big-endian uint64)
- **Purpose**: Tracks total messages and generates unique message numbers

## Use Cases

This hook is perfect for:
- **Anniversary Celebrations**: Collect birthday wishes for Xahau's second anniversary
- **Community Boards**: Open message boards with owner moderation
- **Event Guestbooks**: On-chain guestbooks for special occasions
- **Learning Hook Development**: Educational example of open state management

## Debugging

The hook uses standard XRPL error handling:
- All errors are returned via `rollback()` with descriptive messages
- Success operations use `accept()` with confirmation messages
- View results in Xahau Explorer transaction details

## Tools & Resources

- **[Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop)**: Compile and deploy hooks
- **[Xahau Hooks Technical](https://xrpl-hooks.readme.io/reference/hook-api-conventions)**: Detailed Hooks references
- **[Message HEX String](https://transia-rnd.github.io/xrpl-hex-visualizer/)**: Convert your message and copy the HEX string 
- **[XahauExplorer](https://xahau-testnet.xrplwin.com/)**: View transactions and hook execution logs

## Example Workflow

1. **Deploy Hook**: Install BirthdayCardHook on your account
2. **Add First Message**: Send invoke with "Happy Birthday!" in MSG parameter
3. **Add More Messages**: Community members send additional invokes with their wishes
4. **Delete Message**: Owner sends invoke with DEL parameter set to message number 1
5. **Verify Results**: Check transaction results in Xahau Explorer

This hook demonstrates open message collection, owner moderation, and on-chain celebration—making it a perfect fit for Xahau's second anniversary!