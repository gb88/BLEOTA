#ifndef NIMBLEOTA_H
#define NIMBLEOTA_H

// Guard: this backend exists only for backwards compatibility with
// arduino-esp32 cores older than 3.3.0 that have NimBLE-Arduino
// installed. On core 3.3.0+ we always use BLEDevice.h from the core,
// so this file must be a no-op to avoid pulling NimBLE-Arduino into
// the build and triggering 'multiple definition of npl_freertos_*'
// link errors on ESP32-S3/C3/C6/H2.
//
// The enabling macro BLEOTA_USE_NIMBLE is set inside BLEOTA.h, which
// BLEOTABase.h is included from, so we include that first to inherit
// the dispatch logic even when NimBLEOTA.h is the entry point.
#include "BLEOTABase.h"
#include <esp_arduino_version.h>

// Re-evaluate the branch selector in case NimBLEOTA.h is the first
// header seen on this translation unit (e.g. users who still include
// it directly in v1.x sketches).
#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 3, 0) \
    && __has_include(<NimBLEDevice.h>)
  #ifndef BLEOTA_USE_NIMBLE
    #define BLEOTA_USE_NIMBLE
  #endif
#endif

#if defined(BLEOTA_USE_NIMBLE)

#include <NimBLEDevice.h>

class NimBLEOTAClass : public BLEOTABase {
public:
  NimBLEOTAClass();
  void begin(BLEServer* pServer, bool secure = false);
  void init();

private:
  BLEServer*         _pServer;
  BLEService*        _pBLEOTAService;
  BLECharacteristic* _pRecvFWchar;
  BLECharacteristic* _pCommandchar;
  BLEService*        _pDISService;
  BLECharacteristic* _pModelchar;
  BLECharacteristic* _pSerialNumchar;
  BLECharacteristic* _pFWVerchar;
  BLECharacteristic* _pHWVerchar;
  BLECharacteristic* _pManufacturerchar;

  void sendCommandAnswer(uint16_t command_id, uint16_t status);
  void sendFWAnswer(uint16_t index, uint16_t status);
};

#endif // defined(BLEOTA_USE_NIMBLE)

#endif // NIMBLEOTA_H
