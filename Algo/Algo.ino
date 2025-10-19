/**
 *  AlgoIoT example for ESP32
 * 
 *  Example for "AlgoIoT", Algorand lightweight library for ESP32
 * 
 *  Last mod 20231221-1
 *
 *  By Fernando Carello for GT50
 *  Released under Apache license
 *  Copyright 2023 GT50 S.r.l.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.*/



#include <WiFi.h>
#include <WiFiMulti.h>
#include <AlgoIoT.h>


///////////////////////////
// USER-DEFINED SETTINGS
///////////////////////////
// Please edit accordingly
#define MYWIFI_SSID "nickthelegend-2.4G" 	
#define MYWIFI_PWD "Nicolas1234@" 

// Assign a name to your IoT app:
#define DAPP_NAME "AlgoIoT_MyTest1" // Keep it short; 31 chars = absolute max length
// Mnemonic words (25 BIP-39 words) representing device account. Beware, this is a PRIVATE KEY and should not be shared! 
// Please replace demo mnemonic words with your own:
#define NODE_ACCOUNT_MNEMONICS "shadow market lounge gauge battle small crash funny supreme regular obtain require control oil lend reward galaxy tuition elder owner flavor rural expose absent sniff"  
#define RECEIVER_ADDRESS "" 				// Leave "" to send to self (default, no fee to be paid) or insert a valid Algorand destination address
#define USE_TESTNET	                // Comment out to use Mainnet  *** BEWARE: Mainnet is the "real thing" and will cost you real Algos! ***

// Assign your node serial number (will be added to Note data):
#define NODE_SERIAL_NUMBER 1234567890UL

// Add this define after the other defines in the USER-DEFINED SETTINGS section
#define ASSET_ID_TO_OPT_IN 733709260UL  // Asset ID to opt into

// Add this define for Application NoOp transaction
#define APPLICATION_ID_FOR_NOOP 51UL  // Application ID for NoOp calls
#define ASSET_ID_FOR_OPTOUT 168103UL  // Asset ID for opt-out example
#define ASSET_ID_FOR_FREEZE 168103UL  // Asset ID for freeze example
#define ASSET_ID_FOR_DESTROY 168103UL  // Asset ID for destroy example
#define APPLICATION_ID_TO_OPT_IN 738608433UL  // Application ID to opt into

// Sample labels for your data:
#define SN_LABEL "NodeSerialNum"
#define T_LABEL "Temperature(°C)"
#define H_LABEL "RelHumidity(%)"
#define P_LABEL "Pressure(mbar)"
#define LAT_LABEL "Latitude"
#define LON_LABEL "Longitude"
#define ALT_LABEL "Elevation(m)"

// Comment out if a BME280 module is connected via I2C to the ESP32 board
#define FAKE_TPH_SENSOR

#ifdef FAKE_TPH_SENSOR
// Assign your fake data of choice here
const uint8_t FAKE_H_PCT = 55;
const float FAKE_T_C = 25.5f;
const uint16_t FAKE_P_MBAR = 1018;
#else
#define SDA_PIN 21	// Sensor module connection
#define SCL_PIN 22	// Edit accordingly to your board/wiring
#include <Bme280.h> // https://github.com/malokhvii-eduard/arduino-bme280
#endif

// You may optionally specify sensor position here, or comment out
#define POS_LAT_DEG 41.905344f
#define POS_LON_DEG 12.444122f
#define POS_ALT_M 21

#define DATA_SEND_INTERVAL_MINS 60
#define WIFI_RETRY_DELAY_MS 1000

// Uncomment to get debug prints on Serial Monitor
#define SERIAL_DEBUGMODE

//////////////////////////////////
// END OF USER-DEFINED SETTINGS
//////////////////////////////////


#define DEBUG_SERIAL Serial
#define DATA_SEND_INTERVAL (( DATA_SEND_INTERVAL_MINS ) * 60 * 1000UL) 


// Globals
AlgoIoT g_algoIoT(DAPP_NAME, NODE_ACCOUNT_MNEMONICS);
WiFiMulti g_wifiMulti;
#ifndef FAKE_TPH_SENSOR
Bme280TwoWire g_BMEsensor;
#endif
// End globals



