#include "BLEOTA.h"

// This file is only compiled when BLEOTA.h selected the BLEDevice.h
// backend (either modern core >= 3.3.0, or legacy core with BlueDroid).
// When the legacy-NimBLE-Arduino path is taken, BLEOTA.h defines
// BLEOTA_USE_NIMBLE and the implementation comes from NimBLEOTA.cpp
// instead — so we must skip the entire file here to avoid duplicate
// symbols.
#if !defined(BLEOTA_USE_NIMBLE)

class recvFWCallback : public BLECharacteristicCallbacks {
public:
  BLEOTAClass* _ota = nullptr;
  recvFWCallback(BLEOTAClass* ota) : _ota(ota) {}

private:
  void onWrite(BLECharacteristic* pCharacteristic) {
    if (pCharacteristic->getLength() >= 4) {
      uint8_t* data = pCharacteristic->getData();
      _ota->FWHandler(data, pCharacteristic->getLength());
    }
  }
};

class commandCallback : public BLECharacteristicCallbacks {
public:
  BLEOTAClass* _ota = nullptr;
  commandCallback(BLEOTAClass* ota) : _ota(ota) {}

private:
  void onWrite(BLECharacteristic* pCharacteristic) {
    if (pCharacteristic->getLength() >= 20) {
      uint8_t* data = pCharacteristic->getData();
      _ota->CommandHandler(data, pCharacteristic->getLength());
    }
  }
};

BLEOTAClass::BLEOTAClass() {}

void BLEOTAClass::begin(BLEServer* pServer, bool secure) {
  _pServer           = pServer;
  _pBLEOTAService    = nullptr;
  _pRecvFWchar       = nullptr;
  _pCommandchar      = nullptr;

  _pDISService       = nullptr;
  _pModelchar        = nullptr;
  _pSerialNumchar    = nullptr;
  _pFWVerchar        = nullptr;
  _pHWVerchar        = nullptr;
  _pManufacturerchar = nullptr;

  _done   = false;
  _mode_z = false;
  _secure = secure;

  _model        = "";
  _serial_num   = "";
  _fw_version   = "";
  _hw_version   = "";
  _manufacturer = "";

  _pCallbacks = nullptr;
  _front      = 0;
  _rear       = 0;
  _count      = 0;
}

void BLEOTAClass::init(void) {
  // ── OTA service ────────────────────────────────────────────────────
  _pBLEOTAService = _pServer->createService(BLE_OTA_SERVICE_UUID);

  // RECV_FW characteristic: receives firmware bytes.
  // WRITE_NR (write-without-response) is included so the companion
  // Web Bluetooth app can use writeValueWithoutResponse() for maximum
  // throughput — the per-sector CRC already gives us flow control.
  _pRecvFWchar = _pBLEOTAService->createCharacteristic(
    RECV_FW_UUID,
    BLECharacteristic::PROPERTY_WRITE
      | BLECharacteristic::PROPERTY_WRITE_NR
      | BLECharacteristic::PROPERTY_NOTIFY
      | BLECharacteristic::PROPERTY_INDICATE);
  _pRecvFWchar->setCallbacks(new recvFWCallback(this));

  // COMMAND characteristic: control channel.
  _pCommandchar = _pBLEOTAService->createCharacteristic(
    COMMAND_UUID,
    BLECharacteristic::PROPERTY_WRITE
      | BLECharacteristic::PROPERTY_WRITE_NR
      | BLECharacteristic::PROPERTY_NOTIFY
      | BLECharacteristic::PROPERTY_INDICATE);
  _pCommandchar->setCallbacks(new commandCallback(this));

  // CCCD (0x2902). On BlueDroid this adds the descriptor explicitly; on
  // NimBLE (via the core wrapper on S3/C3/C6/H2) it is a no-op — the
  // descriptor is created automatically. BLE2902 is marked deprecated
  // in core 3.3.0+ but still functional.
  _pRecvFWchar->addDescriptor(new BLE2902());
  _pCommandchar->addDescriptor(new BLE2902());

  // ── DIS service (only if at least one field is populated) ──────────
  const bool hasDIS =
    (_model != "") || (_serial_num != "") || (_fw_version != "") ||
    (_hw_version != "") || (_manufacturer != "");

  if (hasDIS)
    _pDISService = _pServer->createService(DIS_SERVICE_UUID);

  if (_model != "") {
    _pModelchar = _pDISService->createCharacteristic(
      DIS_MODEL_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    _pModelchar->setValue(_model.c_str());
  }
  if (_serial_num != "") {
    _pSerialNumchar = _pDISService->createCharacteristic(
      DIS_SERIAL_N_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    _pSerialNumchar->setValue(_serial_num.c_str());
  }
  if (_fw_version != "") {
    _pFWVerchar = _pDISService->createCharacteristic(
      DIS_FW_VER_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    _pFWVerchar->setValue(_fw_version.c_str());
  }
  if (_hw_version != "") {
    _pHWVerchar = _pDISService->createCharacteristic(
      DIS_HW_VERSION_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    _pHWVerchar->setValue(_hw_version.c_str());
  }
  if (_manufacturer != "") {
    _pManufacturerchar = _pDISService->createCharacteristic(
      DIS_MNF_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    _pManufacturerchar->setValue(_manufacturer.c_str());
  }

  _pBLEOTAService->start();
  if (hasDIS)
    _pDISService->start();
}

void BLEOTAClass::sendCommandAnswer(uint16_t command_id, uint16_t status) {
  memset(_ble_answer, 0, sizeof(_ble_answer));
  _ble_answer[0] = 0x03;
  _ble_answer[1] = 0x00;
  _ble_answer[2] = (command_id & 0xFF);
  _ble_answer[3] = (command_id >> 8) & 0xFF;
  _ble_answer[4] = (status & 0xFF);
  _ble_answer[5] = (status >> 8) & 0xFF;
  uint16_t crc = crc16(0, _ble_answer, 18);
  _ble_answer[18] = crc & 0xFF;
  _ble_answer[19] = (crc >> 8) & 0xFF;
  _pCommandchar->setValue(_ble_answer, sizeof(_ble_answer));
  _pCommandchar->indicate();
  _pCommandchar->notify();
}

void BLEOTAClass::sendFWAnswer(uint16_t index, uint16_t status) {
  memset(_ble_answer, 0, sizeof(_ble_answer));
  _ble_answer[0] = (index & 0xFF);
  _ble_answer[1] = (index >> 8) & 0xFF;
  _ble_answer[2] = (status & 0xFF);
  _ble_answer[3] = (status >> 8) & 0xFF;
  uint16_t crc = crc16(0, _ble_answer, 18);
  _ble_answer[18] = crc & 0xFF;
  _ble_answer[19] = (crc >> 8) & 0xFF;
  _pRecvFWchar->setValue(_ble_answer, sizeof(_ble_answer));
  _pRecvFWchar->indicate();
  _pRecvFWchar->notify();
}

#endif // !defined(BLEOTA_USE_NIMBLE)
