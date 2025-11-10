# HandyHooks - Mainnet/Testnet Production Hooks

Welcome to the **HandyHooks Production Collection** - a curated set of Xahau hooks that are live and operational on both mainnet and testnet environments. These hooks are ready for installation using their respective hook hashes and represent battle-tested, production-ready smart contract solutions.

## 🔧 Important Notice About Service Fees

**These hooks contain hardcoded service fees that fund ongoing development of the HandyHooks collection.** All code is published open source with transparent service fee structures. The fees support continued development, maintenance, and creation of new hooks for the community.

- ✅ **Open Source**: All source code is publicly available
- ✅ **Transparent**: Service fees are clearly documented in each hook
- ✅ **Community Focused**: Fees support ongoing development for everyone
- ✅ **Production Ready**: Tested and deployed on live networks

## 📦 Available Hook Categories

### 🏦 Issuance Collection
Advanced token issuance and management hooks for sophisticated DeFi applications.

#### 🏦 Admin Issuance
**Purpose**: Controlled token issuance with whitelist access and automatic treasury allocation
- **Features**: Invoke-only processing, whitelist access control, dynamic destinations, 5% auto-treasury
- **Service Fee**: Built-in treasury allocation mechanism
- **Use Cases**: Project token distribution, controlled minting, treasury management

### 📝 NoteHook
**Purpose**: On-chain note storage and management system
- **Features**: Owner-only access, dynamic note management, on-chain storage
- **Service Fee**: Minimal storage operation fees
- **Use Cases**: Personal notes, documentation, audit trails, simple data storage
- **Status**: ✅ Live on Mainnet/Testnet

### 🛡️ SafeGuard
**Purpose**: Transaction safety and validation system
- **Features**: Amount limits, transaction filtering, safety controls
- **Service Fee**: Security monitoring fees (50,000 drops per operation)
- **Use Cases**: Account protection, transaction limits, security automation
- **Status**: ✅ Live on Mainnet/Testnet

## 🚀 Installation Guide

### Prerequisites
- Active Xahau account with sufficient XAH for hook installation
- Understanding of hook installation process
- Knowledge of transaction parameters for your chosen hook

### Installation Steps

1. **Choose Your Hook**: Select from the categories above based on your needs
2. **Review Documentation**: Each hook folder contains detailed README with:
   - Installation parameters
   - Transaction parameters  
   - Usage examples
   - Service fee structure
3. **Prepare Installation Transaction**: Use the hook hash and required parameters
4. **Deploy**: Submit the SetHook transaction to install on your account
5. **Test**: Verify functionality with test transactions

### Hook Hash Information
Hook hashes for mainnet/testnet deployment are provided in each individual hook's README file.

## 💰 Service Fee Structure

Each hook implements transparent service fees to support ongoing development:

| Hook Category | Fee Type | Amount | Purpose |
|---------------|----------|---------|---------|
| SafeGuard | Per Operation | 50,000 drops | Security monitoring |
| Admin Issuance | Percentage | 5% to treasury | Development funding |
| Bridge Vault | Transaction | Variable | Bridge maintenance |
| Daily Claim | Processing | Minimal | Claim processing |
| NoteHook | Storage | Minimal | Data storage |

**All fees are hardcoded and transparent in the source code.**

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
1. **Understand Service Fees**: Review the fee structure for your chosen hook
2. **Test First**: Use testnet before mainnet deployment
3. **Read Documentation**: Thoroughly review the hook's README
4. **Parameter Preparation**: Ensure you have all required installation parameters
5. **Account Security**: Understand the hook's access control mechanisms

### Security Notes
- Hooks modify your account's transaction processing
- Only install hooks you understand completely
- Test all functionality on testnet first
- Keep your account secure with proper key management

## 🤝 Contributing to HandyHooks

While these production hooks have hardcoded service fees, we welcome:

- **Bug Reports**: Help us improve reliability
- **Documentation**: Enhance guides and examples  
- **Feature Requests**: Suggest new functionality
- **Community Hooks**: Contribute to our broader collection

## 📄 License & Legal

- **License**: Open source (see individual LICENSE files)
- **Service Fees**: Hardcoded for development funding
- **Transparency**: All fees disclosed in documentation
- **Usage**: Free to use with disclosed service fees

## 🎯 Getting Started

1. **Browse Categories**: Review the available hook types above
2. **Select a Hook**: Choose based on your application needs
3. **Read Documentation**: Study the specific hook's README
4. **Test on Testnet**: Deploy and test before mainnet use
5. **Go Live**: Deploy to mainnet when ready

---

*The HandyHooks collection represents months of development and testing. Service fees support continued innovation and maintenance of these valuable tools for the Xahau ecosystem.*

For detailed information on any specific hook, navigate to its subdirectory and review the comprehensive README and source code provided.
