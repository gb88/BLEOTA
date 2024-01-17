/*
  Create a BLE server with BLE OTA service that implement https://components.espressif.com/components/espressif/ble_ota with security
  
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
  2. Add a BLE OTA Service with security enabled
  2. Add the public key (rsa_key.pub content)
  3. Add a DIS Service
  4. Start the service.
  5. Start advertising.
  6. Process the update
  
  The .bin file must be signed and the public key must be included in the sketch.
  The signing process is the following:
  generate the private key (keep this secret):
    - openssl genrsa -out priv_key.pem 2048

  and the corresponding public key:
    - openssl rsa -in priv_key.pem -pubout > rsa_key.pub

  export the compiled sketch of SPIFFS to get the bin file
    
  sign the file with SHA256 hash with the private key:
    - openssl dgst -sign priv_key.pem -keyform PEM -sha256 -out signature.sign -binary ota.ino.bin

  throw it all in one file
    - cat ota.ino.bin signature.sign > ota.bin
	
  use this file to perform the update

*/

#include <BLEServer.h>
#include "BLEOTA.h"

#define MODEL "1"
#define SERIAL_NUM "1234"
#define FW_VERSION "1.0.0"
#define HW_VERSION "1"
#define MANUFACTURER "Espressif"

const char pub_key[] = "-----BEGIN PUBLIC KEY-----\nMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAw/rrOWrykXdTPFwZzljd\nPuuhkRDQUJQu0et5dWNd4ntbh+Qp9qDiZMEj9PcUkw6VUCWFTcSFkOR4i3M+H3g3\nJsKGe5y45DGK8HvgOAnGGUtb0/V2UVZAqiUzJ2cXSK+1688/kWRBSv6OTMXFg2Fa\nGnaIEupUIJZfnBjJmZOhqJll+kxvkxE3CjbnnP8SZ31ybItPV3DyML/7RZ3gMBB5\ngVh44kzAIzPD+NtSSU/RNbWOi3rgNPx1SLzUPjThkHAkVRJ96pWEctiblv2XwoIm\n1ZJEeeda3O46+zCpsI1Ph5oo8mi4QWj1MvkQldo3XtLWRtH/IbMLEgRSR5y054Tg\n0QIDAQAB\n-----END PUBLIC KEY-----";


BLEServer* pServer = NULL;

bool deviceConnected = false;
bool oldDeviceConnected = false;

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t* param) {
    deviceConnected = true;
    pServer->updateConnParams(param->connect.remote_bda, 0, 0x20, 0x10, 2000);
  };

  void onDisconnect(BLEServer* pServer) {
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
  BLEOTA.begin(pServer, true);
  // Add pub key
  BLEOTA.setKey(pub_key, strlen(pub_key));
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
