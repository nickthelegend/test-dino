// AlgoIoT.h
// header for AlgoIoT library

// requires "minmpk" MessagePack library (included)
// requires ArduinoJSON by Benoit Blanchon
// requires Crypto library
// requires HTTPClient (ESP32)
// requires Base64 by Densaugeo https://github.com/Densaugeo/base64_arduino

// v20240415-1

// TODO:
// API endpoint URL setter (AlgoNode may have to be replaced at some point)
// API token setter (AlgoNode does not require one for Testnet)

/* By Fernando Carello for GT50
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License governing permissions and limitations under the License.
 * */


#ifndef __ALGOIOT_H
#define __ALGOIOT_H

#include <Arduino.h>
#include <stdint.h>
#include <HTTPClient.h>   // https://github.com/espressif/arduino-esp32/blob/master/libraries/HTTPClient/src/HTTPClient/HTTPClient.h
#include <ArduinoJson.h>  // JSON needed for Algorand transactions. ArduinoJson because: https://arduinojson.org/news/2019/11/19/arduinojson-vs-arduino_json/
#include "minmpk.h"
// #include "algoiot_user_config.h"

#define BLANK_MSGPACK_HEADER 75  // We leave this space at the head of the buffer, so we can add the m_signature later
#define JSON_ENCODING_MARGIN 64
#define ALGORAND_POST_MIME_TYPE "application/msgpack"
#define ALGORAND_MAX_RESPONSE_LEN 320      // For Algorand transaction params. Max measured = 250, but ArduinoJSON apparently needs quite a margin (272 bytes proved too small)
#define ALGORAND_MAX_TX_MSGPACK_SIZE 1280  // 1253 max measured for payment transaction   
#define ALGORAND_MAX_NOTES_SIZE 1000
#define ALGORAND_TRANSACTION_PREFIX "TX"
#define ALGORAND_TRANSACTION_PREFIX_BYTES 2
#define ALGORAND_TRANSACTIONID_SIZE 64
#define ALGORAND_TESTNET 0
#define ALGORAND_MAINNET 1
#define ALGORAND_NETWORK_ID_CHARS 12
#define ALGORAND_API_ENDPOINT_CHARS 128
#define ALGORAND_API_TOKEN_CHARS 32
#define ALGORAND_TESTNET_ID "testnet-v1.0"
#define ALGORAND_TESTNET_HASH "SGO1GKSzyE7IEPItTxCByw9x8FmnrCDexi9/cOUJOiI="
#define ALGORAND_TESTNET_API_ENDPOINT "https://testnet-api.algonode.cloud"  // Algonode Testnet API
#define ALGORAND_MAINNET_ID "mainnet-v1.0"
#define ALGORAND_MAINNET_HASH "wGHE2Pwdvd7S12BL5FaOP20EGYesN73ktiC1qzkkit8="
#define ALGORAND_MAINNET_API_ENDPOINT "https://mainnet-api.algonode.cloud"  // Algonode Testnet API
#define ALGORAND_PAYMENT_TRANSACTION_MIN_FIELDS 9 // without "note", otherwise 10 (not counting "sig" which is separate from txn Map)
#define ALGORAND_ADDRESS_BYTES 32
#define ALGORAND_KEY_BYTES 32
#define ALGORAND_SIG_BYTES 64
#define ALGORAND_NET_HASH_BYTES 32
#define ALGORAND_MNEMONICS_NUMBER 25
#define ALGORAND_MNEMONIC_MIN_LEN 3
#define ALGORAND_MNEMONIC_MAX_LEN 8
#define NOTE_LABEL_MAX_LEN 31
#define DAPP_NAME_MAX_LEN NOTE_LABEL_MAX_LEN
#define GET_TRANSACTION_PARAMS "/v2/transactions/params"
#define POST_TRANSACTION "/v2/transactions"
#define ALGORAND_MAX_WAIT_ROUNDS 1000
#define ALGORAND_MIN_PAYMENT_MICROALGOS 1 
#ifndef RECEIVER_ADDRESS
  #define RECEIVER_ADDRESS ""
#endif  

#define PAYMENT_AMOUNT_MICROALGOS 100000	// Please check vs. ALGORAND_MIN_PAYMENT_MICROALGOS in .ino

#define HTTP_CONNECT_TIMEOUT_MS 5000UL
#define HTTP_QUERY_TIMEOUT_S 5