//////////////////////////////////////////////
//
// Forward Declarations for local functions
//
//////////////////////////////////////////////

void waitForever();

void initializeBME280();

// Read sensors data (real of fake depending on #define in user_config.h)
// Returns error code (0 = OK)
int readSensors(float* temperature_C, uint8_t* relhum_Pct, uint16_t* pressure_mbar);

// Get coordinates. We do not actually use a GPS: position is (optionally) specified by user (see above among defines)
int readPosition(float* lat_deg, float* lon_deg, int16_t* alt_m);

// Add a function to debug the transaction content right before submission

// Add this function to your Algo.ino file to manually debug the transaction content
void debugTransactionContent(const char* title, uint8_t* buffer, uint32_t length) {
  #ifdef SERIAL_DEBUGMODE
  DEBUG_SERIAL.printf("\n===== %s =====\n", title);
  DEBUG_SERIAL.println("Hex dump:");
  
  for (uint32_t i = 0; i < length; i++) {
    DEBUG_SERIAL.printf("%02X ", buffer[i]);
    if ((i + 1) % 16 == 0) {
      DEBUG_SERIAL.println();
    }
  }
  
  DEBUG_SERIAL.println("\n===================\n");
  #endif
}

// Add this function to your Algo.ino file to manually debug the MessagePack
// You can call this function right before submitting the transaction

void debugMessagePackAtPosition(uint32_t position) {
  // This function can be called from setup() or loop() to debug a specific position
  // in the MessagePack buffer
  
  // Example usage:
  // In submitAssetOptIn() function, add this line before calling g_algoIoT.submitAssetOptInToAlgorand():
  // debugMessagePackAtPosition(242);
}

// Modify the submitAssetOptIn function to include additional debugging
int submitAssetOptIn()
{
  if((g_wifiMulti.run() == WL_CONNECTED)) 
  {
    int iErr = g_algoIoT.submitAssetOptInToAlgorand(ASSET_ID_TO_OPT_IN);
    if (!iErr) {
      DEBUG_SERIAL.printf("TX ID: %s\n", g_algoIoT.getTransactionID());
    }
    return iErr;
  }
  return 1;
}

// Add this function after the submitAssetOptIn function
int submitApplicationOptIn()
{
  if((g_wifiMulti.run() == WL_CONNECTED)) 
  {
    int iErr = g_algoIoT.submitApplicationOptInToAlgorand(APPLICATION_ID_TO_OPT_IN);
    if (!iErr) {
      DEBUG_SERIAL.printf("TX ID: %s\n", g_algoIoT.getTransactionID());
    }
    return iErr;
  }
  return 1;
}

// Add this function after the submitApplicationOptIn function
int submitAssetCreation()
{
  if((g_wifiMulti.run() == WL_CONNECTED)) 
  {
    static uint32_t assetCounter = 0;
    assetCounter++;
    char assetName[32], unitName[8];
    snprintf(assetName, sizeof(assetName), "TestAsset_%u", assetCounter);
    snprintf(unitName, sizeof(unitName), "TST%u", assetCounter % 1000);

    int iErr = g_algoIoT.submitAssetCreationToAlgorand(assetName, unitName, "https://example.com", 0, 1000000);
    if (!iErr) {
      DEBUG_SERIAL.printf("TX ID: %s\n", g_algoIoT.getTransactionID());
    }
    return iErr;
  }
  return 1;
}

// Add this helper function to your Algo.ino file to verify the mnemonic conversion
void testMnemonicConversion() {
  #ifdef SERIAL_DEBUGMODE
  DEBUG_SERIAL.println("\n===== Testing Mnemonic Conversion =====");
  
  // Create a temporary instance to test the mnemonic conversion
  AlgoIoT testInstance(DAPP_NAME, NODE_ACCOUNT_MNEMONICS);
  
  // Print the sender address (public key) derived from the mnemonic
  DEBUG_SERIAL.println("Derived public key (first 16 bytes):");
  const uint8_t* senderAddress = testInstance.getSenderAddressBytes();
  for (int i = 0; i < 16; i++) {
    DEBUG_SERIAL.printf("%02X ", senderAddress[i]);
  }
  DEBUG_SERIAL.println("\n=====================================\n");
  #endif
}

