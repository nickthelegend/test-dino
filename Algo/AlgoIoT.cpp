// algoiot.cpp
// v20240424-1
// Comments updated 20250905

// Work in progress	
// TODO:
// submitTransactionToAlgorand():
//  check for network errors separately and return appropriate error code
// Max number of attempts connecting to WiFi
// SHA512/256
// Mnemonics checksum (requires SHA512/256)

// By Fernando Carello for GT50
/* Copyright 2023 GT50 S.r.l.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */


#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <Crypto.h>
#include <base64.hpp>    
#include <Ed25519.h>
#include "base32decode.h" // Base32 decoding for Algorand addresses
#include "bip39enwords.h" // BIP39 english words to convert Algorand private key from mnemonics
#include "AlgoIoT.h"

#define LIB_DEBUGMODE
#define DEBUG_SERIAL Serial


// Class AlgoIoT

///////////////////////////////
// Public methods (exported)
///////////////////////////////

// Constructor
AlgoIoT::AlgoIoT(const char* sAppName, const char* nodeAccountMnemonics)
{
  int iErr = 0;

  if (sAppName == NULL)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.println("\n Error: NULL AppName passed to constructor\n");
    #endif
    return;
  }
  if (strlen(sAppName) > DAPP_NAME_MAX_LEN)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.println("\n Error: app name too long\n");
    #endif
    return;
  }
  strcpy(m_appName, sAppName);

  if (nodeAccountMnemonics == NULL)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.println("\n Error: NULL mnemonic words passed to constructor\n");
    #endif
    return;
  }

    // Configure HTTP client
  m_httpClient.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);

  // Decode private key from mnemonics
  iErr = decodePrivateKeyFromMnemonics(nodeAccountMnemonics, m_privateKey);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n Error %d decoding Algorand private key from mnemonic words\n", iErr);
    #endif
    return;
  }

  // Derive public key = sender address ( = this node address) from private key
  Ed25519::derivePublicKey(m_senderAddressBytes, m_privateKey);

  // By default, use current (sender) address as destination address (transaction to self)
  // User may set a different address later, with appropriate setter
  m_receiverAddressBytes = (uint8_t*)malloc(ALGORAND_ADDRESS_BYTES);
  if (!m_receiverAddressBytes)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n Memory error creating receiver address\n", iErr);
    #endif
    return;
  }
  memcpy((void*)(&(m_receiverAddressBytes[0])), m_senderAddressBytes, ALGORAND_ADDRESS_BYTES);
}


int AlgoIoT::setDestinationAddress(const char* algorandAddress)
{
  int iErr = 0;
  
  if (algorandAddress == NULL)
  {
    return ALGOIOT_NULL_POINTER_ERROR;
  }
  if (strlen(algorandAddress) != ALGORAND_ADDRESS_BYTES)
  {
    return ALGOIOT_BAD_PARAM;
  }
  iErr = decodeAlgorandAddress(algorandAddress, m_receiverAddressBytes);
  {
    return ALGOIOT_BAD_PARAM;
  }

  return ALGOIOT_NO_ERROR;
}


int AlgoIoT::setAlgorandNetwork(const uint8_t networkType)
{
  if ( (networkType != ALGORAND_TESTNET) && (networkType != ALGORAND_MAINNET) )
  {
    return ALGOIOT_BAD_PARAM;
  }

  m_networkType = networkType;
  if (m_networkType == ALGORAND_TESTNET)
  {
    m_httpBaseURL = ALGORAND_TESTNET_API_ENDPOINT;
  }
  else
  {
    m_httpBaseURL = ALGORAND_MAINNET_API_ENDPOINT;
  }

  return ALGOIOT_NO_ERROR;
}


const char* AlgoIoT::getTransactionID()
{
  return m_transactionID;
}


// Public methods to add values to be written in the blockchain
// Strongly typed; this helps towards adding ARC-2/MessagePack in the future

int AlgoIoT::dataAddInt8Field(const char* label, const int8_t value)
{
  int len = 0;

  if (label == NULL)
  {
    return ALGOIOT_NULL_POINTER_ERROR;
  }
  if (strlen(label) > NOTE_LABEL_MAX_LEN)
  {
    return ALGOIOT_BAD_PARAM;
  }

  m_noteJDoc[label] = value;
  
  // It is not trivial to anticipate how many chars we are going to add,
  // so we check JSON length after the fact
  len = m_noteOffset + measureJson(m_noteJDoc);
  if (len >= ALGORAND_MAX_NOTES_SIZE)
  {
    return ALGOIOT_DATA_STRUCTURE_TOO_LONG;
  }

  // Update note len
  m_noteLen = len;

  return ALGOIOT_NO_ERROR;
}

int AlgoIoT::dataAddUInt8Field(const char* label, const uint8_t value)
{
  int len = 0;

  if (label == NULL)
  {
    return ALGOIOT_NULL_POINTER_ERROR;
  }
  if (strlen(label) > NOTE_LABEL_MAX_LEN)
  {
    return ALGOIOT_BAD_PARAM;
  }

  m_noteJDoc[label] = value;
  
  // It is not trivial to anticipate how many chars we are going to add,
  // so we check JSON length after the fact
  len = m_noteOffset + measureJson(m_noteJDoc);
  if (len >= ALGORAND_MAX_NOTES_SIZE)
  {
    return ALGOIOT_DATA_STRUCTURE_TOO_LONG;
  }

  // Update note len
  m_noteLen = len;

  return ALGOIOT_NO_ERROR;
}

int AlgoIoT::dataAddInt16Field(const char* label, const int16_t value)
{
  int len = 0;

  if (label == NULL)
  {
    return ALGOIOT_NULL_POINTER_ERROR;
  }
  if (strlen(label) > NOTE_LABEL_MAX_LEN)
  {
    return ALGOIOT_BAD_PARAM;
  }

  m_noteJDoc[label] = value;
  
  // It is not trivial to anticipate how many chars we are going to add,
  // so we check JSON length after the fact
  len = m_noteOffset + measureJson(m_noteJDoc);
  if (len >= ALGORAND_MAX_NOTES_SIZE)
  {
    return ALGOIOT_DATA_STRUCTURE_TOO_LONG;
  }

  // Update note len
  m_noteLen = len;

  return ALGOIOT_NO_ERROR;
}

int AlgoIoT::dataAddUInt16Field(const char* label, const uint16_t value)
{
  int len = 0;

  if (label == NULL)
  {
    return ALGOIOT_NULL_POINTER_ERROR;
  }
  if (strlen(label) > NOTE_LABEL_MAX_LEN)
  {
    return ALGOIOT_BAD_PARAM;
  }

  m_noteJDoc[label] = value;
  
  // It is not trivial to anticipate how many chars we are going to add,
  // so we check JSON length after the fact
  len = m_noteOffset + measureJson(m_noteJDoc);
  if (len >= ALGORAND_MAX_NOTES_SIZE)
  {
    return ALGOIOT_DATA_STRUCTURE_TOO_LONG;
  }

  // Update note len
  m_noteLen = len;

  return ALGOIOT_NO_ERROR;
}

int AlgoIoT::dataAddInt32Field(const char* label, const int32_t value)
{
  int len = 0;

  if (label == NULL)
  {
    return ALGOIOT_NULL_POINTER_ERROR;
  }
  if (strlen(label) > NOTE_LABEL_MAX_LEN)
  {
    return ALGOIOT_BAD_PARAM;
  }

  m_noteJDoc[label] = value;
  
  // It is not trivial to anticipate how many chars we are going to add,
  // so we check JSON length after the fact
  len = m_noteOffset + measureJson(m_noteJDoc);
  if (len >= ALGORAND_MAX_NOTES_SIZE)
  {
    return ALGOIOT_DATA_STRUCTURE_TOO_LONG;
  }

  // Update note len
  m_noteLen = len;

  return ALGOIOT_NO_ERROR;
}

int AlgoIoT::dataAddUInt32Field(const char* label, const uint32_t value)
{
  int len = 0;

  if (label == NULL)
  {
    return ALGOIOT_NULL_POINTER_ERROR;
  }
  if (strlen(label) > NOTE_LABEL_MAX_LEN)
  {
    return ALGOIOT_BAD_PARAM;
  }

  m_noteJDoc[label] = value;
  
  // It is not trivial to anticipate how many chars we are going to add,
  // so we check JSON length after the fact
  len = m_noteOffset + measureJson(m_noteJDoc);
  if (len >= ALGORAND_MAX_NOTES_SIZE)
  {
    return ALGOIOT_DATA_STRUCTURE_TOO_LONG;
  }

  // Update note len
  m_noteLen = len;

  return ALGOIOT_NO_ERROR;
}

int AlgoIoT::dataAddFloatField(const char* label, const float value)
{ 
  int len = 0;

  if (label == NULL)
  {
    return ALGOIOT_NULL_POINTER_ERROR;
  }
  if (strlen(label) > NOTE_LABEL_MAX_LEN)
  {
    return ALGOIOT_BAD_PARAM;
  }

  m_noteJDoc[label] = value;
  
  // It is not trivial to anticipate how many chars we are going to add,
  // so we check JSON length after the fact
  len = m_noteOffset + measureJson(m_noteJDoc);
  if (len >= ALGORAND_MAX_NOTES_SIZE)
  {
    return ALGOIOT_DATA_STRUCTURE_TOO_LONG;
  }

  // Update note len
  m_noteLen = len;

  return ALGOIOT_NO_ERROR;
}

int AlgoIoT::dataAddShortStringField(const char* label, char* shortCString)
{
  int len = 0;

  if ( (label == NULL)||(shortCString == NULL) )
  {
    return ALGOIOT_NULL_POINTER_ERROR;
  }
  if ( (strlen(label) > NOTE_LABEL_MAX_LEN)||(strlen(shortCString) > NOTE_LABEL_MAX_LEN) )
  {
    return ALGOIOT_BAD_PARAM;
  }

  m_noteJDoc[label] = shortCString;
  
  // It is not trivial to anticipate how many chars we are going to add,
  // so we check JSON length after the fact
  len = m_noteOffset + measureJson(m_noteJDoc);
  if (len >= ALGORAND_MAX_NOTES_SIZE)
  {
    return ALGOIOT_DATA_STRUCTURE_TOO_LONG;
  }

  // Update note len
  m_noteLen = len;

  return ALGOIOT_NO_ERROR;
}

