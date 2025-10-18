# AlgoIoT - Algorand IoT Library for ESP32

A lightweight Arduino library for ESP32 microcontrollers that enables direct interaction with the Algorand blockchain for IoT applications.

## Overview

AlgoIoT allows ESP32 devices to:
- Submit payment transactions with sensor data
- Create assets on Algorand blockchain
- Opt-in to existing assets
- Opt-in to smart contract applications
- Store IoT data permanently on blockchain

## Features

- **Payment Transactions**: Send Algos with sensor data in note field
- **Asset Creation**: Create new Algorand Standard Assets (ASAs)
- **Asset Opt-in**: Opt into existing assets for transfers
- **Application Opt-in**: Opt into smart contracts/applications
- **ARC-2 Compliance**: JSON data format in transaction notes
- **Testnet/Mainnet Support**: Switch between networks
- **Ed25519 Signatures**: Cryptographic transaction signing
- **MessagePack Encoding**: Efficient binary serialization

## Quick Start

### Hardware Requirements
- ESP32 microcontroller
- WiFi connection
- Optional: BME280 sensor for real data

### Software Dependencies
- ArduinoJson library
- Crypto library
- Base64 library
- HTTPClient (ESP32)

### Basic Usage

```cpp
#include <AlgoIoT.h>

// Initialize with app name and 25-word mnemonic
AlgoIoT algoIoT("MyIoTApp", "your 25 word mnemonic phrase here...");

void setup() {
  // Add sensor data
  algoIoT.dataAddFloatField("temperature", 25.5);
  algoIoT.dataAddUInt8Field("humidity", 60);
  
  // Submit to blockchain
  int result = algoIoT.submitTransactionToAlgorand();
}
```

## Transaction Types Implemented

### 1. Payment Transaction ✅
- **Purpose**: Send Algos with IoT data
- **Status**: Working
- **Use Case**: Regular sensor data logging

### 2. Asset Creation ✅
- **Purpose**: Create new assets on blockchain
- **Status**: Working
- **Use Case**: Create tokens representing IoT devices/data

### 3. Asset Opt-in ✅
- **Purpose**: Enable asset transfers to account
- **Status**: Working
- **Use Case**: Prepare account to receive specific assets

### 4. Application Opt-in ❌
- **Purpose**: Opt into smart contracts
- **Status**: Not working (implementation issue)
- **Use Case**: Interact with dApps

## Configuration

Edit these settings in `Algo.ino`:

```cpp
#define MYWIFI_SSID "your_wifi_name"
#define MYWIFI_PWD "your_wifi_password"
#define DAPP_NAME "YourAppName"
#define NODE_ACCOUNT_MNEMONICS "your 25 word mnemonic..."
#define USE_TESTNET  // Comment for mainnet
```

## Network Support

- **Testnet**: Free testing environment (default)
- **Mainnet**: Production network (costs real Algos)

## Data Format

Sensor data is stored in transaction notes using ARC-2 JSON format:
```json
{
  "NodeSerialNum": 1234567890,
  "Temperature(°C)": 25.5,
  "RelHumidity(%)": 60,
  "Pressure(mbar)": 1013
}
```

## Error Codes

- `0`: Success
- `1`: Null pointer error
- `2`: JSON error
- `6`: Network error
- `9`: Transaction error

## File Structure

- `AlgoIoT.h` - Library header with class definition
- `AlgoIoT.cpp` - Core implementation
- `Algo.ino` - Example Arduino sketch
- `minmpk.h` - MessagePack encoding utilities
- `base32decode.h` - Address decoding
- `bip39enwords.h` - Mnemonic word list

## Security Notes

⚠️ **Important**: Mnemonic phrases are private keys. Never share them or commit to version control.

## License

Apache License 2.0 - See file headers for details.

## Author

Fernando Carello for GT50 S.r.l.