//////////
// SETUP
//////////

// Add this call to setup() right after the constructor call
void setup() 
{  
  int iErr = 0;

  #ifdef SERIAL_DEBUGMODE
  DEBUG_SERIAL.begin(115200);
  while (!DEBUG_SERIAL)
  {    
  }
  delay(1000);
  DEBUG_SERIAL.println();
  #endif
  
  delay(500);

  // Configure WiFi
  g_wifiMulti.addAP(MYWIFI_SSID, MYWIFI_PWD);


  #ifndef FAKE_TPH_SENSOR
  // Initialize T/H/P sensor
  initializeBME280();
  #endif

  // Test mnemonic conversion - add this line
  testMnemonicConversion();

  // Change data receiver address and Algorand network type if needed
  if (RECEIVER_ADDRESS != "")
  {
    iErr = g_algoIoT.setDestinationAddress(RECEIVER_ADDRESS);
    if (iErr != ALGOIOT_NO_ERROR)
    {
      #ifdef SERIAL_DEBUGMODE
      DEBUG_SERIAL.printf("\n Error %d setting receiver address %s: please check it\n\n", iErr, RECEIVER_ADDRESS);
      #endif

      waitForever();
    }
  }

  #ifndef USE_TESTNET
  iErr = g_algoIoT.setAlgorandNetwork(ALGORAND_MAINNET);
  if (iErr != ALGOIOT_NO_ERROR)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n Error %d setting Algorand network type to Mainnet: please report!\n\n", iErr);
    #endif

    waitForever();
  }
  #endif

  
  // Opt-in to the asset
  #ifdef SERIAL_DEBUGMODE
  DEBUG_SERIAL.println("Attempting to opt-in to asset...");
  #endif
  
  iErr = submitAssetOptIn();
  if (iErr != ALGOIOT_NO_ERROR)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n Error %d opting into asset: please check network connection and asset ID\n\n", iErr);
    #endif
    // We don't call waitForever() here because we still want to proceed with sensor data transactions
    // even if the asset opt-in fails
  }
  else
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.println("Successfully opted into asset!");
    #endif
  }

  // Add this code to the setup() function, right after the asset opt-in code
  // Opt-in to the application
  #ifdef SERIAL_DEBUGMODE
  DEBUG_SERIAL.println("Attempting to opt-in to application...");
  #endif
  
  iErr = submitApplicationOptIn();
  if (iErr != ALGOIOT_NO_ERROR)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n Error %d opting into application: please check network connection and application ID\n\n", iErr);
    #endif
    // We don't call waitForever() here because we still want to proceed with sensor data transactions
    // even if the application opt-in fails
  }
  else
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.println("Successfully opted into application!");
    #endif
  }

  // Add this code to the setup() function, right after the application opt-in code
  // Create an asset
  #ifdef SERIAL_DEBUGMODE
  DEBUG_SERIAL.println("Attempting to create an asset...");
  #endif

  iErr = submitAssetCreation();
  if (iErr != ALGOIOT_NO_ERROR)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n Error %d creating asset: please check network connection\n\n", iErr);
    #endif
    // We don't call waitForever() here because we still want to proceed with sensor data transactions
    // even if the asset creation fails
  }
  else
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.println("Successfully created asset!");
    #endif
  }
}


/////////
// LOOP
/////////