// Submit transaction to Algorand network
// Return: error code (0 = OK)
// We have the Note field ready, in ARC-2 JSON format
int AlgoIoT::submitTransactionToAlgorand()
{
  uint32_t fv = 0;
  uint16_t fee = 0;
  int iErr = 0;
  uint8_t signature[ALGORAND_SIG_BYTES];
  char notes[ALGORAND_MAX_NOTES_SIZE + 1] = "";
  uint8_t transactionMessagePackBuffer[ALGORAND_MAX_TX_MSGPACK_SIZE];
  char transactionID[ALGORAND_TRANSACTIONID_SIZE + 1];
  msgPack msgPackTx = NULL;

  
  // Add preamble to ARC-2 note field
  // Write app name and format specifier for ARC-2 (we use the JSON flavour of ARC-2)
  memcpy((void*)&(notes[0]), (void*)m_appName, strlen(m_appName));
  m_noteOffset = strlen(m_appName);
  notes[m_noteOffset++] = ':';
  notes[m_noteOffset++] = 'j';
  m_noteLen += m_noteOffset;

  // Serialize Note field to binary buffer after "<app-name>:j"
  int jlen = serializeJson(m_noteJDoc, (char*) (notes + m_noteOffset), ALGORAND_MAX_NOTES_SIZE);
  if (jlen < 1)
  {
    return ALGOIOT_JSON_ERROR;
  }
  int notesLen = jlen + m_noteOffset;

  // Get current Algorand parameters
  int httpResCode = getAlgorandTxParams(&fv, &fee);
  if (httpResCode != 200)
  {
    return ALGOIOT_NETWORK_ERROR;
  }

  // Prepare transaction structure as MessagePack
  msgPackTx = msgpackInit(&(transactionMessagePackBuffer[0]), ALGORAND_MAX_TX_MSGPACK_SIZE);
  if (msgPackTx == NULL)  
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.println("\n Error initializing transaction MessagePack\n");
    #endif
    return ALGOIOT_MESSAGEPACK_ERROR;
  }  
  iErr = prepareTransactionMessagePack(msgPackTx, fv, fee, PAYMENT_AMOUNT_MICROALGOS, notes, (uint16_t)notesLen);
  if (iErr)
  {
    return ALGOIOT_MESSAGEPACK_ERROR;
  }

  // Payment transaction correctly assembled. Now sign it
  iErr = signMessagePackAddingPrefix(msgPackTx, &(signature[0]));
  if (iErr)
  {
    return ALGOIOT_SIGNATURE_ERROR;
  }

  // Signed OK: now compose payload
  iErr = createSignedBinaryTransaction(msgPackTx, signature);
  if (iErr)
  {
    return ALGOIOT_INTERNAL_GENERIC_ERROR;
  }

  // Payload ready. Now we can submit it via algod REST API
  #ifdef LIB_DEBUGMODE
  DEBUG_SERIAL.println("\nReady to submit transaction to Algorand network");
  DEBUG_SERIAL.println();
  #endif
  
  // Print transaction data in readable format
  #ifdef LIB_DEBUGMODE
  printTransactionData(msgPackTx);
  #endif
  
  iErr = submitTransaction(msgPackTx); // Returns HTTP code
  if (iErr != 200)  // 200 = HTTP OK
  { // Something went wrong
    return ALGOIOT_TRANSACTION_ERROR;
  }
  // OK: our transaction, carrying sensor data in the Note field, 
  // was successfully submitted to the Algorand blockchain
  #ifdef LIB_DEBUGMODE
  DEBUG_SERIAL.print("\t Transaction successfully submitted with ID=");
  DEBUG_SERIAL.println(getTransactionID());
  #endif
  
  return ALGOIOT_NO_ERROR;
}

// Add this implementation after the existing submitTransactionToAlgorand method

// Submit asset opt-in transaction to Algorand network
// Return: error code (0 = OK)
int AlgoIoT::submitAssetOptInToAlgorand(uint64_t assetId)
{
  uint32_t fv = 0;
  uint16_t fee = 0;
  int iErr = 0;
  uint8_t signature[ALGORAND_SIG_BYTES];
  uint8_t transactionMessagePackBuffer[ALGORAND_MAX_TX_MSGPACK_SIZE];
  char transactionID[ALGORAND_TRANSACTIONID_SIZE + 1];
  msgPack msgPackTx = NULL;

  // Get current Algorand parameters
  int httpResCode = getAlgorandTxParams(&fv, &fee);
  if (httpResCode != 200)
  {
    return ALGOIOT_NETWORK_ERROR;
  }

  #ifdef LIB_DEBUGMODE
  DEBUG_SERIAL.printf("\nPreparing asset opt-in transaction for asset ID: %llu\n", assetId);
  DEBUG_SERIAL.printf("First valid round: %u, Fee: %u\n", fv, fee);
  DEBUG_SERIAL.printf("Sender address (first 8 bytes): %02X %02X %02X %02X %02X %02X %02X %02X\n", 
                     m_senderAddressBytes[0], m_senderAddressBytes[1], m_senderAddressBytes[2], m_senderAddressBytes[3],
                     m_senderAddressBytes[4], m_senderAddressBytes[5], m_senderAddressBytes[6], m_senderAddressBytes[7]);
  #endif

  // Prepare transaction structure as MessagePack
  msgPackTx = msgpackInit(&(transactionMessagePackBuffer[0]), ALGORAND_MAX_TX_MSGPACK_SIZE);
  if (msgPackTx == NULL)  
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.println("\n Error initializing transaction MessagePack\n");
    #endif
    return ALGOIOT_MESSAGEPACK_ERROR;
  }  
  
  iErr = prepareAssetTransferMessagePack(msgPackTx, fv, fee, assetId);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n Error %d preparing asset transfer MessagePack\n", iErr);
    #endif
    return ALGOIOT_MESSAGEPACK_ERROR;
  }

  // Debug print the MessagePack content
  #ifdef LIB_DEBUGMODE
  DEBUG_SERIAL.println("\nUnsigned MessagePack content:");
  debugPrintMessagePack(msgPackTx);
  #endif

  // Asset transfer transaction correctly assembled. Now sign it
  iErr = signMessagePackAddingPrefix(msgPackTx, &(signature[0]));
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n Error %d signing MessagePack\n", iErr);
    #endif
    return ALGOIOT_SIGNATURE_ERROR;
  }

  // Debug print the signature
  #ifdef LIB_DEBUGMODE
  DEBUG_SERIAL.println("\nSignature (64 bytes):");
  for (int i = 0; i < ALGORAND_SIG_BYTES; i++) {
    DEBUG_SERIAL.printf("%02X ", signature[i]);
    if ((i + 1) % 16 == 0) DEBUG_SERIAL.println();
  }
  DEBUG_SERIAL.println();
  #endif

  // Signed OK: now compose payload
  iErr = createSignedBinaryTransaction(msgPackTx, signature);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n Error %d creating signed binary transaction\n", iErr);
    #endif
    return ALGOIOT_INTERNAL_GENERIC_ERROR;
  }

  // Debug print the final signed MessagePack
  #ifdef LIB_DEBUGMODE
  DEBUG_SERIAL.println("\nSigned MessagePack content:");
  debugPrintMessagePack(msgPackTx);
  
  // Payload ready. Now we can submit it via algod REST API
  DEBUG_SERIAL.println("\nReady to submit asset opt-in transaction to Algorand network");
  #endif
  
  // Print transaction data in readable format
  #ifdef LIB_DEBUGMODE
  printTransactionData(msgPackTx);
  #endif
  
  iErr = submitTransaction(msgPackTx); // Returns HTTP code
  if (iErr != 200)  // 200 = HTTP OK
  { // Something went wrong
    return ALGOIOT_TRANSACTION_ERROR;
  }
  
  // OK: our transaction for asset opt-in was successfully submitted to the Algorand blockchain
  #ifdef LIB_DEBUGMODE
  DEBUG_SERIAL.print("\t Asset opt-in transaction successfully submitted with ID=");
  DEBUG_SERIAL.println(getTransactionID());
  #endif
  
  return ALGOIOT_NO_ERROR;
}

