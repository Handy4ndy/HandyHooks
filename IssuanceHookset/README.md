# IssuanceHookset: A Comprehensive Suite of Hooks for Secure, Atomic Token Issuance on Xahau

## Executive Summary

IssuanceHookset is an open-source initiative to empower projects and developers on the Xahau network with a robust, modular toolkit of programmable Hooks designed specifically for atomic, on-chain token issuance. Built entirely on Xahau's WebAssembly-based Hooks framework, this suite enables fully decentralized, automated, and compliant mechanisms for launching and distributing fungible IOU tokens (Issued Currencies) without relying on off-chain infrastructure, intermediaries, or manual processes.

At its core, IssuanceHookset addresses the challenges of traditional token launches by providing "atomic issuance solutions"—meaning every aspect of token creation, distribution, participation tracking, and compliance enforcement happens atomically within a single ledger transaction or a tightly coupled sequence of Hook-triggered emissions (e.g., via Remit for trustless delivery). This ensures transparency, immutability, and efficiency while minimizing risks like front-running, reentrancy, or human error.

The project draws inspiration from real-world regulatory landscapes, emphasizing built-in compliance features, on-ledger metadata for disclosures, and a sequential "hook-chain" design where individual Hooks can interconnect for complex workflows. Starting with the flagship IDO (Initial Dex Offering) Hook as a foundational example, IssuanceHookset is expanding rapidly with more specialized Hooks in development—aiming to cover a wide range of issuance models, from phased fundraising to subscription-based rewards, governance tokens, and beyond.

This is not just a single tool; it's a growing ecosystem of Hooks that democratizes secure token issuance on Xahau, fostering a participation-first economy where value is tied to active engagement rather than speculation. With Hooks handling everything from phase-based multipliers to soft-cap refunds, IssuanceHookset positions Xahau as the premier platform for programmable, compliant DeFi primitives. More examples and integrations are on the horizon, making this a pivotal step toward a fully automated, decentralized token economy.

## Project Vision and Goals

IssuanceHookset envisions a future where token issuance on Xahau is as seamless and secure as sending a payment—yet enriched with sophisticated logic for fairness, compliance, and utility. Key goals include:

- **Atomic Issuance**: Ensure tokens are issued instantly and trustlessly in response to user actions (e.g., payments), with all validations, calculations, and deliveries occurring on-chain in one atomic operation.
- **Modularity and Extensibility**: Design Hooks that can be installed independently or chained together, allowing projects to mix-and-match features like timed phases, user tracking, and refund mechanisms.
- **Regulatory Clarity and User Protection**: Embed compliance tools directly into the Hooks, reducing barriers for projects while providing clear disclosures and safeguards for participants.
- **On-Chain Transparency**: Leverage Xahau's ledger for immutable records of totals raised, tokens issued, per-user participation, and more—eliminating the need for external dashboards or audits.
- **Sustainability and Fairness**: Promote models that reward genuine participation over hype, with features like decaying multipliers, soft caps, and unwind options to align incentives.

By focusing on these principles, IssuanceHookset transforms token launches from opaque, high-risk events into reliable, programmable processes that scale with Xahau's efficient consensus.

## Design Inspirations

The architecture of IssuanceHookset is deeply rooted in practical discussions around blockchain usability, regulatory challenges, and the unique capabilities of Xahau Hooks. Early conceptual talks emphasized the need for "regulatory clarity" in token issuance—drawing from global frameworks like the U.S. SEC's Howey Test, EU's MiCA regulations, UK's FCA guidelines, and FATF standards. This inspired built-in features such as:

- **Embedded Compliance Metadata**: Hooks incorporate mandatory parameters for risk disclosures, terms of service, and KYC/AML self-certification. For instance, the IDO Hook requires a WP_LNK (Whitepaper Link) parameter—a hex-encoded URI stored on-ledger during installation. This acts as an "access token" where participants implicitly acknowledge terms by including it in transaction params, ensuring on-chain evidence of informed consent. This idea evolved from conversations about using URI fields (inspired by XRPL's URI-based metadata standards) to link immutable IPFS-hosted documents for whitepapers, audits, and legal terms—making compliance auditable and tamper-proof.
- **Sequential Hook Interconnectivity**: A key innovation is the "hook-chain" concept, where Hooks are designed to "follow onto" each other in a logical sequence. For example, an initial Setup Hook might configure parameters and activate a Fundraising Hook, which in turn triggers a Distribution Hook for rewards. This modular flow stems from early ideation on creating composable primitives: one Hook handles window timing and multipliers, another manages user namespaces for participation data, and a third enforces refunds or governance. This allows for complex, multi-stage issuance without bloating a single Hook, while ensuring atomicity through emitted transactions (e.g., Remit for token delivery).
- **Regulatory-Driven Safeguards**: Discussions on avoiding "securities-like" pitfalls led to features like soft caps (minimum raise thresholds with automatic refunds if unmet), unwind mechanisms (exact token returns for proportional refunds), and phase-based multipliers that reflect risk/reward without promising profits. URI fields extend this by enabling dynamic links to audit reports, tax guidance, and sustainability notes—promoting self-assessed utility token classifications while advising users to consult local laws.

These inspirations ensure IssuanceHookset isn't just functional but resilient in a regulated world, turning potential liabilities into strengths.

## Components

### Core Hooks

#### 3. Router Hook
A smart router that manages execution flow in hook chains:
- **Transaction Routing**: Determines whether to run IDO or Rewards hooks based on transaction type
- **State-Aware**: Checks IDO window status, refund modes, and participation data
- **Hardcoded Configuration**: Uses predefined hook hashes and namespaces for security
- **Chain Management**: Ensures only relevant hooks execute per transaction

#### 2. IDO Master Hook (IDOM)
The main hook that manages the entire IDO lifecycle:
- **Phased Token Sales**: 5 phases with decreasing multipliers (100x, 75x, 50x, 25x, cooldown)
- **Soft Cap Evaluation**: Automatic evaluation after Phase 4 with refund activation if not met
- **Balance Protection**: Locks raised funds during active IDO, unlocks after successful completion
- **Unwinding**: Allows participants to return IOU tokens for XAH refunds during eligible periods
- **Whitepaper Validation**: Ensures user acknowledgment of terms via WP_LNK parameter


#### 3. Rewards Hook (IRH)
Post-IDO rewards distribution system:
- **Configurable Rates**: Admin-settable interest rates and claim intervals
- **Participation Bonuses**: +5% bonus rates for original IDO participants
- **Trustline Validation**: Ensures claimants have required IOU holdings
- **Timing Constraints**: Enforces claim intervals and lifetime limits
- **Scalable State**: Uses hierarchical namespaces for unlimited user tracking

## Core Features and Technical Highlights

IssuanceHookset leverages Xahau's Hooks for 100% on-chain automation:

- **Atomic Operations**: Hooks react to incoming transactions (e.g., Payments or Invokes) and emit responses like Remit for IOU delivery—creating trust lines on-the-fly without pre-setup.
- **State Management**: Uses hierarchical namespaces for per-user data (e.g., invested amounts and received tokens) and global counters (executions, totals raised/issued, per-phase stats).
- **Security Best Practices**: Includes guard macros to prevent overflows/reentrancy, restricted access (e.g., only admin can start windows), and precise amount validations.
- **Customization via Parameters**: Install-time params like INTERVAL (phase duration), ADMIN (authorized invoker), CURRENCY (token code), WP_LNK (disclosures URI), and SOFT_CAP (refund threshold) make Hooks adaptable.
- **Render Components Integration**: Future expansions will include inline citations for on-chain data and image rendering for visual metadata (e.g., token icons via URI).

All code is written in C with HookAPI conventions, ensuring efficiency and compatibility.

## Flagship Example: The IDO Hook

The Initial Dex Offering (IDO) Hook serves as the cornerstone of IssuanceHookset, demonstrating atomic issuance in a phased fundraising model:

- **Mechanics**: Installed on the issuer account, it accepts XAH payments during a configurable 5-phase window (4 active + 1 cooldown). Multipliers decay over time (100x → 75x → 50x → 25x → 0x), rewarding early participants while closing deposits in the final phase.
- **Atomic Flow**: Incoming XAH → Hook calculates phase/multiplier → Emits Remit with multiplied IOU → Updates state atomically.
- **Protections**: Soft cap check at window end—if unmet, enters permanent refund mode. Users unwind by sending exact IOU back for XAH refund.
- **Compliance Ties**: Requires WP_LNK for disclosures; participants self-certify via transaction params.
- **Real-World Templates**: Includes documented transaction examples (SetHook for install, Invoke for activation, Payments for deposits/unwinds) with hashes verifiable on Xahau explorers.

This Hook encapsulates the project's ethos: secure, fair, and fully on-chain.

## File Structure

```
IssuanceHookset/
├── Docs/                          # Detailed documentation
│   ├── IDOMulti.md               # IDO Hook documentation
│   ├── Rewards.md                # Rewards Hook documentation
│   └── Router.md                 # Router Hook documentation
├── Fin/                          # Production-ready condensed hooks
│   ├── IDOMulti.c                # Condensed IDO hook
│   ├── Rewards.c                 # Condensed Rewards hook
│   ├── Router.c                  # Condensed Router hook
│   └── README.md                 # Production notes
├── Hooks/                        # Master hooks
│   ├── IDOMaster.c               # Commented IDO hook source
|   ├── RewardsMaster.c           # Commented Rewards hook source
|   └── RouterMaster.c            # Commented Router hook source
│   
└── README.md                     # This file
```

## Installation and Setup

### Hook Chain Order
Install hooks in this order for proper chain execution:
1. **Router Hook** (first in chain)
2. **IDO Hook** (second)
3. **Rewards Hook** (third)

### Required Parameters

#### IDO Hook Parameters
- `ADMIN` (20 bytes): Admin account ID
- `CURRENCY` (20 bytes): IOU currency code
- `INTERVAL` (4 bytes): Phase interval in ledgers
- `SOFT_CAP` (8 bytes): Soft cap in XAH
- `WP_LNK` (variable): Whitepaper link

#### Rewards Hook Parameters
- `CURRENCY` (20 bytes): Reward currency code
- `ADMIN` (20 bytes): Admin account ID
- `INT_RATE` (4 bytes): Interest rate (optional)
- `SET_INTERVAL` (4 bytes): Claim interval (optional)
- `SET_MAX_CLAIMS` (4 bytes): Max claims limit (optional)

#### Router Hook
No runtime parameters - uses hardcoded configuration.

## Usage Flow

### 1. Setup Phase
- Install hooks with required parameters
- Admin invokes IDO hook with `START` parameter to begin window

### 2. Active IDO Phase
- Users send XAH payments with `WP_LNK` parameter
- Router directs to IDO hook for token issuance
- Automatic phase progression based on ledger intervals

### 3. Evaluation Phase
- After Phase 4: Soft cap automatically evaluated
- Success: Funds unlock after cooldown
- Failure: Refund mode activated

### 4. Post-IDO Phase
- Successful sales: Users can unwind during cooldown or hold for rewards
- Failed sales: Users must unwind for refunds
- Rewards hook becomes active for claim distribution

### 5. Rewards Phase
- IOU holders claim periodic rewards
- IDO participants receive bonus rates
- Admin can adjust rates and intervals

## XahRise Application

Complementing the IssuanceHookset is the **XahRise** application - a user-friendly platform that enables automated asset issuance on the Xahau network through intelligent hook chain deployment.

### Overview

XahRise provides a complete solution for users to:
- Authenticate via Xaman wallet integration
- Select and configure hook combinations with conflict prevention
- Automatically deploy Router-coordinated hook chains
- Issue assets through orchestrated hook systems on Xahau

### Key Features

#### ✅ Completed Features
- **Authentication System**: Xaman wallet integration with modal-based sign-in flow
- **Theme Implementation**: "Aureum X Rebirth" cyber-alchemical luxury theme
- **Hook Selection Interface**: Modal-based selector with conflict prevention
- **Router Architecture**: Intelligent hook coordination system
- **Parameter Configuration UI**: Advanced forms with duration selectors and validation
- **Backend API**: Complete Express server with Xumm SDK integration
- **SetHook Transaction Engine**: Single-transaction hookchain installation
- **Automated Installation**: QR code generation with WebSocket status monitoring

#### 🔄 In Development
- **Frontend-Backend Integration**: Connecting UI to payload generation API
- **Installation Status Tracking**: Real-time progress updates
- **Transaction Monitoring**: Post-installation verification

### Technical Architecture

#### Frontend Stack
- **React 19** with Vite for fast development
- **Custom CSS Variables** for theme management
- **Modal-based UX** for authentication and configuration
- **Responsive design** with mobile-first approach

#### Backend Integration
- **Express.js** server with Xumm SDK
- **WebSocket support** for real-time updates
- **SetHook Transaction Engine** for atomic hookchain deployment
- **Hook Parameter Processing** with dynamic configuration

#### Hook Chain Implementation
- **Modular Architecture**: Individual hooks with single responsibilities
- **Router-Based Coordination**: Intelligent transaction routing
- **Namespace Isolation**: Clean state separation between chains
- **Parameter Injection**: Automatic dependency configuration

### Current Status

**Project Phase**: Integration & Testing  
**Completion Level**: ~95% (Backend API complete, frontend-backend integration pending)

The XahRise platform features a complete backend API capable of generating proper SetHook transactions for single-transaction hookchain installation. The frontend UI is fully implemented with advanced parameter configuration. The next phase focuses on connecting these components and testing the complete user flow from hook selection to blockchain deployment.

### Performance Optimizations

- **Bundle Size**: Optimized from 869KB to 191KB main app bundle
- **Lazy Loading**: Route-based code splitting for heavy components
- **Manual Chunking**: Separated vendor libraries for better caching
- **Progressive Loading**: Components load only when needed

### Single-Transaction Hookchain Installation

XahRise implements a revolutionary approach where entire hook chains deploy atomically in one Xahau transaction:

```json

{
"TransactionType": "SetHook",
"Account": "raXdWCS1Ro8gVER2zRQMPn3pm4kLdD7okD",
"Flags": 0,
"Hooks": [
{
"Hook": {
"HookHash": "4F7053A3D4B9D433DAB627BDE119B5C2F2304F9A2090FB7BBB181FF42DECAE8B",
"Flags": 1,
"HookOn": "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFFFFFFFFFFFFFBFFFFE",
"HookNamespace": "065D8E6C0BF74A69A6D312C3D5B5CC627434CECE07B2787C1A538FCFD9F9C8DE",
"HookParameters": [
{
"HookParameter": {
"HookParameterName": "49444F5F48415348",
"HookParameterValue": "330961A6811A03131B590D0C69211447E78DF7208898A44F8CC1E13C629F2D2D"
}
},
{
"HookParameter": {
"HookParameterName": "524557415244535F48415348",
"HookParameterValue": "8CFC9AA6AA4A858DEF04D3049D4E7D22A37F968D050634244EC5DACECCE6160D"
}
},
{
    "HookParameter": {
        "HookParameterName": "4E414D455350414345",
"HookParameterValue": "516BA79215002276EF4C381B901955C53A04575589607E7DBA20468DD343DD72"
}
}
]
}
},
{
    "Hook": {
        "HookHash": "330961A6811A03131B590D0C69211447E78DF7208898A44F8CC1E13C629F2D2D",
    "Flags": 1,
"HookOn": "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFFFFFFFFFFFFFBFFFFE",
"HookNamespace": "516BA79215002276EF4C381B901955C53A04575589607E7DBA20468DD343DD72",
"HookParameters": [
{
"HookParameter": {
"HookParameterName": "43555252454E4359",
"HookParameterValue": "5453540000000000000000000000000000000000"
}
},
{
"HookParameter": {
"HookParameterName": "494E54455256414C",
"HookParameterValue": "0000001E"
}
},
{
    "HookParameter": {
        "HookParameterName": "57505F4C4E4B",
"HookParameterValue": "787370656E63652E636F2E756B"
}
}
]
}
},
{
    "Hook": {
        "HookHash": "8CFC9AA6AA4A858DEF04D3049D4E7D22A37F968D050634244EC5DACECCE6160D",
    "Flags": 1,
"HookOn": "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFFFFFFFFFFFFFBFFFFF",
"HookNamespace": "59AFE47D9D3772675632EE5EBBFFEEB325D9A094387C87E3818357F4BD46FCC7",
"HookParameters": [
{
"HookParameter": {
"HookParameterName": "43555252454E4359",
"HookParameterValue": "5453540000000000000000000000000000000000"
}
},
{
"HookParameter": {
"HookParameterName": "494E545F52415445",
"HookParameterValue": "000000C8"
}
},
{
"HookParameter": {
"HookParameterName": "5345545F494E54455256414C",
"HookParameterValue": "00000014"
}
},
{
"HookParameter": {
"HookParameterName": "5345545F4D41585F434C41494D53",
"HookParameterValue": "00000000"
}
}
]
}
}
],
"Sequence": 800025814,
"NetworkID": 21337,
"Fee": "1000",
"LastLedgerSequence": 20391662
} 
```

This provides optimal UX with atomic deployment guarantees, single signature requirements, and intelligent state coordination.

### Links

- **XahRise Repository**: [GitHub Link]
- **Live Demo**: https://xahrise.co.uk
- **Documentation**: [XahRise Docs]

## Roadmap and Future Expansions

## Key Features

### Security
- **Admin Authorization**: Strict access controls for configuration
- **Parameter Validation**: Comprehensive input checking
- **Balance Protection**: Prevents premature fund withdrawal
- **State Integrity**: Foreign namespace queries for secure data access

### Flexibility
- **Configurable Parameters**: Rates, intervals, limits adjustable by admin
- **Multi-Phase System**: Incentivizes early participation
- **Refund Mechanisms**: Protects participants if soft cap not met
- **Bonus Systems**: Rewards loyal participants

### Scalability
- **Hierarchical Namespaces**: Unlimited user tracking
- **Efficient Routing**: Minimizes unnecessary hook execution
- **State Optimization**: Minimal storage usage with packed data

## Integration Notes

- **Hook Chain Compatibility**: Designed for sequential execution
- **State Sharing**: Hooks communicate via shared namespaces
- **Transaction Types**: Supports invokes and payments
- **Ledger Awareness**: Time-based phase management

## Error Handling

All hooks provide detailed error messages for debugging:
- Configuration errors
- Authorization failures
- Timing violations
- Invalid parameters
- State access issues

## Production Deployment

Use the condensed versions in the `Fin/` folder for production deployment. These have comments removed for smaller binary size while maintaining full functionality.

## Documentation

Detailed documentation for each hook:
- [IDO Hook Documentation](Docs/IDOMulti.md)
- [Router Hook Documentation](Docs/Router.md)
- [Rewards Hook Documentation](Docs/Rewards.md)

## Participants

- **Handy Andy** (@Handy_4ndy)

## Social Media & Contact

- **X (Twitter)**: https://x.com/Handy_4ndy
- **GitHub**: https://github.com/Handy4ndy
- **Contact Email**: handy_4ndy@outlook.com

## Links

- **Project Repository**: https://github.com/Handy4ndy/XahauHooks101
- **Live Demo**: Mainnet explorer links in documentation
- **Xahau Address**:  raXdWCS1Ro8gVER2zRQMPn3pm4kLdD7okD  (Issuer/Hook account with live mainnet installation)
- **Hook Code & Hash**: IDO.c - Hash: 330961A6811A03131B590D0C69211447E78DF7208898A44F8CC1E13C629F2D2D (Verify: https://xahau.xrplwin.com/tx/7189E633C9DA44F6DC30F8633F2E1D34DF7C6730BE412C1DA3BD4FA932365036
- **Additional Resources**:
  - Manifesto: https://github.com/Handy4ndy/XahauHooks101/blob/main/XahauSecureTokenPlatformManifest.txt
  - Transaction Templates: https://github.com/Handy4ndy/XahauHooks101/blob/main/TEMPLATES.md
  - Metadata Template: https://github.com/Handy4ndy/XahauHooks101/blob/main/METATEMPLATE.json
  - Source Links: https://github.com/Handy4ndy/XahauHooks101/blob/main/Sourcelinks.txt

## Disclaimer

Always test on testnet, seek audits, and consult professionals. Token issuance carries risks—use responsibly. Updated: January 31, 2026.

## Author

@Handy_4ndy

## License

Part of the Xahau HandyHook Collection. See repository license for details.