void loop() 
{
  uint16_t notesLen = 0;
  uint32_t currentMillis = millis();
  uint8_t rhPct = 0;
  float tempC = 0.0f;
  uint16_t pmbar = 0;

  if((g_wifiMulti.run() == WL_CONNECTED)) 
  {
    int iErr = 0;

    iErr = readSensors(&tempC, &rhPct, &pmbar);
    if (!iErr)
    { // sensors OK
      float lat = 0.0f;
      float lon = 0.0f;
      int16_t alt = 0;

      uint8_t positionNotSpecified = readPosition(&lat, &lon, &alt);

      if (!positionNotSpecified)
      {
        g_algoIoT.dataAddFloatField(LAT_LABEL, lat);
        g_algoIoT.dataAddFloatField(LON_LABEL, lon);
        g_algoIoT.dataAddInt16Field(ALT_LABEL, alt);
      }
      
      // Execute all transaction types sequentially
      DEBUG_SERIAL.println("\n========== ALGORAND TRANSACTION EXECUTION ==========\n");

      // 1. Payment Transaction
      DEBUG_SERIAL.println("[1/8] Payment Transaction");
      iErr = g_algoIoT.dataAddUInt32Field(SN_LABEL, NODE_SERIAL_NUMBER);
      iErr |= g_algoIoT.dataAddFloatField(T_LABEL, tempC);
      iErr |= g_algoIoT.dataAddUInt8Field(H_LABEL, rhPct);
      iErr |= g_algoIoT.dataAddInt16Field(P_LABEL, pmbar);
      
      if (!iErr) {
        iErr = g_algoIoT.submitTransactionToAlgorand();
        DEBUG_SERIAL.printf("Result: %s\n", iErr ? "FAILED" : "SUCCESS");
        if (!iErr) DEBUG_SERIAL.printf("TX ID: %s\n", g_algoIoT.getTransactionID());
      }
      delay(15000);

      // 2. Asset Creation
      DEBUG_SERIAL.println("\n[2/8] Asset Creation Transaction");
      iErr = submitAssetCreation();
      DEBUG_SERIAL.printf("Result: %s\n", iErr ? "FAILED" : "SUCCESS");
      delay(15000);

      // 3. Asset Opt-in
      DEBUG_SERIAL.println("\n[3/8] Asset Opt-in Transaction");
      iErr = submitAssetOptIn();
      DEBUG_SERIAL.printf("Result: %s\n", iErr ? "FAILED" : "SUCCESS");
      delay(15000);

      // 4. Asset Opt-out
      DEBUG_SERIAL.println("\n[4/8] Asset Opt-out Transaction");
      iErr = submitAssetOptOut();
      DEBUG_SERIAL.printf("Result: %s\n", iErr ? "FAILED" : "SUCCESS");
      delay(15000);

      // 5. Asset Freeze
      DEBUG_SERIAL.println("\n[5/8] Asset Freeze Transaction");
      iErr = submitAssetFreeze();
      DEBUG_SERIAL.printf("Result: %s\n", iErr ? "FAILED" : "SUCCESS");
      delay(15000);

      // 6. Asset Destroy
      DEBUG_SERIAL.println("\n[6/8] Asset Destroy Transaction");
      iErr = submitAssetDestroy();
      DEBUG_SERIAL.printf("Result: %s\n", iErr ? "FAILED" : "SUCCESS");
      delay(15000);

      // 7. Application NoOp
      DEBUG_SERIAL.println("\n[7/8] Application NoOp Transaction");
      iErr = submitApplicationNoOp();
      DEBUG_SERIAL.printf("Result: %s\n", iErr ? "FAILED" : "SUCCESS");
      delay(15000);

      // 8. Application Opt-in
      DEBUG_SERIAL.println("\n[8/8] Application Opt-in Transaction");
      iErr = submitApplicationOptIn();
      DEBUG_SERIAL.printf("Result: %s\n", iErr ? "FAILED" : "SUCCESS");
      
      DEBUG_SERIAL.println("\n========== ALL TRANSACTIONS COMPLETED ==========\n");
    }
    // Wait for next data upload
    delay(DATA_SEND_INTERVAL);
  }
  // WiFi connection not established, wait a bit and retry
  delay(WIFI_RETRY_DELAY_MS);
}


////////////////////
//
// Implementations
//
////////////////////

void waitForever()
{
  while(1)
    delay(ULONG_MAX);
}


#ifndef FAKE_TPH_SENSOR
void initializeBME280()
{
  Bme280Settings customBmeSettings = Bme280Settings::weatherMonitoring();
  customBmeSettings.temperatureOversampling = Bme280Oversampling::X4;
  customBmeSettings.pressureOversampling = Bme280Oversampling::X2;
  customBmeSettings.humidityOversampling = Bme280Oversampling::X4;
  customBmeSettings.filter = Bme280Filter::X4;
  Wire.begin(SDA_PIN, SCL_PIN);  
  g_BMEsensor.begin(Bme280TwoWireAddress::Primary);
  g_BMEsensor.setSettings(customBmeSettings);
}
#endif    