// Prepares an asset transfer transaction MessagePack for opt-in
// Returns error code (0 = OK)
int AlgoIoT::prepareAssetTransferMessagePack(msgPack msgPackTx,
                                  const uint32_t lastRound, 
                                  const uint16_t fee,
                                  const uint64_t assetId)
{ 
  int iErr = 0;
  char gen[ALGORAND_NETWORK_ID_CHARS + 1] = "";
  uint32_t lv = lastRound + ALGORAND_MAX_WAIT_ROUNDS;
  const char type[] = "axfer";
  uint8_t nFields = ALGORAND_ASSET_TRANSFER_MIN_FIELDS;

  if (msgPackTx == NULL)
    return ALGOIOT_NULL_POINTER_ERROR;
  if (msgPackTx->msgBuffer == NULL)
    return ALGOIOT_INTERNAL_GENERIC_ERROR;
  if ((lastRound == 0) || (fee == 0))
  {
    return ALGOIOT_INTERNAL_GENERIC_ERROR;
  }
  
  #ifdef LIB_DEBUGMODE
  DEBUG_SERIAL.printf("\nPreparing asset transfer with asset ID: %llu\n", assetId);
  #endif
  
  if (m_networkType == ALGORAND_TESTNET)
  { // TestNet
    strncpy(gen, ALGORAND_TESTNET_ID, ALGORAND_NETWORK_ID_CHARS);
    // Decode Algorand network hash
    iErr = decodeAlgorandNetHash(ALGORAND_TESTNET_HASH, m_netHash);
    if (iErr)
    {
      #ifdef LIB_DEBUGMODE
      DEBUG_SERIAL.printf("\n prepareAssetTransferMessagePack(): ERROR %d decoding Algorand network hash\n\n", iErr);
      #endif
      return ALGOIOT_INTERNAL_GENERIC_ERROR;
    }
  }
  else
  { // MainNet
    strncpy(gen, ALGORAND_MAINNET_ID, ALGORAND_NETWORK_ID_CHARS);
    iErr = decodeAlgorandNetHash(ALGORAND_MAINNET_HASH, m_netHash);
    if (iErr)
    {
      #ifdef LIB_DEBUGMODE
      DEBUG_SERIAL.printf("\n prepareAssetTransferMessagePack(): ERROR %d decoding Algorand network hash\n\n", iErr);
      #endif
      return ALGOIOT_INTERNAL_GENERIC_ERROR;
    }
  }
  gen[ALGORAND_NETWORK_ID_CHARS] = '\0';

  // We leave a blank space header so we can add:
  // - "TX" prefix before signing
  // - m_signature field and "txn" node field after signing
  iErr = msgPackModifyCurrentPosition(msgPackTx, BLANK_MSGPACK_HEADER);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareAssetTransferMessagePack(): ERROR %d from msgPackModifyCurrentPosition()\n\n", iErr);
    #endif
    return 5;
  }

  // IMPORTANT: Make sure we include the xaid field in the field count
  // Asset transfer requires 9 fields: arcv, fee, fv, gen, gh, lv, snd, type, xaid
  nFields = 9;  // Change from 9 to 10 to include aamt field
  
  // Add root map
  iErr = msgpackAddShortMap(msgPackTx, nFields); 
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareAssetTransferMessagePack(): ERROR %d adding root map with %d fields\n\n", iErr, nFields);
    #endif
    return 5;
  }

  // Fields must follow alphabetical order

  // "arcv" label (asset receiver - same as sender for opt-in)
  iErr = msgpackAddShortString(msgPackTx, "arcv");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareAssetTransferMessagePack(): ERROR %d adding arcv label\n\n", iErr);
    #endif
    return 5;
  }
  // arcv value (binary buffer) - same as sender for opt-in
  iErr = msgpackAddShortByteArray(msgPackTx, (const uint8_t*)&(m_senderAddressBytes[0]), (const uint8_t)ALGORAND_ADDRESS_BYTES);  
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareAssetTransferMessagePack(): ERROR %d adding arcv value\n\n", iErr);
    #endif
    return 5;
  }

  // "fee" label
  iErr = msgpackAddShortString(msgPackTx, "fee");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareAssetTransferMessagePack(): ERROR %d adding fee label\n\n", iErr);
    #endif
    return 5;
  }
  // fee value
  iErr = msgpackAddUInt16(msgPackTx, fee);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareAssetTransferMessagePack(): ERROR %d adding fee value\n\n", iErr);
    #endif
    return 5;
  }

  // "fv" label
  iErr = msgpackAddShortString(msgPackTx, "fv");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareAssetTransferMessagePack(): ERROR %d adding fv label\n\n", iErr);
    #endif
    return 5;
  }
  // fv value
  iErr = msgpackAddUInt32(msgPackTx, lastRound);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareAssetTransferMessagePack(): ERROR %d adding fv value\n\n", iErr);
    #endif
    return 5;
  }

  // "gen" label
  iErr = msgpackAddShortString(msgPackTx, "gen");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareAssetTransferMessagePack(): ERROR %d adding gen label\n\n", iErr);
    #endif
    return 5;
  }
  // gen string
  iErr = msgpackAddShortString(msgPackTx, gen);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareAssetTransferMessagePack(): ERROR %d adding gen string\n\n", iErr);
    #endif
    return 5;
  }

  // "gh" label
  iErr = msgpackAddShortString(msgPackTx, "gh");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareAssetTransferMessagePack(): ERROR %d adding gh label\n\n", iErr);
    #endif
    return 5;
  }
  // gh value (binary buffer)
  iErr = msgpackAddShortByteArray(msgPackTx, (const uint8_t*)&(m_netHash[0]), (const uint8_t)ALGORAND_NET_HASH_BYTES);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareAssetTransferMessagePack(): ERROR %d adding gh value\n\n", iErr);
    #endif
    return 5;
  }

  // "lv" label
  iErr = msgpackAddShortString(msgPackTx, "lv");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareAssetTransferMessagePack(): ERROR %d adding lv label\n\n", iErr);
    #endif
    return 5;
  }
  // lv value
  iErr = msgpackAddUInt32(msgPackTx, lv);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareAssetTransferMessagePack(): ERROR %d adding lv value\n\n", iErr);
    #endif
    return 5;
  }

  // "snd" label
  iErr = msgpackAddShortString(msgPackTx, "snd");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareAssetTransferMessagePack(): ERROR %d adding snd label\n\n", iErr);
    #endif
    return 5;
  }
  // snd value (binary buffer)
  iErr = msgpackAddShortByteArray(msgPackTx, (const uint8_t*)&(m_senderAddressBytes[0]), (const uint8_t)ALGORAND_ADDRESS_BYTES);  
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareAssetTransferMessagePack(): ERROR %d adding snd value\n\n", iErr);
    #endif
    return 5;
  }

  // "type" label
  iErr = msgpackAddShortString(msgPackTx, "type");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareAssetTransferMessagePack(): ERROR %d adding type label\n\n", iErr);
    #endif
    return 5;
  }
  // type string
  iErr = msgpackAddShortString(msgPackTx, "axfer");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareAssetTransferMessagePack(): ERROR %d adding type string\n\n", iErr);
    #endif
    return 5;
  }

  // "xaid" label
  iErr = msgpackAddShortString(msgPackTx, "xaid");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareAssetTransferMessagePack(): ERROR %d adding xaid label\n\n", iErr);
    #endif
    return 5;
  }
  
  // xaid value - Use UInt32 for asset IDs that fit in 32 bits to match Algo SDK encoding
  #ifdef LIB_DEBUGMODE
  DEBUG_SERIAL.printf("\nAdding asset ID: %llu\n", assetId);
  #endif
  
  // Check if the asset ID fits in a uint32
  if (assetId <= 0xFFFFFFFF) {
    iErr = msgpackAddUInt32(msgPackTx, (uint32_t)assetId);
  } else {
    iErr = msgpackAddUInt64(msgPackTx, assetId);
  }
  
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareAssetTransferMessagePack(): ERROR %d adding xaid value\n\n", iErr);
    #endif
    return 5;
  }
  
  #ifdef LIB_DEBUGMODE
  DEBUG_SERIAL.println("\nAsset transfer MessagePack preparation complete");
  #endif

  // End of messagepack
  return 0;
}

// Debug function to print MessagePack content in hexadecimal format
void AlgoIoT::debugPrintMessagePack(msgPack msgPackTx) {
  #ifdef LIB_DEBUGMODE
  DEBUG_SERIAL.println("\nMessagePack content (hex):");
  for (uint32_t i = 0; i < msgPackTx->currentMsgLen; i++) {
    DEBUG_SERIAL.printf("%02X ", msgPackTx->msgBuffer[i]);
    if ((i + 1) % 16 == 0) {
      DEBUG_SERIAL.println();
    }
  }
  DEBUG_SERIAL.println("\n");
  #endif
}

