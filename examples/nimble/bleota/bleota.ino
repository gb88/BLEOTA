/*
  Create a BLE server with BLE OTA service that implement https://components.espressif.com/components/espressif/ble_ota without security
  The service advertises itself as: 00008018-0000-1000-8000-00805f9b34fb
  If any of this is defined:
  MODEL
  SERIAL_NUM 
  FW_VERSION  
  HW_VERSION 
  MANUFACTURER 
  the DIS service is added

  The flow of creating the BLE server is:
  1. Create a BLE Server
  2. Add a BLE OTA Service
  3. Add a DIS Service
  4. Start the service.
  5. Start advertising.
  6. Process the update
*/

#include <BLEServer.h>
#include "NimBLEOTA.h"

#define MODEL "1"
#define SERIAL_NUM "1234"
#define FW_VERSION "1.0.0"
#define HW_VERSION "1"
#define MANUFACTURER "Espressif"

NimBLEOTAClass BLEOTA;

BLEServer* pServer = NULL;

bool deviceConnected = false;
bool oldDeviceConnected = false;

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer, NimBLEConnInfo& connInfo) {
    deviceConnected = true;
    pServer->updateConnParams(connInfo.getConnHandle(), 0x06, 0x12, 0, 2000);
  };

  void onDisconnect(BLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
    deviceConnected = false;
  }
};

void setup() {
  Serial.begin(115200);

  // Create the BLE Device
  BLEDevice::init("ESP32");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  // Add OTA Service
  BLEOTA.begin(pServer);
#ifdef MODEL
  BLEOTA.setModel(MODEL);
#endif
#ifdef SERIAL_NUM
  BLEOTA.setSerialNumber(SERIAL_NUM);
#endif
#ifdef FW_VERSION
  BLEOTA.setFWVersion(FW_VERSION);
#endif
#ifdef HW_VERSION
  BLEOTA.setHWVersion(HW_VERSION);
#endif
#ifdef MANUFACTURER
  BLEOTA.setManufactuer(MANUFACTURER);
#endif

  BLEOTA.init();
  // Start advertising
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(BLEOTA.getBLEOTAuuid());
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();

#ifdef FW_VERSION
  Serial.print("Firmware Version: ");
  Serial.println(FW_VERSION);
#endif

  Serial.println("Waiting a client connection...");
}

void loop() {
  //disconnecting
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);                   // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising();  // restart advertising
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
  BLEOTA.process();
  delay(1000);
}
