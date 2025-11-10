# Secure Savings Manager Hook (SSM) - Xahau HandyHook Collection

A production-ready Xahau hook that creates a time-released manageable escrow system for parent-controlled child savings accounts. Acts as a financial guardian enforcing spending limits and automatic savings discipline.

## Overview

This hook provides secure parent-managed savings accounts with the following features:

- **Automatic Escrow**: All incoming payments automatically locked - no bypass possible
- **Parent/Admin Control**: Only designated parent/guardian can release funds via invoke transactions
- **Spending Enforcement**: Outgoing payments strictly limited to available balance
- **Time-Released System**: Configurable intervals for automatic fund releases
- **Milestone Releases**: Manual releases for birthdays, achievements, rewards, etc.
- **Real-Time Balance Tracking**: Separate locked, available, and spent amount monitoring
- **Emergency Controls**: Safety mechanisms for urgent fund access
- **Pass-Through Design**: Non-payment transactions pass through without interference

## Core Functionality

### Escrow Management
- **Incoming Payments**: All funds automatically locked in escrow (cannot be disabled)
- **Balance Pools**: Separate tracking of locked vs available balances
- **Spending Control**: Child can only spend up to available balance (strictly enforced)
- **Real-Time Deduction**: Available balance reduced immediately on spending

### Parent Controls
- **Release Schedule**: Configure automatic release intervals and percentages
- **Manual Adjustments**: Release funds for milestones and achievements
- **Emergency Access**: Unlock all funds with safety code when needed
- **Complete Oversight**: Full visibility into child's financial activity

## Hook Parameters (via ttINVOKE from admin account)

| Parameter | Size     | Format            | Description |
|-----------|----------|-------------------|-------------|
| `ADMIN` | 20 bytes | Account ID | Parent/guardian account ID with full control |
| `SET_INTERVAL` | 4 bytes | Big-endian uint32 | Release interval in ledgers (default: 17280 = ~24h) |
| `AUTO_RELEASE` | 4 bytes | Big-endian uint32 | Automatic release percentage per interval (0-100%) |
| `RELEASE` | 4 bytes | Big-endian uint32 | Manual release percentage for milestones (1-100%) |
| `STATUS` | 1 byte | Any value | Query current locked, available, and spent balances |
| `UNLOCK` | 4 bytes | Safety code | Emergency unlock code (0xDEADBEEF) - all funds available |

## Installation Example