// Prints transaction data in a readable string format
void AlgoIoT::printTransactionData(msgPack msgPackTx) {
  #ifdef LIB_DEBUGMODE
  DEBUG_SERIAL.println("\n----- TRANSACTION DATA (READABLE FORMAT) -----");
  
  // Skip to the transaction content (after header or after "txn" field if signed)
  uint32_t startPos = 0;
  bool isSigned = false;
  
  // Check if this is a signed transaction (has "sig" and "txn" fields)
  for (uint32_t i = 0; i < 20 && i < msgPackTx->currentMsgLen; i++) {
    // Look for "sig" string in the first few bytes
    if (i + 3 < msgPackTx->currentMsgLen && 
        msgPackTx->msgBuffer[i] == 's' && 
        msgPackTx->msgBuffer[i+1] == 'i' && 
        msgPackTx->msgBuffer[i+2] == 'g') {
      isSigned = true;
      break;
    }
  }
  
  if (isSigned) {
    // For signed transactions, find the "txn" field
    for (uint32_t i = 0; i < msgPackTx->currentMsgLen - 3; i++) {
      if (msgPackTx->msgBuffer[i] == 't' && 
          msgPackTx->msgBuffer[i+1] == 'x' && 
          msgPackTx->msgBuffer[i+2] == 'n') {
        startPos = i + 3; // Skip past "txn"
        break;
      }
    }
  } else {
    // For unsigned transactions, start after the header
    startPos = BLANK_MSGPACK_HEADER;
  }
  
  // Parse and print transaction fields
  DEBUG_SERIAL.println("Transaction Fields:");
  
  // Transaction type
  for (uint32_t i = startPos; i < msgPackTx->currentMsgLen - 4; i++) {
    if (msgPackTx->msgBuffer[i] == 't' && 
        msgPackTx->msgBuffer[i+1] == 'y' && 
        msgPackTx->msgBuffer[i+2] == 'p' && 
        msgPackTx->msgBuffer[i+3] == 'e') {
      // Find the type value (usually "pay" or "axfer")
      char typeStr[10] = {0};
      uint8_t typeLen = 0;
      
      // Skip to the value (usually 5 bytes after "type")
      i += 5;
      
      // Copy the string value
      while (i < msgPackTx->currentMsgLen && typeLen < 9 && 
             msgPackTx->msgBuffer[i] >= 32 && msgPackTx->msgBuffer[i] <= 126) {
        typeStr[typeLen++] = msgPackTx->msgBuffer[i++];
      }
      
      DEBUG_SERIAL.printf("  Type: %s\n", typeStr);
      break;
    }
  }
  
  // Fee
  for (uint32_t i = startPos; i < msgPackTx->currentMsgLen - 3; i++) {
    if (msgPackTx->msgBuffer[i] == 'f' && 
        msgPackTx->msgBuffer[i+1] == 'e' && 
        msgPackTx->msgBuffer[i+2] == 'e') {
      // Fee is usually encoded as a uint16, so look for the value 2-3 bytes after "fee"
      uint16_t fee = 0;
      
      // Skip to the value (usually 3-4 bytes after "fee")
      i += 4;
      
      // Simple extraction - this is a rough approximation
      if (i+1 < msgPackTx->currentMsgLen) {
        fee = (msgPackTx->msgBuffer[i] << 8) | msgPackTx->msgBuffer[i+1];
        DEBUG_SERIAL.printf("  Fee: %u microAlgos\n", fee);
      }
      break;
    }
  }
  
  // First valid round (fv)
  for (uint32_t i = startPos; i < msgPackTx->currentMsgLen - 2; i++) {
    if (msgPackTx->msgBuffer[i] == 'f' && 
        msgPackTx->msgBuffer[i+1] == 'v') {
      // fv is usually encoded as a uint32, so look for the value 2-3 bytes after "fv"
      uint32_t fv = 0;
      
      // Skip to the value (usually 3-5 bytes after "fv")
      i += 3;
      
      // Simple extraction - this is a rough approximation
      if (i+3 < msgPackTx->currentMsgLen) {
        fv = (msgPackTx->msgBuffer[i] << 24) | 
             (msgPackTx->msgBuffer[i+1] << 16) | 
             (msgPackTx->msgBuffer[i+2] << 8) | 
             msgPackTx->msgBuffer[i+3];
        DEBUG_SERIAL.printf("  First Valid Round: %u\n", fv);
      }
      break;
    }
  }
  
  // Last valid round (lv)
  for (uint32_t i = startPos; i < msgPackTx->currentMsgLen - 2; i++) {
    if (msgPackTx->msgBuffer[i] == 'l' && 
        msgPackTx->msgBuffer[i+1] == 'v') {
      // lv is usually encoded as a uint32, so look for the value 2-3 bytes after "lv"
      uint32_t lv = 0;
      
      // Skip to the value (usually 3-5 bytes after "lv")
      i += 3;
      
      // Simple extraction - this is a rough approximation
      if (i+3 < msgPackTx->currentMsgLen) {
        lv = (msgPackTx->msgBuffer[i] << 24) | 
             (msgPackTx->msgBuffer[i+1] << 16) | 
             (msgPackTx->msgBuffer[i+2] << 8) | 
             msgPackTx->msgBuffer[i+3];
        DEBUG_SERIAL.printf("  Last Valid Round: %u\n", lv);
      }
      break;
    }
  }
  
  // For asset transfers, print the asset ID
  for (uint32_t i = startPos; i < msgPackTx->currentMsgLen - 4; i++) {
    if (msgPackTx->msgBuffer[i] == 'x' && 
        msgPackTx->msgBuffer[i+1] == 'a' && 
        msgPackTx->msgBuffer[i+2] == 'i' && 
        msgPackTx->msgBuffer[i+3] == 'd') {
      // xaid could be uint32 or uint64
      uint64_t assetId = 0;
      
      // Skip to the value
      i += 5;
      
      // Check if it's a uint32 or uint64 by looking at the format byte
      if (msgPackTx->msgBuffer[i-1] == 0xCE) {  // uint32 format
        if (i+3 < msgPackTx->currentMsgLen) {
          assetId = (uint32_t)((msgPackTx->msgBuffer[i] << 24) | 
                   (msgPackTx->msgBuffer[i+1] << 16) | 
                   (msgPackTx->msgBuffer[i+2] << 8) | 
                   msgPackTx->msgBuffer[i+3]);
        }
      } else if (msgPackTx->msgBuffer[i-1] == 0xCF) {  // uint64 format
        if (i+7 < msgPackTx->currentMsgLen) {
          assetId = ((uint64_t)msgPackTx->msgBuffer[i] << 56) | 
                    ((uint64_t)msgPackTx->msgBuffer[i+1] << 48) | 
                    ((uint64_t)msgPackTx->msgBuffer[i+2] << 40) | 
                    ((uint64_t)msgPackTx->msgBuffer[i+3] << 32) | 
                    ((uint64_t)msgPackTx->msgBuffer[i+4] << 24) | 
                    ((uint64_t)msgPackTx->msgBuffer[i+5] << 16) | 
                    ((uint64_t)msgPackTx->msgBuffer[i+6] << 8) | 
                    (uint64_t)msgPackTx->msgBuffer[i+7];
        }
      }
      
      DEBUG_SERIAL.printf("  Asset ID: %llu\n", assetId);
      break;
    }
  }
  
  // For payment transactions, print the amount
  for (uint32_t i = startPos; i < msgPackTx->currentMsgLen - 3; i++) {
    if (msgPackTx->msgBuffer[i] == 'a' && 
        msgPackTx->msgBuffer[i+1] == 'm' && 
        msgPackTx->msgBuffer[i+2] == 't') {
      // Amount could be encoded in various ways
      uint32_t amount = 0;
      
      // Skip to the value
      i += 4;
      
      // Simple extraction - this is a rough approximation
      if (i < msgPackTx->currentMsgLen) {
        // Check format byte
        if (msgPackTx->msgBuffer[i-1] < 0x80) {  // positive fixint
          amount = msgPackTx->msgBuffer[i-1];
        } else if (msgPackTx->msgBuffer[i-1] == 0xCC) {  // uint8
          amount = msgPackTx->msgBuffer[i];
        } else if (msgPackTx->msgBuffer[i-1] == 0xCD) {  // uint16
          amount = (msgPackTx->msgBuffer[i] << 8) | msgPackTx->msgBuffer[i+1];
        } else if (msgPackTx->msgBuffer[i-1] == 0xCE) {  // uint32
          amount = (msgPackTx->msgBuffer[i] << 24) | 
                   (msgPackTx->msgBuffer[i+1] << 16) | 
                   (msgPackTx->msgBuffer[i+2] << 8) | 
                   msgPackTx->msgBuffer[i+3];
        }
        
        DEBUG_SERIAL.printf("  Amount: %u microAlgos\n", amount);
      }
      break;
    }
  }
  
  // Print note field if present
  for (uint32_t i = startPos; i < msgPackTx->currentMsgLen - 4; i++) {
    if (msgPackTx->msgBuffer[i] == 'n' && 
        msgPackTx->msgBuffer[i+1] == 'o' && 
        msgPackTx->msgBuffer[i+2] == 't' && 
        msgPackTx->msgBuffer[i+3] == 'e') {
      
      // Skip to the value
      i += 5;
      
      // Check format byte to determine length
      uint16_t noteLen = 0;
      uint32_t noteStart = 0;
      
      if (msgPackTx->msgBuffer[i-1] == 0xC4) {  // bin 8 format
        noteLen = msgPackTx->msgBuffer[i];
        noteStart = i + 1;
      } else if (msgPackTx->msgBuffer[i-1] == 0xC5) {  // bin 16 format
        noteLen = (msgPackTx->msgBuffer[i] << 8) | msgPackTx->msgBuffer[i+1];
        noteStart = i + 2;
      }
      
      if (noteLen > 0 && noteStart + noteLen <= msgPackTx->currentMsgLen) {
        DEBUG_SERIAL.print("  Note: ");
        
        // Print the note content as a string (if printable)
        for (uint16_t j = 0; j < noteLen && j < 100; j++) {  // Limit to 100 chars
          char c = msgPackTx->msgBuffer[noteStart + j];
          if (c >= 32 && c <= 126) {  // Printable ASCII
            DEBUG_SERIAL.print(c);
          } else {
            DEBUG_SERIAL.print('.');  // Replace non-printable with dot
          }
        }
        
        if (noteLen > 100) {
          DEBUG_SERIAL.print("... (truncated)");
        }
        
        DEBUG_SERIAL.println();
      }
      break;
    }
  }
  
  DEBUG_SERIAL.println("----- END TRANSACTION DATA -----\n");
  #endif
}

///////////////////////////
//
// End exported functions
//
///////////////////////////


// Private methods

// Decodes Base64 Algorand network hash to 32-byte binary buffer suitable for our functions
// outBinaryHash has to be freed by caller
// Returns error code (0 = OK)
int AlgoIoT::decodeAlgorandNetHash(const char* hashB64, uint8_t*& outBinaryHash)
{ 
  if (hashB64 == NULL)
    return 1;
  
  int inputLen = strlen(hashB64);
  if (inputLen > encode_base64_length(ALGORAND_NET_HASH_BYTES))
    return 2;
  
  outBinaryHash = (uint8_t*) malloc(ALGORAND_NET_HASH_BYTES);
  
  int hashLen = decode_base64((unsigned char*)hashB64, outBinaryHash);
  if (hashLen != ALGORAND_NET_HASH_BYTES)
  {
    free(outBinaryHash);
    return 3;
  }

  return 0;
}


int AlgoIoT::decodeAlgorandAddress(const char* addressB32, uint8_t*& outBinaryAddress)
{
  if (addressB32 == NULL)
    return 1;
  
  int iLen = Base32::fromBase32((uint8_t*)addressB32, strlen(addressB32), outBinaryAddress);
  if (iLen < ALGORAND_ADDRESS_BYTES + 4)  // Decoded address len from Base32 has to be exactly 36 bytes (but we use only the first 32 bytes)
  {
    free(outBinaryAddress);
    return 2;
  }

  return 0;
}