#define ALGORAND_ASSET_TRANSFER_MIN_FIELDS 10 // Fields for asset transfer transaction: aamt, arcv, fee, fv, gen, gh, lv, snd, type, xaid
#define DEFAULT_ASSET_ID 733709260 // Default asset ID to use for asset transfers
#define ALGORAND_APPLICATION_OPTIN_MIN_FIELDS 9 // Fields for application opt-in: apan, apid, fee, fv, gen, gh, lv, snd, type
#define DEFAULT_APPLICATION_ID 738608433 // Default application ID to use for application opt-ins

// Add these constants after the existing constants
#define ALGORAND_ASSET_CREATION_MIN_FIELDS 10 // Fields for asset creation: apar, fee, fv, gen, gh, lv, snd, type
#define DEFAULT_ASSET_TOTAL 1 // Default total supply for created assets
#define ALGORAND_APPLICATION_NOOP_MIN_FIELDS 7 // Fields for application NoOp: apid, fee, fv, gen, gh, lv, snd, type (minimum fields)
#define DEFAULT_APPLICATION_NOOP_ID 51 // Default application ID for NoOp calls
#define ALGORAND_ASSET_OPTOUT_MIN_FIELDS 10 // Fields for asset opt-out: aclose, arcv, fee, fv, gen, gh, lv, snd, type, xaid
#define ALGORAND_ASSET_FREEZE_MIN_FIELDS 10 // Fields for asset freeze: afrz, fadd, faid, fee, fv, gen, gh, lv, snd, type
#define ALGORAND_ASSET_DESTROY_MIN_FIELDS 7 // Fields for asset destroy: caid, fee, fv, gen, gh, lv, snd, type


// Error codes
#define ALGOIOT_NO_ERROR 0
#define ALGOIOT_NULL_POINTER_ERROR 1
#define ALGOIOT_JSON_ERROR 2
#define ALGOIOT_BAD_PARAM 3
#define ALGOIOT_MEMORY_ERROR 4
#define ALGOIOT_INTERNAL_GENERIC_ERROR 5
#define ALGOIOT_NETWORK_ERROR 6
#define ALGOIOT_MESSAGEPACK_ERROR 7
#define ALGOIOT_SIGNATURE_ERROR 8
#define ALGOIOT_TRANSACTION_ERROR 9
#define ALGOIOT_DATA_STRUCTURE_TOO_LONG 10


// AlgoIoT class
class AlgoIoT
{
  private:
  // Private vars
  HTTPClient m_httpClient;
  char m_appName[DAPP_NAME_MAX_LEN + 1] = "";
  String m_httpBaseURL = ALGORAND_TESTNET_API_ENDPOINT;
  char APItoken[ALGORAND_API_TOKEN_CHARS + 1] = "";
  StaticJsonDocument <ALGORAND_MAX_NOTES_SIZE + JSON_ENCODING_MARGIN>m_noteJDoc;  // TO BE TESTED with complete 1000-bytes note field
  char m_transactionID[ALGORAND_TRANSACTIONID_SIZE + 1] = "";
  uint8_t m_networkType = ALGORAND_TESTNET;
  uint8_t m_privateKey[ALGORAND_KEY_BYTES];
  uint8_t m_senderAddressBytes[ALGORAND_KEY_BYTES]; // = public key
  uint8_t* m_pvtKey = NULL;
  uint8_t* m_receiverAddressBytes = NULL;
  uint8_t* m_netHash = NULL;
  uint16_t m_noteOffset = 0;
  uint16_t m_noteLen = 0;
  
  // Decodes Base32 Algorand address to 32-byte binary address suitable for our functions
  // outBinaryAddress allocated internally, has to be freed by caller
  // Returns error code (0 = OK)
  int decodeAlgorandAddress(const char* addressB32, uint8_t*& outBinaryAddress);


  // Decodes Base64 Algorand network hash to 32-byte binary buffer suitable for our functions
  // outBinaryHash allocated internally, has to be freed by caller
  // Returns error code (0 = OK)
  int decodeAlgorandNetHash(const char* hashB64, uint8_t*& outBinaryHash);


  // Accepts a C string containing space-delimited mnemonic words (25 words)
  // outm_privateKey allocated internally, has to be freed by caller
  // Returns error code (0 = OK)
  int decodePrivateKeyFromMnemonics(const char* mnemonicWords, uint8_t out_privateKey[ALGORAND_KEY_BYTES]);