int readPosition(float* lat_deg, float* lon_deg, int16_t* alt_m)
{
  #if defined(POS_LAT_DEG) && defined(POS_LON_DEG) && defined(POS_ALT_M)
  *lat_deg = POS_LAT_DEG;
  *lon_deg = POS_LON_DEG;
  *alt_m = POS_ALT_M;

  return 0; // OK
  #endif

  // Position not specified (it is optional after all)
  return 1;
}


// User may adapt this demo function to collect data
int readSensors(float* temperature_C, uint8_t* relhum_Pct, uint16_t* pressure_mbar)
{
  uint8_t bmeChipID = 0;

  #ifdef FAKE_TPH_SENSOR
  *temperature_C = FAKE_T_C;  // Dummy values
  *relhum_Pct = FAKE_H_PCT;
  *pressure_mbar = FAKE_P_MBAR;
  #else
  bmeChipID = g_BMEsensor.getChipId();
  if (bmeChipID == 0)
  { // BME280 not connected or not working
    return 1;
  }
  // Forced mode has to be specified every time we read data. We play safe and re-apply all settings
  Bme280Settings customBmeSettings = Bme280Settings::weatherMonitoring();
  customBmeSettings.temperatureOversampling = Bme280Oversampling::X4;
  customBmeSettings.pressureOversampling = Bme280Oversampling::X2;
  customBmeSettings.humidityOversampling = Bme280Oversampling::X4;
  customBmeSettings.filter = Bme280Filter::X4;
  g_BMEsensor.setSettings(customBmeSettings);
  delay(100);
  *temperature_C = g_BMEsensor.getTemperature();  
  *pressure_mbar = (uint16_t) (0.01f * g_BMEsensor.getPressure());
  *relhum_Pct = (uint8_t)(g_BMEsensor.getHumidity() + 0.5f);  // Round to largest int (H is always > 0)
  if (*pressure_mbar == 0)  // Sensor not working
    return 1;
  #endif

  return 0;
}

// Example function to demonstrate Asset Opt-Out transaction
int submitAssetOptOut()
{
  if((g_wifiMulti.run() == WL_CONNECTED))
  {
    int iErr = g_algoIoT.submitAssetOptOutToAlgorand(ASSET_ID_FOR_OPTOUT);
    if (!iErr) {
      DEBUG_SERIAL.printf("TX ID: %s\n", g_algoIoT.getTransactionID());
    }
    return iErr;
  }
  return 1;
}

// Example function to demonstrate Asset Freeze transaction
int submitAssetFreeze()
{
  if((g_wifiMulti.run() == WL_CONNECTED))
  {
    const char* freezeAddress = "4RLXQGPZVVRSXQF4VKZ74I6BCUD7TUVROOUBCVRKY37LQSHXORZV4KCAP4";
    int iErr = g_algoIoT.submitAssetFreezeToAlgorand(ASSET_ID_FOR_FREEZE, freezeAddress, true);
    if (!iErr) {
      DEBUG_SERIAL.printf("TX ID: %s\n", g_algoIoT.getTransactionID());
    }
    return iErr;
  }
  return 1;
}

// Example function to demonstrate Asset Destroy transaction
int submitAssetDestroy()
{
  if((g_wifiMulti.run() == WL_CONNECTED))
  {
    int iErr = g_algoIoT.submitAssetDestroyToAlgorand(ASSET_ID_FOR_DESTROY);
    if (!iErr) {
      DEBUG_SERIAL.printf("TX ID: %s\n", g_algoIoT.getTransactionID());
    }
    return iErr;
  }
  return 1;
}

// Example function to demonstrate Application NoOp transaction
int submitApplicationNoOp()
{
  if((g_wifiMulti.run() == WL_CONNECTED)) 
  {
    int iErr = g_algoIoT.submitApplicationNoOpToAlgorand(APPLICATION_ID_FOR_NOOP);
    if (!iErr) {
      DEBUG_SERIAL.printf("TX ID: %s\n", g_algoIoT.getTransactionID());
    }
    return iErr;
  }
  return 1;
}
