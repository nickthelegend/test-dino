# AlgoIoT Complete Implementation Guide

## Table of Contents
1. [Architecture Overview](#architecture-overview)
2. [Transaction Types Deep Dive](#transaction-types-deep-dive)
3. [MessagePack Implementation](#messagepack-implementation)
4. [Cryptographic Operations](#cryptographic-operations)
5. [Network Communication](#network-communication)
6. [Troubleshooting Guide](#troubleshooting-guide)
7. [Advanced Configuration](#advanced-configuration)

## Architecture Overview

### Core Components

The AlgoIoT library consists of several key components:

```
AlgoIoT Class
├── Private Key Management (BIP-39 mnemonics)
├── MessagePack Serialization
├── Ed25519 Cryptographic Signing
├── HTTP Client (Algorand API)
└── Transaction Builders
    ├── Payment Transactions
    ├── Asset Creation
    ├── Asset Opt-in
    ├── Asset Opt-out
    ├── Asset Freeze
    ├── Asset Destroy
    ├── Application NoOp
    └── Application Opt-in
```

### Data Flow

1. **Initialization**: Decode mnemonic → Generate key pair
2. **Data Collection**: Add sensor data to JSON structure
3. **Transaction Building**: Create MessagePack transaction
4. **Signing**: Ed25519 signature with "TX" prefix
5. **Submission**: HTTP POST to Algorand node
6. **Confirmation**: Parse transaction ID from response

## Transaction Types Deep Dive

### 1. Payment Transaction (Working ✅)

**Purpose**: Send Algos with IoT data in note field

**MessagePack Structure**:
```
{
  "amt": <payment_amount>,     // microAlgos
  "fee": <transaction_fee>,    // microAlgos
  "fv": <first_valid_round>,   // uint32
  "gen": "testnet-v1.0",       // network ID
  "gh": <genesis_hash>,        // 32 bytes
  "lv": <last_valid_round>,    // uint32
  "note": <sensor_data>,       // ARC-2 JSON
  "rcv": <receiver_address>,   // 32 bytes
  "snd": <sender_address>,     // 32 bytes
  "type": "pay"                // transaction type
}
```

**Implementation**: `prepareTransactionMessagePack()`

**Key Features**:
- ARC-2 compliant note field
- Automatic fee calculation
- Self-transactions (sender = receiver) to save fees

### 2. Asset Creation (Working ✅)

**Purpose**: Create new Algorand Standard Assets (ASAs)

**MessagePack Structure**:
```
{
  "apar": {                    // asset parameters
    "an": <asset_name>,        // string (max 32 chars)
    "t": <total_supply>,       // uint64
    "un": <unit_name>          // string (max 8 chars)
  },
  "fee": <transaction_fee>,
  "fv": <first_valid_round>,
  "gen": "testnet-v1.0",
  "gh": <genesis_hash>,
  "lv": <last_valid_round>,
  "snd": <sender_address>,
  "type": "acfg"               // asset config transaction
}
```

**Implementation**: `prepareAssetCreationMessagePack()`

**Parameters**:
- `assetName`: Asset display name (max 32 chars)
- `unitName`: Asset unit symbol (max 8 chars)
- `assetURL`: Optional metadata URL
- `decimals`: Decimal places (0-19)
- `total`: Total supply

### 3. Asset Opt-in (Working ✅)

**Purpose**: Enable account to receive specific assets

**MessagePack Structure**:
```
{
  "arcv": <receiver_address>,  // same as sender for opt-in
  "fee": <transaction_fee>,
  "fv": <first_valid_round>,
  "gen": "testnet-v1.0",
  "gh": <genesis_hash>,
  "lv": <last_valid_round>,
  "snd": <sender_address>,
  "type": "axfer",             // asset transfer
  "xaid": <asset_id>           // asset ID to opt into
}
```

**Implementation**: `prepareAssetTransferMessagePack()`

**Note**: Opt-in is achieved by sending 0 amount of asset to self

### 4. Asset Opt-out (Working ✅)

**Purpose**: Opt out of assets and close asset holdings

**MessagePack Structure**:
```
{
  "aclose": <close_to_address>, // where to send remaining balance
  "arcv": <receiver_address>,  // same as sender for opt-out
  "fee": <transaction_fee>,
  "fv": <first_valid_round>,
  "gen": "testnet-v1.0",
  "gh": <genesis_hash>,
  "lv": <last_valid_round>,
  "snd": <sender_address>,
  "type": "axfer",             // asset transfer
  "xaid": <asset_id>           // asset ID to opt out of
}
```

**Implementation**: `prepareAssetOptOutMessagePack()`

**Parameters**:
- `assetId`: Asset ID to opt out of
- `closeToAddress`: Optional address to receive remaining balance (defaults to sender)

### 5. Asset Freeze (Working ✅)

**Purpose**: Freeze or unfreeze assets for specific addresses

**MessagePack Structure**:
```
{
  "afrz": <freeze_flag>,       // true to freeze, false to unfreeze
  "fadd": <freeze_address>,    // address to freeze/unfreeze
  "faid": <asset_id>,          // asset ID
  "fee": <transaction_fee>,
  "fv": <first_valid_round>,
  "gen": "testnet-v1.0",
  "gh": <genesis_hash>,
  "lv": <last_valid_round>,
  "snd": <sender_address>,
  "type": "afrz"               // asset freeze transaction
}
```

**Implementation**: `prepareAssetFreezeMessagePack()`

**Parameters**:
- `assetId`: Asset ID to freeze/unfreeze
- `freezeAddress`: Address to freeze/unfreeze
- `freeze`: Boolean flag (true = freeze, false = unfreeze)

### 6. Asset Destroy (Working ✅)

**Purpose**: Destroy assets from the Algorand ledger

**MessagePack Structure**:
```
{
  "caid": <asset_id>,          // config asset ID to destroy
  "fee": <transaction_fee>,
  "fv": <first_valid_round>,
  "gen": "testnet-v1.0",
  "gh": <genesis_hash>,
  "lv": <last_valid_round>,
  "snd": <sender_address>,
  "type": "acfg"               // asset config transaction
}
```

**Implementation**: `prepareAssetDestroyMessagePack()`

**Parameters**:
- `assetId`: Asset ID to destroy

**Requirements**:
- Original creator must possess all units of the asset
- Manager must send and authorize the transaction
- Differentiated from asset creation by presence of `caid` field
- Differentiated from asset reconfiguration by absence of asset parameters

### 7. Application NoOp (Working ✅)

**Purpose**: Call smart contract applications without changing lifecycle state

**MessagePack Structure**:
```
{
  "apid": <application_id>,    // application ID to call
  "apaa": [<app_args>],        // optional application arguments
  "apat": [<accounts>],        // optional accounts to reference
  "apas": [<foreign_assets>],  // optional foreign assets
  "apfa": [<foreign_apps>],    // optional foreign applications
  "fee": <transaction_fee>,
  "fv": <first_valid_round>,
  "gen": "testnet-v1.0",
  "gh": <genesis_hash>,
  "lv": <last_valid_round>,
  "snd": <sender_address>,
  "type": "appl"               // application call
}
```

**Implementation**: `prepareApplicationNoOpMessagePack()`

**Parameters**:
- `applicationId`: Application ID to call
- `appArgs`: Optional application arguments (strings)
- `foreignAssets`: Optional foreign asset IDs to reference
- `foreignApps`: Optional foreign application IDs to reference
- `accounts`: Optional accounts to reference

### 7. Application Opt-in (Not Working ❌)

**Purpose**: Opt into smart contract applications

**MessagePack Structure**:
```
{
  "apan": 1,                   // OnComplete: OptIn
  "apid": <application_id>,    // application ID
  "fee": <transaction_fee>,
  "fv": <first_valid_round>,
  "gen": "testnet-v1.0",
  "gh": <genesis_hash>,
  "lv": <last_valid_round>,
  "snd": <sender_address>,
  "type": "appl"               // application call
}
```

**Implementation**: `prepareApplicationOptInMessagePack()`

**Known Issues**:
- MessagePack encoding may have field ordering issues
- Application ID encoding format mismatch
- Missing required fields for application calls

## MessagePack Implementation

### Encoding Strategy

The library uses a custom MessagePack implementation (`minmpk.h`) with these key features:

1. **Header Reservation**: 75-byte header for signature insertion
2. **Field Ordering**: Alphabetical ordering required by Algorand
3. **Type Optimization**: Automatic selection of smallest encoding

### Encoding Functions

```cpp
// String encoding
msgpackAddShortString(msgPack, "field_name");

// Integer encoding (auto-sized)
msgpackAddUInt32(msgPack, value);
msgpackAddUInt64(msgPack, large_value);

// Binary data
msgpackAddShortByteArray(msgPack, data, length);

// Map structures
msgpackAddShortMap(msgPack, field_count);
```

### Debugging MessagePack

The library includes debugging functions:

```cpp
// Print hex dump
debugPrintMessagePack(msgPackTx);

// Analyze specific position
debugMessagePackAtPosition(msgPackTx, error_position);

// Print readable transaction data
printTransactionData(msgPackTx);
```

## Cryptographic Operations

### Key Derivation

1. **Mnemonic Processing**: 25 BIP-39 words → 11-bit indices
2. **Bit Packing**: Convert to 256-bit private key
3. **Public Key**: Ed25519 derivation from private key

```cpp
// Implementation in decodePrivateKeyFromMnemonics()
Ed25519::derivePublicKey(publicKey, privateKey);
```

### Transaction Signing

1. **Prefix Addition**: Prepend "TX" to MessagePack
2. **Ed25519 Signature**: Sign prefixed transaction
3. **Signature Insertion**: Add to MessagePack structure

```cpp
// Signing process
Ed25519::sign(signature, privateKey, publicKey, data, dataLength);
```

### Address Encoding

- **Internal**: 32-byte binary addresses
- **External**: Base32 encoded (58 characters)
- **Conversion**: `decodeAlgorandAddress()`

## Network Communication

### API Endpoints

**Testnet**:
- Base URL: `https://testnet-api.algonode.cloud`
- Get Params: `/v2/transactions/params`
- Submit TX: `/v2/transactions`

**Mainnet**:
- Base URL: `https://mainnet-api.algonode.cloud`
- Same endpoints as testnet

### HTTP Configuration

```cpp
// Timeouts
#define HTTP_CONNECT_TIMEOUT_MS 5000UL
#define HTTP_QUERY_TIMEOUT_S 5

// Content type
#define ALGORAND_POST_MIME_TYPE "application/msgpack"
```

### Response Handling

```cpp
// Success: HTTP 200
{
  "txId": "TRANSACTION_ID_HERE"
}

// Error: HTTP 400
{
  "message": "error description",
  "data": {...}
}
```

## Troubleshooting Guide

### Common Issues

#### 1. Application Opt-in Failures

**Symptoms**: HTTP 400 errors, MessagePack format errors

**Possible Causes**:
- Incorrect field count in root map
- Missing required application call fields
- Wrong OnComplete value encoding

**Debug Steps**:
```cpp
// Add before submission
debugPrintMessagePack(msgPackTx);
printTransactionData(msgPackTx);
```

#### 2. Asset ID Encoding Issues

**Symptoms**: "invalid asset ID" errors

**Solution**: Ensure proper uint32/uint64 encoding
```cpp
if (assetId <= 0xFFFFFFFF) {
    msgpackAddUInt32(msgPackTx, (uint32_t)assetId);
} else {
    msgpackAddUInt64(msgPackTx, assetId);
}
```

#### 3. Network Connection Problems

**Symptoms**: Network error codes, timeouts

**Checks**:
- WiFi connectivity
- API endpoint availability
- Firewall restrictions

#### 4. Signature Verification Failures

**Symptoms**: Invalid signature errors

**Causes**:
- Incorrect mnemonic phrase
- Wrong network hash
- MessagePack corruption

### Debug Output Analysis

Enable debug mode:
```cpp
#define LIB_DEBUGMODE
#define DEBUG_SERIAL Serial
```

Key debug information:
- Private/public key derivation
- MessagePack hex dumps
- HTTP request/response details
- Transaction field analysis

## Advanced Configuration

### Custom Network Settings

```cpp
// Change API endpoint
algoIoT.setAlgorandNetwork(ALGORAND_MAINNET);

// Custom receiver address
algoIoT.setDestinationAddress("ALGORAND_ADDRESS_HERE");
```

### Memory Management

```cpp
// Buffer sizes
#define ALGORAND_MAX_TX_MSGPACK_SIZE 1280
#define ALGORAND_MAX_NOTES_SIZE 1000
#define ALGORAND_MAX_RESPONSE_LEN 320
```

### Fee Optimization

```cpp
// Minimum fees by transaction type
Payment: 1000 microAlgos
Asset Creation: 1000 microAlgos (minimum)
Asset Opt-in: 1000 microAlgos
Asset Opt-out: 1000 microAlgos
Asset Freeze: 1000 microAlgos
Asset Destroy: 1000 microAlgos
Application NoOp: 1000 microAlgos (minimum)
Application Opt-in: 1000 microAlgos
```

### Data Field Limits

```cpp
// Note field constraints
Max note size: 1000 bytes
Label max length: 31 characters
App name max length: 31 characters

// Asset constraints
Asset name: 32 characters max
Unit name: 8 characters max
```

## Performance Considerations

### Memory Usage
- Static allocation preferred over dynamic
- MessagePack buffers pre-allocated
- JSON document size limits enforced

### Network Efficiency
- Connection reuse where possible
- Timeout configuration for reliability
- Error retry mechanisms (not implemented)

### Power Management
- WiFi connection management
- Sleep modes between transactions
- Sensor reading optimization

## Future Improvements

### Application Opt-in Fix
1. Review Algorand application call specification
2. Verify required fields for opt-in transactions
3. Test MessagePack field ordering
4. Add proper error handling

### Enhanced Features
1. Transaction retry mechanisms
2. Multiple network endpoint support
3. Batch transaction submission
4. Advanced asset parameter support
5. Smart contract interaction methods

### Security Enhancements
1. Secure key storage options
2. Hardware security module support
3. Multi-signature transactions
4. Key rotation mechanisms

## Conclusion

The AlgoIoT library provides a solid foundation for IoT blockchain integration with Algorand. While most transaction types work correctly, the application opt-in functionality requires debugging and fixes. The modular architecture allows for easy extension and customization for specific IoT use cases.