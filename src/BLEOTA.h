#ifndef BLEOTA_H
#define BLEOTA_H

// ════════════════════════════════════════════════════════════════════
//  BLEOTA 2.0 — unified header
// ════════════════════════════════════════════════════════════════════
//
// This header dispatches to the right BLE backend depending on the
// version of the arduino-esp32 core installed:
//
//   • core >= 3.3.0  →  use the core's BLEDevice.h.  On classic ESP32
//                       that wraps BlueDroid, on S3/C3/C6/H2 it wraps
//                       the core's built-in NimBLE.  Same API both
//                       ways, so a single BLEOTAClass covers every
//                       ESP32 variant.  The NimBLE-Arduino library is
//                       NEVER referenced in this branch, so it can
//                       stay installed for other projects without
//                       causing 'multiple definition of npl_freertos_*'
//                       link errors on S3/C3/C6/H2.
//
//   • core <  3.3.0  →  legacy v1.x behaviour, kept for backwards
//                       compatibility.  If NimBLE-Arduino is installed
//                       in the user's libraries folder the NimBLE
//                       backend is used, otherwise BlueDroid.
//
// The public API the sketch sees is identical in both cases:
//
//     BLEOTAClass BLEOTA;
//     BLEOTA.begin(pServer);
//     BLEOTA.init();
//     BLEOTA.process();
//
// Users migrating from v1.x who were using `NimBLEOTAClass` directly
// keep working: a type alias is provided below.
// ════════════════════════════════════════════════════════════════════

#include "BLEOTABase.h"
#include <esp_arduino_version.h>


// ─── Branch selector ───────────────────────────────────────────────
// BLEOTA_USE_NIMBLE is defined when the active backend is NimBLE-Arduino
// (legacy path only). It is NOT defined when the core's BLEDevice.h is
// used, even if the underlying stack happens to be NimBLE.
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 3, 0)
  // Modern core: always use BLEDevice.h. This is the preferred path.
#else
  // Legacy core (2.x / 3.0 / 3.1 / 3.2): preserve v1.x behaviour.
  #if __has_include(<NimBLEDevice.h>)
    #define BLEOTA_USE_NIMBLE
  #endif
#endif


// ─── NimBLE-Arduino path (legacy core, library installed) ──────────
// Handled first so that when BLEOTA_USE_NIMBLE is set the BLEDevice.h
// includes below are completely skipped.
#if defined(BLEOTA_USE_NIMBLE)

#include "NimBLEOTA.h"
// Give users the familiar 'BLEOTAClass' name as an alias to NimBLEOTAClass.
using BLEOTAClass = NimBLEOTAClass;


// ─── BLEDevice.h path (core >= 3.3.0, or legacy with BlueDroid) ────
#else

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

class BLEOTAClass : public BLEOTABase {
public:
  BLEOTAClass();
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

// Back-compat alias: sketches written for v1.x NimBLE backend keep
// compiling unchanged on core 3.3.0+, where BLEOTAClass maps onto
// the core's BLEDevice.h (which itself wraps NimBLE on S3/C3/C6/H2).
using NimBLEOTAClass = BLEOTAClass;

#endif // branch selector

#endif // BLEOTA_H