int AlgoIoT::decodePrivateKeyFromMnemonics(const char* inMnemonicWords, uint8_t privateKey[ALGORAND_KEY_BYTES])
{ 
  uint16_t  indexes11bit[ALGORAND_MNEMONICS_NUMBER];
  uint8_t   decodedBytes[ALGORAND_KEY_BYTES + 3];
  // char      checksumWord[ALGORAND_MNEMONIC_MAX_LEN + 1] = "";  
  char*     mnWord = NULL;
  char*     mnemonicWords = NULL;

  if (inMnemonicWords == NULL)
    return 1;

  int inputLen = strlen(inMnemonicWords);

  // Early sanity check: mnemonicWords contains 25 space-delimited words, each composed by a minimum of 3 chars
  if (inputLen < ALGORAND_MNEMONICS_NUMBER * (ALGORAND_MNEMONIC_MIN_LEN + 1))
    return 2;

  // Copy input to work string
  mnemonicWords = (char*)malloc(inputLen + 1);
  if (!mnemonicWords)
    return 7;
  strcpy(mnemonicWords, inMnemonicWords);

  // Off-cycle
  mnWord = strtok(mnemonicWords, " ");
  if (mnWord == NULL)
  {
    free(mnemonicWords);
    return 3; // Invalid input, does not contain spaces
  }

  // Input parsing loop
  uint8_t index = 0;
  uint8_t found = 0;
  uint16_t pos = 0;
  while (mnWord != NULL) 
  {
    // Check word validity against BIP39 English words
    found = 0;
    pos = 0;
    while ((pos < BIP39_EN_WORDS_NUM) && (!found))
    {
      if (!strcmp(mnWord, BIP39_EN_Wordlist[pos]))
      {
        indexes11bit[index++] = pos;
        found = 1;
      }
      pos++;
    }
    if (!found)
    {
      free(mnemonicWords);
      return 4; // Wrong mnemonics: invalid word
    }
    if (index > ALGORAND_MNEMONICS_NUMBER)
    {
      free(mnemonicWords);
      return 5; // Wrong mnemonics: too many words
    }

    mnWord = strtok(NULL, " "); // strtok with NULL as first argument means it continues to parse the original string
  }

  if (index != ALGORAND_MNEMONICS_NUMBER)
  {
    free(mnemonicWords);
    return 6; // Wrong mnemonics: too few words (we already managed the too much words case)
  }
  
  // We now have an array of ALGORAND_MNEMONICS_NUMBER 16-bit unsigned values, which actually only use 11 bits (0..2047)
  // The last element is a checksum 

  // Save checksum word (not used ATM, see below)
  // strncpy(checksumWord, BIP39_EN_Wordlist[indexes[index-1]], ALGORAND_MNEMONIC_MAX_LEN);

  free(mnemonicWords);

  // We now build a byte array from the uint16_t array: 25 x 11-bits values become 34/35 x 8-bits values

  uint32_t tempInt = 0;
  uint16_t numBits = 0;
  uint16_t destIndex = 0;
  for (uint16_t i = 0; i < ALGORAND_MNEMONICS_NUMBER; i++)
  { 
    // For each 11-bit value, fill appropriate consecutive byte array elements
    tempInt |= (((uint32_t)indexes11bit[i]) << numBits);
    numBits += 11;
    while (numBits >= 8)
    {
      decodedBytes[destIndex] = (uint8_t)(tempInt & 0xff);
      destIndex++;
      tempInt = tempInt >> 8;
      numBits -= 8;
    }
  }
  if (numBits != 0)
  {
    decodedBytes[destIndex] = (uint8_t)(tempInt & 0xff);
  }

  // TODO we do not verify the checksum, because at the moment we miss a viable implementation of SHA512/256

  // Copy key to output array
  memcpy((void*)&(privateKey[0]), (void*)decodedBytes, ALGORAND_KEY_BYTES);

  return 0;
}


// Retrieves current Algorand transaction parameters
// Returns HTTP error code
// TODO: On error codes 5xx (server error), maybe we should retry after 5s?
int AlgoIoT::getAlgorandTxParams(uint32_t* round, uint16_t* minFee)
{
  String httpRequest = m_httpBaseURL + GET_TRANSACTION_PARAMS;
          
  *round = 0;
  *minFee = 0;

  // configure server and url               
  m_httpClient.begin(httpRequest);
    
  int httpResponseCode = m_httpClient.GET();

      
  // httpResponseCode will be negative on error
  if (httpResponseCode < 0)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.print("HTTP GET failed, error: "); DEBUG_SERIAL.println(m_httpClient.errorToString(httpResponseCode).c_str());
    #endif
    return ALGOIOT_INTERNAL_GENERIC_ERROR;
  }
  else
  {
    switch (httpResponseCode)
    {
      case 200:
      {   // No error: let's get the response
        String payload = m_httpClient.getString();
        StaticJsonDocument<ALGORAND_MAX_RESPONSE_LEN> JSONResDoc;
                        
        #ifdef LIB_DEBUGMODE
        DEBUG_SERIAL.println("GetParams server response:");
        DEBUG_SERIAL.println(payload);
        #endif

        DeserializationError error = deserializeJson(JSONResDoc, payload);                
        if (error) 
        {
          #ifdef LIB_DEBUGMODE
          DEBUG_SERIAL.println("GetParams: JSON response parsing failed!");
          #endif
          return ALGOIOT_INTERNAL_GENERIC_ERROR;
        }
        else
        { // Fetch interesting fields
          *minFee = JSONResDoc["min-fee"];
          *round = JSONResDoc["last-round"];

          #ifdef LIB_DEBUGMODE
          DEBUG_SERIAL.println("Algorand transaction parameters received:");
          DEBUG_SERIAL.print("min-fee = "); DEBUG_SERIAL.print(*minFee); DEBUG_SERIAL.println(" microAlgo");
          DEBUG_SERIAL.print("last-round = "); DEBUG_SERIAL.println(*round);                  
          #endif                  
        }
      }
      break;
      case 204:
      {   // No error, but no data available from server
        #ifdef LIB_DEBUGMODE
        DEBUG_SERIAL.println("Server returned no data");
        #endif
        return ALGOIOT_NETWORK_ERROR;
      }
      break;
      default:
      {
        #ifdef LIB_DEBUGMODE
        DEBUG_SERIAL.print("Unmanaged HTTP response code "); DEBUG_SERIAL.println(httpResponseCode);
        #endif
        return ALGOIOT_INTERNAL_GENERIC_ERROR;
      }
      break;
    }
  }        
  
  m_httpClient.end();

  return httpResponseCode;
}



// To be called AFTER getAlgorandTxParams(), because we need current "min-fee" and "last-round" values from algod
// Returns error code (0 = OK)
int AlgoIoT::prepareTransactionMessagePack(msgPack msgPackTx,
                                  const uint32_t lastRound, 
                                  const uint16_t fee, 
                                  const uint32_t paymentAmountMicroAlgos,
                                  const char* notes,
                                  const uint16_t notesLen)
{ 
  int iErr = 0;
  char gen[ALGORAND_NETWORK_ID_CHARS + 1] = "";
  uint32_t lv = lastRound + ALGORAND_MAX_WAIT_ROUNDS;
  const char type[] = "pay";
  uint8_t nFields = ALGORAND_PAYMENT_TRANSACTION_MIN_FIELDS;

  if (msgPackTx == NULL)
    return ALGOIOT_NULL_POINTER_ERROR;
  if (msgPackTx->msgBuffer == NULL)
    return ALGOIOT_INTERNAL_GENERIC_ERROR;
  if ((lastRound == 0) || (fee == 0) || (paymentAmountMicroAlgos < ALGORAND_MIN_PAYMENT_MICROALGOS))
  {
    return ALGOIOT_INTERNAL_GENERIC_ERROR;
  }
  
  if ( (notes != NULL) && (notesLen > 0) )
    nFields++;  // We have 9 fields without Note, 10 with Note

  if (m_networkType == ALGORAND_TESTNET)
  { // TestNet
    strncpy(gen, ALGORAND_TESTNET_ID, ALGORAND_NETWORK_ID_CHARS);
    // Decode Algorand network hash
    iErr = decodeAlgorandNetHash(ALGORAND_TESTNET_HASH, m_netHash);
    if (iErr)
    {
      #ifdef LIB_DEBUGMODE
      DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d decoding Algorand network hash\n\n", iErr);
      #endif
      return ALGOIOT_INTERNAL_GENERIC_ERROR;
    }
  }
  else
  { // MainNet
    strncpy(gen, ALGORAND_MAINNET_ID, ALGORAND_NETWORK_ID_CHARS);
    iErr = decodeAlgorandNetHash(ALGORAND_MAINNET_HASH, m_netHash);
    if (iErr)
    {
      #ifdef LIB_DEBUGMODE
      DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d decoding Algorand network hash\n\n", iErr);
      #endif
      return ALGOIOT_INTERNAL_GENERIC_ERROR;
    }
  }
  gen[ALGORAND_NETWORK_ID_CHARS] = '\0';


  // We leave a blank space header so we can add:
  // - "TX" prefix before signing
  // - m_signature field and "txn" node field after signing
  iErr = msgPackModifyCurrentPosition(msgPackTx, BLANK_MSGPACK_HEADER);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d from msgPackModifyCurrentPosition()\n\n");
    #endif

    return 5;
  }

  // Add root map
  iErr = msgpackAddShortMap(msgPackTx, nFields); 
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding root map\n\n");
    #endif

    return 5;
  }

  // Fields must follow alphabetical order

  // "amt" label
  iErr = msgpackAddShortString(msgPackTx, "amt");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding amt label\n\n");
    #endif

    return 5;
  }
  // amt value
  // Note: encoding depends on payment amount
  if (paymentAmountMicroAlgos < 128)
  {
    iErr = msgpackAddUInt7(msgPackTx, (uint8_t)paymentAmountMicroAlgos);
  }
  else
  {
    if (paymentAmountMicroAlgos < 256)
    {
      iErr = msgpackAddUInt8(msgPackTx, (uint8_t)paymentAmountMicroAlgos);
    }
    else
    {
      if (paymentAmountMicroAlgos < 65536)
      {
        iErr = msgpackAddUInt16(msgPackTx, (uint16_t)paymentAmountMicroAlgos);
      }
      else    
      {
        iErr = msgpackAddUInt32(msgPackTx, paymentAmountMicroAlgos);
      }
    }
  }
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding amt value\n\n");
    #endif

    return 5;
  }

  // "fee" label
  iErr = msgpackAddShortString(msgPackTx, "fee");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding fee label\n\n");
    #endif

    return 5;
  }
  // fee value
  iErr = msgpackAddUInt16(msgPackTx, fee);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding fee value\n\n");
    #endif

    return 5;
  }

  // "fv" label
  iErr = msgpackAddShortString(msgPackTx, "fv");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding fv label\n\n");
    #endif

    return 5;
  }
  // fv value
  iErr = msgpackAddUInt32(msgPackTx, lastRound);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding fv value\n\n");
    #endif

    return 5;
  }

  // "gen" label
  iErr = msgpackAddShortString(msgPackTx, "gen");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding gen label\n\n");
    #endif

    return 5;
  }
  // gen string
  iErr = msgpackAddShortString(msgPackTx, gen);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding gen string\n\n");
    #endif

    return 5;
  }

  // "gh" label
  iErr = msgpackAddShortString(msgPackTx, "gh");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding gh label\n\n");
    #endif

    return 5;
  }
  // gh value (binary buffer)
  iErr = msgpackAddShortByteArray(msgPackTx, (const uint8_t*)&(m_netHash[0]), (const uint8_t)ALGORAND_NET_HASH_BYTES);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding gh value\n\n");
    #endif

    return 5;
  }

  // "lv" label
  iErr = msgpackAddShortString(msgPackTx, "lv");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding lv label\n\n");
    #endif

    return 5;
  }
  // lv value
  iErr = msgpackAddUInt32(msgPackTx, lv);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding lv value\n\n");
    #endif

    return 5;
  }

  if ((notes != NULL) && (notesLen > 0))
  {
    // Add "note" label
    iErr = msgpackAddShortString(msgPackTx, "note");
    if (iErr)
    {
      #ifdef LIB_DEBUGMODE
      DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding note label\n\n");
      #endif

      return 5;
    }
    // Add note content as binary buffer
    // WARNING: if note len is < 256, we have to encode Bin 8 so msgpackAddShortByteArray
    // Otherwise, m_signature does not pass verification
    if (notesLen < 256)
      iErr = msgpackAddShortByteArray(msgPackTx, (const uint8_t*)notes, (const uint8_t)notesLen);    
    else
      iErr = msgpackAddByteArray(msgPackTx, (const uint8_t*)notes, (const uint16_t)notesLen);
    if (iErr)
    {
      #ifdef LIB_DEBUGMODE
      DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding notes content\n\n");
      #endif

      return 5;
    }
  }

  // "rcv" label
  iErr = msgpackAddShortString(msgPackTx, "rcv");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding rcv label\n\n");
    #endif

    return 5;
  }
  // rcv value (binary buffer)
  iErr = msgpackAddShortByteArray(msgPackTx, (const uint8_t*)&(m_receiverAddressBytes[0]), (const uint8_t)ALGORAND_ADDRESS_BYTES);  
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding rcv value\n\n");
    #endif

    return 5;
  }

  // "snd" label
  iErr = msgpackAddShortString(msgPackTx, "snd");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding snd label\n\n");
    #endif

    return 5;
  }
  // snd value (binary buffer)
  iErr = msgpackAddShortByteArray(msgPackTx, (const uint8_t*)&(m_senderAddressBytes[0]), (const uint8_t)ALGORAND_ADDRESS_BYTES);  
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding snd value\n\n");
    #endif

    return 5;
  }

  // "type" label
  iErr = msgpackAddShortString(msgPackTx, "type");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding type label\n\n");
    #endif

    return 5;
  }
  // type string
  iErr = msgpackAddShortString(msgPackTx, "pay");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding type string\n\n");
    #endif

    return 5;
  }

  // End of messagepack

  return 0;
}



