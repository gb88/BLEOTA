#ifndef BLEOTA_H
#define BLEOTA_H
#include "BLEOTABase.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

class BLEOTAClass: public BLEOTABase {
public:
  BLEOTAClass();
  void begin(BLEServer* pServer, bool secure = false);
  void init();
private:
  BLEServer* _pServer;
  BLEService* _pBLEOTAService;
  BLECharacteristic* _pRecvFWchar;
  BLECharacteristic* _pCommandchar;
  BLEService* _pDISService;
  BLECharacteristic* _pModelchar;
  BLECharacteristic* _pSerialNumchar;
  BLECharacteristic* _pFWVerchar;
  BLECharacteristic* _pHWVerchar;
  BLECharacteristic* _pManufacturerchar;
  /* BLE answer */
  void sendCommandAnswer(uint16_t command_id, uint16_t status);
  void sendFWAnswer(uint16_t index, uint16_t status);
};

#endif  //BLEOTA_H