  // 1. Retrieves current Algorand transaction parameters
  // Returns HTTP response code (200 = OK)
  int getAlgorandTxParams(uint32_t* round, uint16_t* minFee);

  // msgPack passed by caller (not allocated internally):

  // 2. Fills Algorand transaction MessagePack
  // Returns error code (0 = OK)
  // "notes" max 1000 bytes
  int prepareTransactionMessagePack(msgPack msgPackTx,
                                  const uint32_t lastRound, 
                                  const uint16_t fee, 
                                  const uint32_t paymentAmountMicroAlgos,
                                  const char* notes,
                                  const uint16_t notesLen);

  // 4. Gets Ed25519 m_signature of binary pack (to which it internally prepends "TX" prefix)
  // Caller passes a 64-bytes buffer in "signature"
  // Returns error code (0 = OK)
  int signMessagePackAddingPrefix(msgPack msgPackTx, uint8_t signature[ALGORAND_SIG_BYTES]);


  // 5. Adds signature to transaction and modifies messagepack accordingly
  // Returns error code (0 = OK)
  int createSignedBinaryTransaction(msgPack msgPackTx, const uint8_t signature[ALGORAND_SIG_BYTES]);


  // 6. Submits transaction to algod
  // Last method to be called, after all the others
  // Returns HTTP response code (200 = OK)
  int submitTransaction(msgPack msgPackTx); 

  // Prepares an asset transfer transaction MessagePack for opt-in
  // Returns error code (0 = OK)
  int prepareAssetTransferMessagePack(msgPack msgPackTx,
                                  const uint32_t lastRound, 
                                  const uint16_t fee,
                                  const uint64_t assetId);
                                  
  // Debug function to print MessagePack content
  void debugPrintMessagePack(msgPack msgPackTx);

  // Prints transaction data in a readable string format
  void printTransactionData(msgPack msgPackTx);

  // Add this function declaration to the AlgoIoT class in the private section
  void debugMessagePackAtPosition(msgPack msgPackTx, uint32_t errorPosition);

  // Prepares an application opt-in transaction MessagePack
  // Returns error code (0 = OK)
  int prepareApplicationOptInMessagePack(msgPack msgPackTx,
                                  const uint32_t lastRound, 
                                  const uint16_t fee,
                                  const uint64_t applicationId);

  // Add this method declaration to the private section of the AlgoIoT class
  // Prepares an asset creation transaction MessagePack
  // Returns error code (0 = OK)
  int prepareAssetCreationMessagePack(
    msgPack msgPackTx,
    const uint32_t lastRound, 
    const uint16_t fee,
    const char* assetName,
    const char* unitName,
    const char* assetURL,
    uint8_t decimals,
    const uint64_t total);

  // Prepares an application NoOp transaction MessagePack
  // Returns error code (0 = OK)
  int prepareApplicationNoOpMessagePack(
    msgPack msgPackTx,
    const uint32_t lastRound, 
    const uint16_t fee,
    const uint64_t applicationId,
    const char** appArgs = NULL,
    uint8_t appArgsCount = 0,
    const uint64_t* foreignAssets = NULL,
    uint8_t foreignAssetsCount = 0,
    const uint64_t* foreignApps = NULL,
    uint8_t foreignAppsCount = 0,
    const char** accounts = NULL,
    uint8_t accountsCount = 0);

  // Prepares an asset opt-out transaction MessagePack
  // Returns error code (0 = OK)
  int prepareAssetOptOutMessagePack(
    msgPack msgPackTx,
    const uint32_t lastRound, 
    const uint16_t fee,
    const uint64_t assetId,
    const char* closeToAddress);

  // Prepares an asset freeze transaction MessagePack
  // Returns error code (0 = OK)
  int prepareAssetFreezeMessagePack(
    msgPack msgPackTx,
    const uint32_t lastRound, 
    const uint16_t fee,
    const uint64_t assetId,
    const char* freezeAddress,
    bool freeze);

  // Prepares an asset destroy transaction MessagePack
  // Returns error code (0 = OK)
  int prepareAssetDestroyMessagePack(
    msgPack msgPackTx,
    const uint32_t lastRound, 
    const uint16_t fee,
    const uint64_t assetId);