```json
{
  "Account": "rChildSavingsAccount...",
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

### Initial Setup (Child Account)

Set parent as admin (one-time setup):
```json
{
  "TransactionType": "Invoke",
  "Account": "rChildSavingsAccount...",
  "Destination": "rChildSavingsAccount...",
  "HookParameters": [
    {
      "HookParameter": {
        "HookParameterName": "41444D494E",
        "HookParameterValue": "F734DAE9FB86A7EA543BBFFECBF432F50D2B6423"
      }
    }
  ]
}
```

### Parent Configuration

Set automatic release schedule (10% every 24 hours):
```json
{
  "TransactionType": "Invoke",
  "Account": "rParentAccount...",
  "Destination": "rChildSavingsAccount...",
  "HookParameters": [
    {
      "HookParameter": {
        "HookParameterName": "4155544F5F52454C45415345",
        "HookParameterValue": "0000000A"
      }
    },
    {
      "HookParameter": {
        "HookParameterName": "5345545F494E54455256414C",
        "HookParameterValue": "00004380"
      }
    }
  ]
}
```

### Manual Milestone Release

Release 25% for birthday/achievement:
```json
{
  "TransactionType": "Invoke",
  "Account": "rParentAccount...",
  "Destination": "rChildSavingsAccount...",
  "HookParameters": [
    {
      "HookParameter": {
        "HookParameterName": "52454C45415345",
        "HookParameterValue": "00000019"
      }
    }
  ]
}
```

### Balance Status Query

```json
{
  "TransactionType": "Invoke",
  "Account": "rParentAccount...",
  "Destination": "rChildSavingsAccount...",
  "HookParameters": [
    {
      "HookParameter": {
        "HookParameterName": "535441545553",
        "HookParameterValue": "01"
      }
    }
  ]
}
```

### Emergency Unlock

```json
{
  "TransactionType": "Invoke",
  "Account": "rParentAccount...",
  "Destination": "rChildSavingsAccount...",
  "HookParameters": [
    {
      "HookParameter": {
        "HookParameterName": "554E4C4F434B",
        "HookParameterValue": "DEADBEEF"
      }
    }
  ]
}
```

## Savings Flow Logic

### Incoming Funds (Automatic Escrow)
1. **Any Payment**: Funds sent to child's savings account
2. **Automatic Lock**: All incoming amounts locked in escrow
3. **No Bypass**: Child cannot access funds until parent releases them
4. **Auto-Release Check**: Trigger automatic interval-based releases if configured

### Parent-Controlled Releases
1. **Manual Releases**: Parent triggers percentage-based releases for milestones
2. **Automatic Releases**: Scheduled releases based on configured intervals
3. **Balance Transfer**: Funds move from locked → available pool
4. **Spending Authorization**: Child can now spend released amounts

### Child Spending (Enforced Limits)
1. **Payment Attempt**: Child tries to send payment from account
2. **Balance Check**: Hook verifies payment amount vs available balance
3. **Approval/Block**: Payment approved if sufficient funds, blocked otherwise
4. **Real-Time Deduction**: Available balance reduced on successful payments

**Example Flow**: 
- **Genesis Mint**: 1000 XAH → Child account (automatically locked)
- **Parent Release**: 20% → 200 XAH available for spending
- **Child Spending**: Can spend up to 200 XAH, remaining 800 XAH stays locked
- **Overspend Attempt**: 300 XAH payment → BLOCKED ("Insufficient available balance")

## Technical Implementation

### Balance Management
- **Locked Balance**: Funds in escrow awaiting parent release
- **Available Balance**: Funds released by parent, ready for spending
- **Spent Tracking**: Running total of all outgoing payments
- **Real-Time Updates**: Balances updated immediately on transactions

### Timing Controls
- **Interval System**: Uses ledger sequences for precise timing (17280 ledgers ≈ 24 hours)
- **Release Constraints**: Prevents rapid manual releases (respects intervals)
- **Auto-Release Logic**: Automatic percentage releases when intervals pass

### Security Features
- **Admin Access Control**: Only designated parent can configure system
- **Spending Enforcement**: Payments physically blocked if insufficient available balance
- **Emergency Safety**: Unlock mechanism with safety code for urgent situations
- **State Isolation**: Each account maintains independent state data

### Transaction Flow
1. Validates transaction type and participants
2. For ttINVOKE: Processes parent configuration or child setup
3. For outgoing ttPAYMENT: Enforces spending limits and updates balances
4. For incoming ttPAYMENT: Locks funds and triggers auto-releases
5. Updates state and provides transaction feedback

## Pass-Through Messages

The hook allows these transactions to pass through unchanged:

- `"Transaction passed through"` - Non-payment transaction types
- `"Outgoing transaction approved"` - Approved child spending (within limits)
- `"Payment locked in escrow"` - Incoming payments successfully locked
- `"Non-XAH payment accepted"` - Non-native currency payments

## Configuration Messages

| Success Message | Action |
|----------------|--------|
| `"Admin configured"` | Parent/guardian account set |
| `"Auto-release percentage configured"` | Automatic release percentage set |
| `"Release interval configured"` | Release timing interval set |
| `"Funds released to available balance"` | Manual/automatic release completed |
| `"Status query completed"` | Balance information retrieved |
| `"All funds unlocked"` | Emergency unlock executed |

## Error Messages

| Error Message | Cause |
|---------------|-------|
| `"Only hook owner can set initial admin"` | Non-owner trying to set initial admin |
| `"Only admin can configure"` | Non-admin attempting configuration |
| `"AUTO_RELEASE percentage must be 0-100"` | Invalid auto-release percentage |
| `"Release percentage must be 1-100"` | Invalid manual release percentage |
| `"Release too soon - wait for interval"` | Manual release attempted before interval elapsed |
| `"No locked funds to release"` | Release attempted with zero locked balance |
| `"Invalid unlock code"` | Wrong emergency unlock safety code |
| `"Insufficient available balance - wait for parent release"` | Child overspend attempt blocked |
| `"No valid parameters"` | ttINVOKE with no recognized parameters |

## State Management

### Hook State (Account-Level Configuration)
- `ADMIN`: Parent/guardian account ID (20 bytes)
- `LOCKED`: Current locked balance in drops (8 bytes)
- `AVAILABLE`: Current available balance in drops (8 bytes)  
- `AUTO_LOCK`: Auto-release percentage for intervals (4 bytes)
- `INTERVAL`: Release interval in ledgers (4 bytes)
- `LAST_RELEASE`: Last release ledger sequence (4 bytes)
- `SPENT`: Total spent amount tracking (8 bytes)

### Balance Calculations
- **Locked Balance**: Funds waiting for parent release
- **Available Balance**: Funds released and ready for spending
- **Total Savings**: Locked + Available balances
- **Spent Total**: Cumulative outgoing payment amounts

## Real-World Use Cases

### Weekly Allowance System
```
Setup: SET_INTERVAL 120960 (1 week), AUTO_RELEASE 15
Result: 15% of locked funds released weekly as allowance
Child gets consistent spending money without daily parent management
```

### Achievement-Based Rewards
```
Child completes chores/goals → Parent: RELEASE 25
Extra funds released as milestone reward on top of regular allowance
```

### Emergency Situations
```
Medical emergency → Parent: UNLOCK 0xDEADBEEF
All locked funds immediately available for urgent expenses
```

### Budget Learning
```
Child attempts overspending → Transaction blocked with clear message
Teaches financial discipline and budget awareness in real-time
```

## Educational Benefits

### Financial Literacy
- **Budget Awareness**: Child learns to live within released amounts
- **Delayed Gratification**: Understanding that savings require patience
- **Spending Discipline**: Real-time feedback on financial decisions
- **Milestone Recognition**: Rewards tied to achievements and behavior

### Parental Tools
- **Flexible Control**: Balance automation with manual oversight
- **Teaching Moments**: Use releases to reinforce positive behaviors
- **Emergency Safety**: Peace of mind with override capabilities
- **Complete Visibility**: Full transparency into child's spending patterns

## Debugging

The hook includes comprehensive trace logging:
- Balance amounts displayed in XAH for readability
- Release percentages and amounts logged for verification
- Status queries provide complete financial overview
- View trace output in Xahau Explorer transaction details

## Tools & Resources

- **[Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop)**: Compile and deploy hooks
- **[Xahau Hooks Technical](https://xrpl-hooks.readme.io/reference/hook-api-conventions)**: Detailed Hooks references
- **[Address to Account ID Converter](https://hooks.services/tools/raddress-to-accountid)**: Convert addresses to 20-byte account IDs
- **[Amount to Uint64](https://transia-rnd.github.io/xrpl-hex-visualizer/)**: Convert integer amounts to Uint64
- **[XahauExplorer](https://xahau-testnet.xrplwin.com/)**: View transactions and hook execution logs

## Integration with Genesis Mint Savings Hook

This Savings Manager Hook is designed to work seamlessly with the Genesis Mint Savings Hook:

1. **Genesis Mint Detection**: Genesis Mint Savings Hook detects ttGENESIS_MINT transactions
2. **Automatic Forwarding**: Configured percentages sent to child savings accounts
3. **Automatic Escrow**: Savings Manager immediately locks all received funds
4. **Parent Management**: Parents control release of accumulated Genesis Mint proceeds
5. **Complete System**: End-to-end automatic savings with parental oversight

This creates a comprehensive **automated capture → controlled release** system for teaching financial responsibility while ensuring savings discipline.