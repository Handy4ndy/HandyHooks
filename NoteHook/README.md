# NoteHook (NTH) - Xahau HandyHook Collection

## Overview

The **NoteHook** is simple yet powerful Xahau hook that allows the hook owner to store and manage notes on-chain. This hook demonstrates basic state management and owner-only access control patterns.

This hook provides on-chain note storage capabilities with the following features:

- **Owner-Only Access**: Only the hook account owner can add or delete notes
- **On-Chain Storage**: Notes are stored directly in the hook's state
- **Dynamic Note Management**: Add new notes or delete existing ones by note number
- **Automatic Counter Management**: Tracks the total number of notes stored
- **Simple Invoke Interface**: Uses invoke transactions for all operations

## Hook Parameters

This hook does not require any installation parameters. All functionality is controlled through transaction parameters.

## Transaction Parameters

When invoking the hook, use one of these parameters:

### Adding Notes
| Parameter | Size | Format | Description |
|-----------|------|--------|-------------|
| `NOT`     | Variable | UTF-8 text | The note content to store (up to 1024 bytes) |

### Deleting Notes
| Parameter | Size    | Format            | Description               |
|-----------|---------|-------------------|---------------------------|
| `DEL`     | 8 bytes | Big-endian uint64 | The note number to delete |

## State Management

The hook manages the following state data:

| State Key       | Size     | Description                                   |
|-----------------|----------|-----------------------------------------------|
| `CNT`           | 8 bytes  | Counter tracking total number of notes stored |
| `{note_number}` | Variable | Individual note content, keyed by note number |

## Installation Example

```json
{
  "Account": "rNoteOwnerAccount...",
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

### Adding a Note

```json
{
  "TransactionType": "Invoke",
  "Account": "rNoteOwnerAccount...",
  "Destination": "rNoteOwnerAccount...",
  "HookParameters": [
    {
      "HookParameter": {
        "HookParameterName": "4E4F54",
        "HookParameterValue": "48656C6C6F2C20746869732069732061206E6F7465"
      }
    }
  ]
}
```

### Deleting a Note

```json
{
  "TransactionType": "Invoke",
  "Account": "rNoteOwnerAccount...",
  "Destination": "rNoteOwnerAccount...",
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

### Note Addition Process
1. Validates that invoker is the hook owner
2. Retrieves current note counter from state
3. Increments counter to generate new note number
4. Stores note content using note number as key
5. Updates counter in state
6. Returns success message

### Note Deletion Process
1. Validates that invoker is the hook owner
2. Deletes note with specified number from state
3. Decrements note counter
4. Updates counter in state
5. Returns success message

### Security Features
- **Owner Verification**: Only hook account owner can invoke operations
- **Transaction Type Validation**: Only processes invoke transactions
- **Buffer Management**: Safe handling of note content up to 1024 bytes
- **State Consistency**: Proper counter management for note tracking

## Access Control

- **Owner Only**: All operations require the transaction to originate from the hook account owner
- **Invoke Only**: Only invoke transactions are processed; all other transaction types are rejected
- **Parameter Validation**: Ensures proper parameter format and size

## Success Messages

- `"Success: Note added"` - Note successfully stored
- `"Success: Note deleted"` - Note successfully removed

## Error Messages

| Error Message                                   | Cause                                  |
|-------------------------------------------------|----------------------------------------|
| `"Error: Only hook owner can invoke this hook"` | Transaction not from hook owner        |
| `"Error: Transaction must be an Invoke"`        | Non-invoke transaction type            |
| `"Error: Could not set note state"`             | Failed to store note in state          |
| `"Error: Could not update count state"`         | Failed to update note counter          |
| `"Error: Could not delete note"`                | Failed to remove note from state       |
| `"Error: No valid operation specified"`         | Neither NOT nor DEL parameter provided |

## State Storage Details

### Note Storage Format
- **Key**: 8-byte note number (big-endian uint64)
- **Value**: Raw note content (UTF-8 text)
- **Size Limit**: 1024 bytes per note

### Counter Management
- **Key**: "CNT" (3 bytes)
- **Value**: 8-byte counter (big-endian uint64)
- **Purpose**: Tracks total notes and generates unique note numbers

## Use Cases

This hook is perfect for:
- **Personal Note Taking**: Store private notes on-chain
- **Documentation**: Keep project notes or documentation immutable
- **Audit Trails**: Create timestamped records of events
- **Simple Data Storage**: Basic key-value storage for small data
- **Learning Hook Development**: Educational example of state management

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

1. **Deploy Hook**: Install NoteHook on your account
2. **Add First Note**: Send invoke with "Hello World" in NOT parameter
3. **Add More Notes**: Send additional invokes with different note content
4. **Delete Note**: Send invoke with DEL parameter set to note number 1
5. **Verify Results**: Check transaction results in Xahau Explorer

This hook demonstrates fundamental hook development concepts including state management, access control, and transaction parameter handling, making it an excellent learning resource for new hook developers.