  public:

  ///////////////////
  // Public methods
  ///////////////////

  // Constructor
  // "appName" not null and 31 chars max
  // "algoAccountWords" is a string containing the 25 words which encode the Algorand account private key in BIP-39
  AlgoIoT(const char* appName, const char* algoAccountWords);

  // By default, destination address = this device address (transaction to self). This saves transaction fee
  // User may need a different destination address (Smart Contract, collector address, ...)
  // "algorandAddress" not null and precisely 58 chars long
  // Return: error code (0 = OK)
  int setDestinationAddress(const char* algorandAddress);

  // By default, the library operates on the free Algorand Testnet (ALGORAND_TESTNET) 
  // User may add some (free) test currency to the device account using Dispensers like https://testnet.algoexplorer.io/dispenser
  // If switching to ALGORAND_MAINNET, however, real currency is involved and the user will have to purchase real Algos
  // "networkType" has to be either ALGORAND_TESTNET or ALGORAND_MAINNET
  // Return: error code (0 = OK)
  int setAlgorandNetwork(const uint8_t networkType);

  // Returns the ID of the transaction submitted to the Algorand blockchain (if successfully submitted), or an empty string
  const char* getTransactionID();

  // Methods to add data fields (with labels) to the transaction
  // We explicitely provide different methods for each data type (instead a single method with dynamic type)
  // because we do not support each possible data type: only the following ones
  // "label" not null and 31 chars max

  // Return: error code (0 = OK)
  int dataAddInt8Field(const char* label, const int8_t value);

  // Return: error code (0 = OK)
  int dataAddUInt8Field(const char* label, const uint8_t value);

  // Return: error code (0 = OK)
  int dataAddInt16Field(const char* label, const int16_t value);

  // Return: error code (0 = OK)
  int dataAddUInt16Field(const char* label, const uint16_t value);

  // Return: error code (0 = OK)
  int dataAddInt32Field(const char* label, const int32_t value);

  // Return: error code (0 = OK)
  int dataAddUInt32Field(const char* label, const uint32_t value);

  // Return: error code (0 = OK)
  int dataAddFloatField(const char* label, const float value);

  // Max 31 chars
  int dataAddShortStringField(const char* label, char* shortCString);

  // Submit transaction to Algorand network
  // Return: error code (0 = OK)
  int submitTransactionToAlgorand();

  // Submit asset opt-in transaction to Algorand network
  // Return: error code (0 = OK)
  int submitAssetOptInToAlgorand(uint64_t assetId = DEFAULT_ASSET_ID);

  // Submit application opt-in transaction to Algorand network
  // Return: error code (0 = OK)
  int submitApplicationOptInToAlgorand(uint64_t applicationId = DEFAULT_APPLICATION_ID);

  // Submit asset creation transaction to Algorand network
  // Return: error code (0 = OK)
  int submitAssetCreationToAlgorand(
    const char* assetName, 
    const char* unitName, 
    const char* assetURL = NULL,
    uint8_t decimals = 0,
    uint64_t total = DEFAULT_ASSET_TOTAL);

  // Submit asset opt-out transaction to Algorand network
  // Return: error code (0 = OK)
  int submitAssetOptOutToAlgorand(uint64_t assetId, const char* closeToAddress = NULL);

  // Submit asset freeze transaction to Algorand network
  // Return: error code (0 = OK)
  int submitAssetFreezeToAlgorand(uint64_t assetId, const char* freezeAddress, bool freeze);

  // Submit asset destroy transaction to Algorand network
  // Return: error code (0 = OK)
  int submitAssetDestroyToAlgorand(uint64_t assetId);

  // Submit application NoOp transaction to Algorand network
  // Return: error code (0 = OK)
  int submitApplicationNoOpToAlgorand(
    uint64_t applicationId,
    const char** appArgs = NULL, uint8_t appArgsCount = 0,
    const uint64_t* foreignAssets = NULL, uint8_t foreignAssetsCount = 0,
    const uint64_t* foreignApps = NULL, uint8_t foreignAppsCount = 0,
    const char** accounts = NULL, uint8_t accountsCount = 0);




  
  // Add a new public method to get the sender address bytes
  // Returns a pointer to the sender address bytes (public key)
  const uint8_t* getSenderAddressBytes() const;
};

#endif