// Obtains Ed25519 signature of passed MessagePack, adding "TX" prefix; fills "signature" return buffer
// To be called AFTER convertToMessagePack()
// Returns error code (0 = OK)
// Caller passes a 64-byte array in "signature", to be filled
int AlgoIoT::signMessagePackAddingPrefix(msgPack msgPackTx, uint8_t signature[ALGORAND_SIG_BYTES])
{
  uint8_t* payloadPointer = NULL;
  uint32_t payloadBytes = 0;

  if (msgPackTx == NULL)
    return 1;
  if (msgPackTx->msgBuffer == NULL)
    return 2;
  if (msgPackTx->currentMsgLen == 0)
    return 2;

  // We sign from prefix (included), leaving out the rest of the blank header
  payloadPointer = msgPackTx->msgBuffer + BLANK_MSGPACK_HEADER - ALGORAND_TRANSACTION_PREFIX_BYTES;
  payloadBytes = msgPackTx->currentMsgLen + ALGORAND_TRANSACTION_PREFIX_BYTES;

  // Add prefix to messagepack; we purposedly left a blank header, with length BLANK_MSGPACK_HEADER
  payloadPointer[0] = 'T';
  payloadPointer[1] = 'X';

  // Sign pack+prefix
  Ed25519::sign(signature, m_privateKey, m_senderAddressBytes, payloadPointer, payloadBytes);

  return 0;
}


// Add signature to MessagePack. We reserved a blank space header for this purpose
// To be called AFTER signMessagePackAddingPrefix()
// Returns error code (0 = OK)
int AlgoIoT::createSignedBinaryTransaction(msgPack mPack, const uint8_t signature[ALGORAND_SIG_BYTES])
{
  int iErr = 0;
  // When adding the m_signature "sig" field, the messagepack has to be changed into a 2-level structure,
  // with a "txn" node holding existing fields and a new "sig" field directly in root
  // The JSON equivalent would be something like:
  /*
  {
    "sig": <array>,
    "txn": 
    {
     "amt": 10000000,
     "fee": 1000,
     "fv": 32752600,
     "gen": "testnet-v1.0",
      [...]
    }
  }
  */

  // We reset internal msgpack pointer, since we now start from the beginning of the messagepack (filling the blank space)
  iErr = msgPackModifyCurrentPosition(mPack, 0);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n createSignedBinaryTransaction(): ERROR %d resetting position\n\n");
    #endif

    return 5;
  }

  // Add a Map holding 2 fields (sig and txn)
  iErr = msgpackAddShortMap(mPack, 2);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n createSignedBinaryTransaction(): ERROR %d adding map\n\n");
    #endif

    return 5;
  }
  
  // Add "sig" label
  iErr = msgpackAddShortString(mPack, "sig");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n createSignedBinaryTransaction(): ERROR %d adding sign label\n\n");
    #endif

    return 5;
  }

  // Add m_signature, which is a 64-bytes byte array 
  iErr = msgpackAddShortByteArray(mPack, signature, (const uint8_t)ALGORAND_SIG_BYTES);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n createSignedBinaryTransaction(): ERROR %d adding m_signature\n\n");
    #endif

    return 5;
  }

  // Add "txn" label
  iErr = msgpackAddShortString(mPack, "txn");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n createSignedBinaryTransaction(): ERROR %d adding txn label\n\n");
    #endif

    return 5;
  }

  // The rest stays as it is

  return 0;
}


// Submits transaction messagepack to algod
// Last method to be called, after all the others
// Returns http response code (200 = OK) or AlgoIoT error code
// TODO: On error codes 5xx (server error), maybe we should retry after 5s?
int AlgoIoT::submitTransaction(msgPack msgPackTx)
{
  String httpRequest = m_httpBaseURL + POST_TRANSACTION;
          
  // Configure server and url               
  m_httpClient.begin(httpRequest);
  
  // Configure MIME type
  m_httpClient.addHeader("Content-Type", ALGORAND_POST_MIME_TYPE);

  #ifdef LIB_DEBUGMODE
  DEBUG_SERIAL.printf("\nSubmitting transaction to: %s\n", httpRequest.c_str());
  DEBUG_SERIAL.printf("Content-Type: %s\n", ALGORAND_POST_MIME_TYPE);
  DEBUG_SERIAL.printf("Payload size: %d bytes\n", msgPackTx->currentMsgLen);
  #endif

  int httpResponseCode = m_httpClient.POST(msgPackTx->msgBuffer, msgPackTx->currentMsgLen);
      
  // httpResponseCode will be negative on error
  if (httpResponseCode < 0)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.print("\n[HTTP] POST failed, error: "); DEBUG_SERIAL.println(m_httpClient.errorToString(httpResponseCode).c_str());
    #endif
  }
  else
  {
    switch (httpResponseCode)
    {
      case 200:
      {   // No error: let's get the response for debug purposes
        String payload = m_httpClient.getString();
        StaticJsonDocument<ALGORAND_MAX_RESPONSE_LEN> JSONResDoc;
                        
        DeserializationError error = deserializeJson(JSONResDoc, payload);                
        if (error) 
        {
          #ifdef LIB_DEBUGMODE
          DEBUG_SERIAL.println("JSON response parsing failed!");
          DEBUG_SERIAL.println(payload);
          #endif
          return ALGOIOT_INTERNAL_GENERIC_ERROR;
        }
        else
        { // Fetch interesting fields                  
          strncpy(m_transactionID, JSONResDoc["txId"], 64);
          
          #ifdef LIB_DEBUGMODE
          DEBUG_SERIAL.println("Server response:");
          DEBUG_SERIAL.println(payload);
          #endif
        }
      }
      break;
      case 204:
      {   // No error, but no data available from server
        #ifdef LIB_DEBUGMODE
        DEBUG_SERIAL.println("\nServer returned no data");
        #endif
        return ALGOIOT_NETWORK_ERROR;
      }
      break;
      case 400:
      {   // Malformed request
        #ifdef LIB_DEBUGMODE
        DEBUG_SERIAL.println("\nTransaction format error");
        DEBUG_SERIAL.println("Server response:");
        String payload = m_httpClient.getString();
        DEBUG_SERIAL.println(payload);
        
        // Extract the position number from the error message if available
        uint32_t errorPosition = 0;
        if (payload.indexOf("pos ") >= 0) {
          int posStart = payload.indexOf("pos ") + 4;
          int posEnd = payload.indexOf("]", posStart);
          if (posEnd > posStart) {
            String posStr = payload.substring(posStart, posEnd);
            errorPosition = posStr.toInt();
            
            // Debug the MessagePack at the error position
            debugMessagePackAtPosition(msgPackTx, errorPosition);
          }
        } else {
          // If we can't find a specific position, debug around position 242 (from your error)
          debugMessagePackAtPosition(msgPackTx, 242);
        }
        #endif
        return ALGOIOT_TRANSACTION_ERROR;
      }
      break;
      default:
      {
        #ifdef LIB_DEBUGMODE
        DEBUG_SERIAL.print("\nUnmanaged HTTP response code "); DEBUG_SERIAL.println(httpResponseCode);
        String payload = m_httpClient.getString();
        DEBUG_SERIAL.println("Server response:");
        DEBUG_SERIAL.println(payload);
        #endif
        return ALGOIOT_INTERNAL_GENERIC_ERROR;
      }
      break;
    }
  }        
  
  m_httpClient.end();

  return httpResponseCode;
}

