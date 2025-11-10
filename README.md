# HandyHooks - Mainnet/Testnet Production Hooks

Welcome to the **HandyHooks Production Collection** - a curated and growing set of Xahau hooks that are live and operational on both mainnet and testnet environments. These hooks are ready for installation using their respective hook hashes and represent battle-tested, production-ready smart contract solutions for a wide range of use cases, from token issuance to security and savings automation.

As the collection expands, new hooks are continuously added to support the Xahau ecosystem. Each hook is designed with transparency, security, and community benefit in mind.

## 📦 Available Hook Categories

HandyHooks is organized into categories to help you find the right tool for your needs. Each category includes production-ready hooks with detailed documentation in their subdirectories. Explore the folders below for specific hook READMEs, installation guides, and examples.

### 🏦 Issuance Collection
Advanced token issuance and management hooks for sophisticated DeFi applications.
- **Admin Issuance**: Controlled token issuance with whitelist access and automatic treasury allocation (5% auto-treasury).
- **Native Issue**: Automatic token issuance in response to XAH payments with treasury allocation.
- **Daily Rewards**: Automated daily token rewards via invoke transactions with configurable limits.
- **Bridge Reserve**: Cross-chain token bridging through burn-to-mint mechanisms.

### 💰 Savings Collection
Automated savings and financial discipline tools.
- **Savings Manager**: Parent-controlled child savings with time-released funds and spending limits.
- **Incoming Payment Savings**: Automatic forwarding of a percentage of incoming XAH to designated savings accounts.

### 🛡️ Security Collection
Hooks for account protection, compliance, and risk management.
- **SafeGuard**: Transaction safety with limits, blacklists, and monitoring (includes developer contributions).
- **Blacklist Provider/Trustee**: Centralized blacklist management with distributed enforcement.
- **Set Hook Locker**: Prevents unauthorized hook modifications for production accounts.

### 📝 Utility Collection
General-purpose hooks for data management and community features.
- **NoteHook**: On-chain note storage and management.
- **Birthday Card**: Community celebration hook for Xahau's anniversary.

### 🤝 Beneficiary Collection
Automated distribution and inheritance tools.
- **Single Beneficiary Contracts**: Delegate-controlled or threshold-based balance transfers.

*And more coming soon! Check back as we expand the collection with new hooks for staking, governance, and beyond.*

## 🚀 Installation Guide

### Prerequisites
- Active Xahau account with sufficient XAH for hook installation.
- Understanding of hook installation process.
- Knowledge of transaction parameters for your chosen hook.

### Installation Steps
1. **Choose Your Hook**: Browse the categories above and select based on your needs.
2. **Review Documentation**: Each hook's subdirectory contains detailed README with installation parameters, transaction examples, and usage guides.
3. **Prepare Installation Transaction**: Use the hook hash and required parameters from the specific hook's docs.
4. **Deploy**: Submit the SetHook transaction to install on your account.
5. **Test**: Verify functionality with test transactions on testnet first.

Hook hashes for mainnet/testnet deployment are provided in each individual hook's README file.

## 💰 Developer Contributions

To support ongoing development, maintenance, and expansion of the HandyHooks collection, many hooks include transparent developer contributions. These are hardcoded into the hook code and automatically charged for specific operations (e.g., successful transactions or claims). All code remains open source, and contributions fund community-driven innovation.

- ✅ **Open Source**: All source code is publicly available.
- ✅ **Transparent**: Contributions are clearly documented in each hook's README.
- ✅ **Community Focused**: Contributions support continued development for everyone.
- ✅ **Production Ready**: Tested and deployed on live networks.

### Developer Contribution Structure

Each hook specifies its contribution model in its documentation. Here's an overview of contributions across the collection:

| Hook Category | Contribution Type | Amount | Purpose |
|---------------|-------------------|---------|---------|
| SafeGuard | Per Operation | 50,000 drops | Security monitoring |
| Admin Issuance | Percentage | 5% to treasury | Development funding |
| Bridge Reserve | Per Bridge | 0.05 XAH | Bridge maintenance |
| Daily Rewards | Per Claim | 0.05 XAH | Claim processing |
| NoteHook | None | - | Data storage (no contributions) |
| Savings Manager | None | - | Financial discipline (no contributions) |
| And more... | Varies | Varies | Check individual hook docs |

**All contributions are hardcoded and transparent in the source code. They are only charged on successful operations and do not affect your token amounts or core functionality.**

## 🔗 Network Availability

### Mainnet
- **Network**: Xahau Mainnet
- **Status**: Production Ready
- **Use Case**: Live applications, real value transactions

### Testnet  
- **Network**: Xahau Testnet
- **Status**: Testing Environment
- **Use Case**: Development, testing, experimentation

## 📖 Documentation Standards

Each hook includes comprehensive documentation:
- **README.md**: Complete installation and usage guide
- **Source Code**: Fully commented C implementation
- **Examples**: Real transaction examples
- **Parameters**: Detailed parameter specifications
- **Error Handling**: Complete error code reference

## 🛠️ Development Resources

### Tools & Utilities
- **[Xahau Hooks Builder](https://hooks-builder.xrpl.org/develop)**: Compile and deploy hooks
- **[Xahau Hooks Technical](https://xrpl-hooks.readme.io/reference/hook-api-conventions)**: API reference
- **[Message HEX String](https://transia-rnd.github.io/xrpl-hex-visualizer/)**: Parameter converter
- **[XahauExplorer](https://xahau.xrplwin.com/)**: Transaction monitoring

### Community Support
- **GitHub Issues**: Report bugs and request features
- **Documentation**: Detailed guides in each hook folder
- **Open Source**: All code available for review and learning

## ⚠️ Important Considerations

### Before Installing
1. **Review Developer Contributions**: Understand any contributions for your chosen hook.
2. **Test First**: Use testnet before mainnet deployment.
3. **Read Documentation**: Thoroughly review the hook's README.
4. **Parameter Preparation**: Ensure you have all required installation parameters.
5. **Account Security**: Understand the hook's access control mechanisms.

### Security Notes
- Hooks modify your account's transaction processing.
- Only install hooks you understand completely.
- Test all functionality on testnet first.
- Keep your account secure with proper key management.

## 🤝 Contributing to HandyHooks

While these production hooks may include developer contributions, we welcome:
- **Bug Reports**: Help us improve reliability.
- **Documentation**: Enhance guides and examples.
- **Feature Requests**: Suggest new functionality.
- **Community Hooks**: Contribute to our broader collection.

## 📄 License & Legal
- **License**: Open source (see individual LICENSE files).
- **Developer Contributions**: Hardcoded for development funding.
- **Transparency**: All contributions disclosed in documentation.
- **Usage**: Free to use with disclosed contributions.

## 🎯 Getting Started
1. **Browse Categories**: Review the available hook types above.
2. **Select a Hook**: Choose based on your application needs.
3. **Explore Subdirectories**: Navigate to the specific hook's folder for detailed README and source code.
4. **Test on Testnet**: Deploy and test before mainnet use.
5. **Go Live**: Deploy to mainnet when ready.

---

*The HandyHooks collection represents months of development and testing. Developer contributions support continued innovation and maintenance of these valuable tools for the Xahau ecosystem.*

For detailed information on any specific hook, navigate to its subdirectory and review the comprehensive README and source code provided.
