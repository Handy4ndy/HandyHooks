# Handy_4ndy_IssuanceHookset

## Project Title
IssuanceHookset: A Suite of Hooks for Secure, Atomic Token Issuance on Xahau

## Brief Description
IssuanceHookset is an open-source collection of programmable Hooks that enable fully on-chain, atomic token issuance and distribution for fungible IOU tokens on Xahau Mainnet.  

The flagship component is a complete **Initial Dex Offering (IDO) Hook** that powers phased, fair token launches directly from the issuer account—no servers, no intermediaries.  

Key features include:  
- 4 active phases with time-decaying multipliers (100× → 75× → 50× → 25×) to reward early participants  
- Soft-cap threshold with automatic permanent refund/unwind mode if unmet  
- Atomic issuance: incoming XAH Payment → Hook logic → emitted Remit with multiplied IOU (trust lines created on-the-fly)  
- On-chain tracking of global totals (raised XAH, issued IOU, executions, per-phase stats) + per-user participation data  
- Built-in compliance aids: mandatory `WP_LNK` URI parameter (hex-encoded whitepaper/docs link) for disclosures and user acknowledgment  
- Unwind mechanism: users return exact IOU amount → Hook emits proportional XAH refund  

Designed for end-users (project creators & participants), it lowers barriers to secure, transparent token launches while promoting participation-first models over speculation. The Hooks are modular and extensible—more issuance patterns (e.g. subscription rewards, governance) are in development.  

Live on Xahau Mainnet with real transaction examples verifiable via explorers.

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
├── IssuanceHookset.md
└── README.md                     # This file
```

## Participants
Handy Andy (@Handy_4ndy) – Creator of Xahau Hooks 101 and the Handy Hooks collection

## Participants’ Social Media
- X / Twitter: https://x.com/Handy_4ndy  
- GitHub: https://github.com/Handy4ndy

## Contact Email
[your-email-here@example.com]  <!-- replace with real email, e.g. contests@handyhooks.com -->

## Link to the Online Project
Live mainnet demo via explorer transactions and state:  
https://xahscan.com/account/rEydpmfnemhN5JCteuVju8DLmnpc6eGSZK  
(Example SetHook install tx: https://xahscan.com/tx/EE3EE43C01187276B9DEF63ED9F400E7CCE475786A066917324724E770E648D6)

## Xahau Address
[rYourXahauAddressHere]  <!-- replace with your prize-receiving address, e.g. rEydpmfnemhN5JCteuVju8DLmnpc6eGSZK if using the same -->

## Link to Documentation
- Main README: https://github.com/Handy4ndy/XahauHooks101/blob/main/README.md  
- Installation & Usage Guide: https://github.com/Handy4ndy/XahauHooks101/blob/main/IDO_Hook_Guide.md  
- Transaction Templates & Mainnet Examples: https://github.com/Handy4ndy/XahauHooks101/blob/main/TEMPLATES.md  
- Full Disclaimer & Risks: https://github.com/Handy4ndy/XahauHooks101/blob/main/DISCLAIMER.md

## Hooks code in C and matching hash
- Flagship Hook: **IDO Hook** (IDO.c) – full source attached in repo  
  → View: https://github.com/Handy4ndy/XahauHooks101/blob/main/IDO.c  
- HookDefinition / Install Tx Hash (confirms deployed code):  
  EE3EE43C01187276B9DEF63ED9F400E7CCE475786A066917324724E770E648D6  
  → Explorer: https://xahscan.com/tx/EE3EE43C01187276B9DEF63ED9F400E7CCE475786A066917324724E770E648D6

## Hooks account of the project
rEydpmfnemhN5JCteuVju8DLmnpc6eGSZK  
(Issuer account where the IDO Hook is installed and active on Mainnet)

## Link to Project’s Repository
https://github.com/Handy4ndy/XahauHooks101

## Other Links
- Project Manifesto (vision for secure token platform on Xahau): https://github.com/Handy4ndy/XahauHooks101/blob/main/XahauSecureTokenPlatformManifest.txt  
- Metadata JSON Template (for token metadata integration): https://github.com/Handy4ndy/XahauHooks101/blob/main/METATEMPLATE.json  
- Source Inspirations & Tools: https://github.com/Handy4ndy/XahauHooks101/blob/main/Sourcelinks.txt  

This submission heavily leverages **Xahau Hooks** for smart, user-facing token issuance logic—making complex fundraising safe, transparent, and fully on-chain for everyday projects and participants.