// Also add a standalone function to manually debug the MessagePack at any point
void debugMessagePackAtPos(msgPack msgPackTx, uint32_t position) {
  #ifdef LIB_DEBUGMODE
  AlgoIoT dummyInstance("debug", "shadow market lounge gauge battle small crash funny supreme regular obtain require control oil lend reward galaxy tuition elder owner flavor rural expose absent sniff");
  dummyInstance.debugMessagePackAtPosition(msgPackTx, position);
  #endif
}

// Add this debugging function to examine the MessagePack content at a specific position
void AlgoIoT::debugMessagePackAtPosition(msgPack msgPackTx, uint32_t errorPosition) {
  #ifdef LIB_DEBUGMODE
  if (msgPackTx == NULL || msgPackTx->msgBuffer == NULL || errorPosition >= msgPackTx->currentMsgLen) {
    DEBUG_SERIAL.println("Invalid parameters for debugging");
    return;
  }

  // Define the range to examine (20 bytes before and after the error position)
  uint32_t startPos = (errorPosition > 20) ? errorPosition - 20 : 0;
  uint32_t endPos = (errorPosition + 20 < msgPackTx->currentMsgLen) ? errorPosition + 20 : msgPackTx->currentMsgLen - 1;

  DEBUG_SERIAL.println("\n===== MESSAGEPACK DEBUG AT ERROR POSITION =====");
  DEBUG_SERIAL.printf("Error reported at position: %u\n", errorPosition);
  DEBUG_SERIAL.printf("Total MessagePack length: %u bytes\n", msgPackTx->currentMsgLen);
  
  // Print the byte at the error position
  DEBUG_SERIAL.printf("Byte at position %u: 0x%02X (decimal: %u, ASCII: %c)\n", 
                     errorPosition, 
                     msgPackTx->msgBuffer[errorPosition],
                     msgPackTx->msgBuffer[errorPosition],
                     (msgPackTx->msgBuffer[errorPosition] >= 32 && msgPackTx->msgBuffer[errorPosition] <= 126) ? 
                      (char)msgPackTx->msgBuffer[errorPosition] : '.');

  // Print surrounding bytes in hex
  DEBUG_SERIAL.println("\nSurrounding bytes (hex):");
  for (uint32_t i = startPos; i <= endPos; i++) {
    if (i == errorPosition) {
      DEBUG_SERIAL.printf("[0x%02X] ", msgPackTx->msgBuffer[i]); // Highlight the error position
    } else {
      DEBUG_SERIAL.printf("0x%02X ", msgPackTx->msgBuffer[i]);
    }
    
    // Add a newline every 8 bytes for readability
    if ((i - startPos + 1) % 8 == 0) {
      DEBUG_SERIAL.println();
    }
  }
  DEBUG_SERIAL.println();

  // Try to identify MessagePack format types around the error position
  DEBUG_SERIAL.println("\nMessagePack format analysis:");
  
  // Check for common MessagePack format markers
  for (uint32_t i = startPos; i <= endPos; i++) {
    uint8_t byte = msgPackTx->msgBuffer[i];
    String formatType = "";
    
    // Identify MessagePack format types based on the byte value
    if (byte < 0x80) {
      formatType = "positive fixint";
    } else if (byte >= 0x80 && byte <= 0x8f) {
      formatType = "fixmap (size " + String(byte & 0x0f) + ")";
    } else if (byte >= 0x90 && byte <= 0x9f) {
      formatType = "fixarray (size " + String(byte & 0x0f) + ")";
    } else if (byte >= 0xa0 && byte <= 0xbf) {
      formatType = "fixstr (length " + String(byte & 0x1f) + ")";
    } else if (byte == 0xc0) {
      formatType = "nil";
    } else if (byte == 0xc2) {
      formatType = "false";
    } else if (byte == 0xc3) {
      formatType = "true";
    } else if (byte == 0xc4) {
      formatType = "bin 8";
    } else if (byte == 0xc5) {
      formatType = "bin 16";
    } else if (byte == 0xc6) {
      formatType = "bin 32";
    } else if (byte == 0xca) {
      formatType = "float 32";
    } else if (byte == 0xcb) {
      formatType = "float 64";
    } else if (byte == 0xcc) {
      formatType = "uint 8";
    } else if (byte == 0xcd) {
      formatType = "uint 16";
    } else if (byte == 0xce) {
      formatType = "uint 32";
    } else if (byte == 0xcf) {
      formatType = "uint 64";
    } else if (byte == 0xd0) {
      formatType = "int 8";
    } else if (byte == 0xd1) {
      formatType = "int 16";
    } else if (byte == 0xd2) {
      formatType = "int 32";
    } else if (byte == 0xd3) {
      formatType = "int 64";
    } else if (byte == 0xd9) {
      formatType = "str 8";
    } else if (byte == 0xda) {
      formatType = "str 16";
    } else if (byte == 0xdb) {
      formatType = "str 32";
    } else if (byte == 0xdc) {
      formatType = "array 16";
    } else if (byte == 0xdd) {
      formatType = "array 32";
    } else if (byte == 0xde) {
      formatType = "map 16";
    } else if (byte == 0xdf) {
      formatType = "map 32";
    } else if (byte >= 0xe0) {
      formatType = "negative fixint";
    }
    
    if (formatType != "") {
      if (i == errorPosition) {
        DEBUG_SERIAL.printf("Position %u: [0x%02X] - %s\n", i, byte, formatType.c_str());
      } else {
        DEBUG_SERIAL.printf("Position %u: 0x%02X - %s\n", i, byte, formatType.c_str());
      }
    }
  }
  
  // Try to identify string fields near the error position
  DEBUG_SERIAL.println("\nAttempting to identify string fields:");
  for (uint32_t i = startPos; i <= endPos - 3; i++) {
    // Look for fixstr format (0xa0-0xbf) or str8 format (0xd9)
    if ((msgPackTx->msgBuffer[i] >= 0xa0 && msgPackTx->msgBuffer[i] <= 0xbf) || 
        msgPackTx->msgBuffer[i] == 0xd9) {
      
      uint8_t strLen = 0;
      uint32_t strStart = 0;
      
      if (msgPackTx->msgBuffer[i] >= 0xa0 && msgPackTx->msgBuffer[i] <= 0xbf) {
        // fixstr format
        strLen = msgPackTx->msgBuffer[i] & 0x1f;
        strStart = i + 1;
      } else if (msgPackTx->msgBuffer[i] == 0xd9) {
        // str8 format
        strLen = msgPackTx->msgBuffer[i+1];
        strStart = i + 2;
      }
      
      if (strLen > 0 && strStart + strLen <= endPos) {
        String fieldName = "";
        for (uint8_t j = 0; j < strLen; j++) {
          char c = msgPackTx->msgBuffer[strStart + j];
          if (c >= 32 && c <= 126) {  // Printable ASCII
            fieldName += c;
          } else {
            fieldName += '.';  // Replace non-printable with dot
          }
        }
        
        DEBUG_SERIAL.printf("Position %u: String field \"%s\" (length %u)\n", 
                           i, fieldName.c_str(), strLen);
        
        // Skip ahead past this string
        i = strStart + strLen - 1;
      }
    }
  }
  
  DEBUG_SERIAL.println("\n===== END MESSAGEPACK DEBUG =====");
  #endif
}

// Add this implementation after the submitAssetOptInToAlgorand method

