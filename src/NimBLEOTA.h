#ifndef NIMBLEOTA_H
#define NIMBLEOTA_H

#include "BLEOTABase.h"
#include <NimBLEDevice.h>

class NimBLEOTAClass: public BLEOTABase {
  public:
	NimBLEOTAClass();
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
	void sendCommandAnswer(uint16_t command_id, uint16_t status);
	void sendFWAnswer(uint16_t index, uint16_t status);
};
#endif