// Submit application opt-in transaction to Algorand network
// Return: error code (0 = OK)
int AlgoIoT::submitApplicationOptInToAlgorand(uint64_t applicationId)
{
  uint32_t fv = 0;
  uint16_t fee = 0;
  int iErr = 0;
  uint8_t signature[ALGORAND_SIG_BYTES];
  uint8_t transactionMessagePackBuffer[ALGORAND_MAX_TX_MSGPACK_SIZE];
  char transactionID[ALGORAND_TRANSACTIONID_SIZE + 1] = "";
  msgPack msgPackTx = NULL;

  // Get current Algorand parameters
  int httpResCode = getAlgorandTxParams(&fv, &fee);
  if (httpResCode != 200)
  {
    return ALGOIOT_NETWORK_ERROR;
  }

  #ifdef LIB_DEBUGMODE
  DEBUG_SERIAL.printf("\nPreparing application opt-in transaction for application ID: %llu\n", applicationId);
  DEBUG_SERIAL.printf("First valid round: %u, Fee: %u\n", fv, fee);
  DEBUG_SERIAL.printf("Sender address (first 8 bytes): %02X %02X %02X %02X %02X %02X %02X %02X\n", 
                     m_senderAddressBytes[0], m_senderAddressBytes[1], m_senderAddressBytes[2], m_senderAddressBytes[3],
                     m_senderAddressBytes[4], m_senderAddressBytes[5], m_senderAddressBytes[6], m_senderAddressBytes[7]);
  #endif

  // Prepare transaction structure as MessagePack
  msgPackTx = msgpackInit(&(transactionMessagePackBuffer[0]), ALGORAND_MAX_TX_MSGPACK_SIZE);
  if (msgPackTx == NULL)  
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.println("\n Error initializing transaction MessagePack\n");
    #endif
    return ALGOIOT_MESSAGEPACK_ERROR;
  }  
  
  iErr = prepareApplicationOptInMessagePack(msgPackTx, fv, fee, applicationId);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n Error %d preparing application opt-in MessagePack\n", iErr);
    #endif
    return ALGOIOT_MESSAGEPACK_ERROR;
  }

  // Debug print the MessagePack content
  #ifdef LIB_DEBUGMODE
  DEBUG_SERIAL.println("\nUnsigned MessagePack content:");
  debugPrintMessagePack(msgPackTx);
  #endif

  // Application opt-in transaction correctly assembled. Now sign it
  iErr = signMessagePackAddingPrefix(msgPackTx, &(signature[0]));
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n Error %d signing MessagePack\n", iErr);
    #endif
    return ALGOIOT_SIGNATURE_ERROR;
  }

  // Debug print the signature
  #ifdef LIB_DEBUGMODE
  DEBUG_SERIAL.println("\nSignature (64 bytes):");
  for (int i = 0; i < ALGORAND_SIG_BYTES; i++) {
    DEBUG_SERIAL.printf("%02X ", signature[i]);
    if ((i + 1) % 16 == 0) DEBUG_SERIAL.println();
  }
  DEBUG_SERIAL.println();
  #endif

  // Signed OK: now compose payload
  iErr = createSignedBinaryTransaction(msgPackTx, signature);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n Error %d creating signed binary transaction\n", iErr);
    #endif
    return ALGOIOT_INTERNAL_GENERIC_ERROR;
  }

  // Debug print the final signed MessagePack
  #ifdef LIB_DEBUGMODE
  DEBUG_SERIAL.println("\nSigned MessagePack content:");
  debugPrintMessagePack(msgPackTx);
  
  // Payload ready. Now we can submit it via algod REST API
  DEBUG_SERIAL.println("\nReady to submit application opt-in transaction to Algorand network");
  #endif
  
  // Print transaction data in readable format
  #ifdef LIB_DEBUGMODE
  printTransactionData(msgPackTx);
  #endif
  
  iErr = submitTransaction(msgPackTx); // Returns HTTP code
  if (iErr != 200)  // 200 = HTTP OK
  { // Something went wrong
    return ALGOIOT_TRANSACTION_ERROR;
  }
  
  // OK: our transaction for application opt-in was successfully submitted to the Algorand blockchain
  #ifdef LIB_DEBUGMODE
  DEBUG_SERIAL.print("\t Application opt-in transaction successfully submitted with ID=");
  DEBUG_SERIAL.println(getTransactionID());
  #endif
  
  return ALGOIOT_NO_ERROR;
}

// Prepares an application opt-in transaction MessagePack
// Returns error code (0 = OK)
int AlgoIoT::prepareApplicationOptInMessagePack(msgPack msgPackTx,
                                  const uint32_t lastRound, 
                                  const uint16_t fee,
                                  const uint64_t applicationId)
{ 
  int iErr = 0;
  char gen[ALGORAND_NETWORK_ID_CHARS + 1] = "";
  uint32_t lv = lastRound + ALGORAND_MAX_WAIT_ROUNDS;
  uint8_t nFields = ALGORAND_APPLICATION_OPTIN_MIN_FIELDS;

  if (msgPackTx == NULL)
    return ALGOIOT_NULL_POINTER_ERROR;
  if (msgPackTx->msgBuffer == NULL)
    return ALGOIOT_INTERNAL_GENERIC_ERROR;
  if ((lastRound == 0) || (fee == 0))
  {
    return ALGOIOT_INTERNAL_GENERIC_ERROR;
  }
  
  #ifdef LIB_DEBUGMODE
  DEBUG_SERIAL.printf("\nPreparing application opt-in with application ID: %llu\n", applicationId);
  #endif
  
  if (m_networkType == ALGORAND_TESTNET)
  { // TestNet
    strncpy(gen, ALGORAND_TESTNET_ID, ALGORAND_NETWORK_ID_CHARS);
    // Decode Algorand network hash
    iErr = decodeAlgorandNetHash(ALGORAND_TESTNET_HASH, m_netHash);
    if (iErr)
    {
      #ifdef LIB_DEBUGMODE
      DEBUG_SERIAL.printf("\n prepareApplicationOptInMessagePack(): ERROR %d decoding Algorand network hash\n\n", iErr);
      #endif
      return ALGOIOT_INTERNAL_GENERIC_ERROR;
    }
  }
  else
  { // MainNet
    strncpy(gen, ALGORAND_MAINNET_ID, ALGORAND_NETWORK_ID_CHARS);
    iErr = decodeAlgorandNetHash(ALGORAND_MAINNET_HASH, m_netHash);
    if (iErr)
    {
      #ifdef LIB_DEBUGMODE
      DEBUG_SERIAL.printf("\n prepareApplicationOptInMessagePack(): ERROR %d decoding Algorand network hash\n\n", iErr);
      #endif
      return ALGOIOT_INTERNAL_GENERIC_ERROR;
    }
  }
  gen[ALGORAND_NETWORK_ID_CHARS] = '\0';

  // We leave a blank space header so we can add:
  // - "TX" prefix before signing
  // - m_signature field and "txn" node field after signing
  iErr = msgPackModifyCurrentPosition(msgPackTx, BLANK_MSGPACK_HEADER);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareApplicationOptInMessagePack(): ERROR %d from msgPackModifyCurrentPosition()\n\n", iErr);
    #endif
    return 5;
  }

  // Add root map
  iErr = msgpackAddShortMap(msgPackTx, nFields); 
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareApplicationOptInMessagePack(): ERROR %d adding root map with %d fields\n\n", iErr, nFields);
    #endif
    return 5;
  }

  // Fields must follow alphabetical order

  // "apan" label (OnComplete type - 1 for OptIn)
  iErr = msgpackAddShortString(msgPackTx, "apan");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareApplicationOptInMessagePack(): ERROR %d adding apan label\n\n", iErr);
    #endif
    return 5;
  }
  // apan value (1 for OptIn)
  iErr = msgpackAddUInt7(msgPackTx, 1);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareApplicationOptInMessagePack(): ERROR %d adding apan value\n\n", iErr);
    #endif
    return 5;
  }

  // "apid" label (Application ID)
  iErr = msgpackAddShortString(msgPackTx, "apid");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareApplicationOptInMessagePack(): ERROR %d adding apid label\n\n", iErr);
    #endif
    return 5;
  }
  
  // apid value - Use UInt32 for application IDs that fit in 32 bits to match Algo SDK encoding
  #ifdef LIB_DEBUGMODE
  DEBUG_SERIAL.printf("\nAdding application ID: %llu\n", applicationId);
  #endif
  
  // Check if the application ID fits in a uint32
  if (applicationId <= 0xFFFFFFFF) {
    iErr = msgpackAddUInt32(msgPackTx, (uint32_t)applicationId);
  } else {
    iErr = msgpackAddUInt64(msgPackTx, applicationId);
  }
  
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareApplicationOptInMessagePack(): ERROR %d adding apid value\n\n", iErr);
    #endif
    return 5;
  }

  // "fee" label
  iErr = msgpackAddShortString(msgPackTx, "fee");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareApplicationOptInMessagePack(): ERROR %d adding fee label\n\n", iErr);
    #endif
    return 5;
  }
  // fee value
  iErr = msgpackAddUInt16(msgPackTx, fee);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareApplicationOptInMessagePack(): ERROR %d adding fee value\n\n", iErr);
    #endif
    return 5;
  }

  // "fv" label
  iErr = msgpackAddShortString(msgPackTx, "fv");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareApplicationOptInMessagePack(): ERROR %d adding fv label\n\n", iErr);
    #endif
    return 5;
  }
  // fv value
  iErr = msgpackAddUInt32(msgPackTx, lastRound);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareApplicationOptInMessagePack(): ERROR %d adding fv value\n\n", iErr);
    #endif
    return 5;
  }

  // "gen" label
  iErr = msgpackAddShortString(msgPackTx, "gen");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareApplicationOptInMessagePack(): ERROR %d adding gen label\n\n", iErr);
    #endif
    return 5;
  }
  // gen string
  iErr = msgpackAddShortString(msgPackTx, gen);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareApplicationOptInMessagePack(): ERROR %d adding gen string\n\n", iErr);
    #endif
    return 5;
  }

  // "gh" label
  iErr = msgpackAddShortString(msgPackTx, "gh");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareApplicationOptInMessagePack(): ERROR %d adding gh label\n\n", iErr);
    #endif
    return 5;
  }
  // gh value (binary buffer)
  iErr = msgpackAddShortByteArray(msgPackTx, (const uint8_t*)&(m_netHash[0]), (const uint8_t)ALGORAND_NET_HASH_BYTES);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareApplicationOptInMessagePack(): ERROR %d adding gh value\n\n", iErr);
    #endif
    return 5;
  }

  // "lv" label
  iErr = msgpackAddShortString(msgPackTx, "lv");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareApplicationOptInMessagePack(): ERROR %d adding lv label\n\n", iErr);
    #endif
    return 5;
  }
  // lv value
  iErr = msgpackAddUInt32(msgPackTx, lv);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareApplicationOptInMessagePack(): ERROR %d adding lv value\n\n", iErr);
    #endif
    return 5;
  }

  // "snd" label
  iErr = msgpackAddShortString(msgPackTx, "snd");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareApplicationOptInMessagePack(): ERROR %d adding snd label\n\n", iErr);
    #endif
    return 5;
  }
  // snd value (binary buffer)
  iErr = msgpackAddShortByteArray(msgPackTx, (const uint8_t*)&(m_senderAddressBytes[0]), (const uint8_t)ALGORAND_ADDRESS_BYTES);  
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareApplicationOptInMessagePack(): ERROR %d adding snd value\n\n", iErr);
    #endif
    return 5;
  }

  // "type" label
  iErr = msgpackAddShortString(msgPackTx, "type");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareApplicationOptInMessagePack(): ERROR %d adding type label\n\n", iErr);
    #endif
    return 5;
  }
  // type string
  iErr = msgpackAddShortString(msgPackTx, "appl");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareApplicationOptInMessagePack(): ERROR %d adding type string\n\n", iErr);
    #endif
    return 5;
  }
  
  #ifdef LIB_DEBUGMODE
  DEBUG_SERIAL.println("\nApplication opt-in MessagePack preparation complete");
  #endif

  // End of messagepack
  return 0;
}
