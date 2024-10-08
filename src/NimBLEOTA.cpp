#include "NimBLEOTA.h"
 
class NimBLErecvFWCallback : public NimBLECharacteristicCallbacks {
  public:
  NimBLEOTAClass * _ota = NULL;
  NimBLErecvFWCallback(NimBLEOTAClass * ota)
  {
	  _ota = ota;
  }
  private:
  void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    if (pCharacteristic->getValue().length() >= 4) {
      NimBLEAttValue data = pCharacteristic->getValue();
      _ota->FWHandler((uint8_t*)data.c_str(), pCharacteristic->getValue().length());
    }
  }
};

class NimBLEcommandCallback : public NimBLECharacteristicCallbacks {
  public:
  NimBLEOTAClass * _ota = NULL;
  NimBLEcommandCallback(NimBLEOTAClass * ota)
  {
	  _ota = ota;
  }
  private:
  void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    if (pCharacteristic->getValue().length() >= 20) {
      NimBLEAttValue data = pCharacteristic->getValue();
      _ota->CommandHandler((uint8_t*)data.c_str(), pCharacteristic->getValue().length());
    }
  }
};

NimBLEOTAClass::NimBLEOTAClass() {
}


void NimBLEOTAClass::begin(BLEServer* pServer, bool secure) {
  _pServer = pServer;
  _pBLEOTAService = NULL;
  _pRecvFWchar = NULL;
  _pCommandchar = NULL;

  _pDISService = NULL;
  _pModelchar = NULL;
  _pSerialNumchar = NULL;
  _pFWVerchar = NULL;
  _pHWVerchar = NULL;
  _pManufacturerchar = NULL;

  _done = false;
  _mode_z = false;
  _secure = secure;

  _model = "";
  _serial_num = "";
  _fw_version = "";
  _hw_version = "";
  _manufacturer = "";
  
  _pCallbacks = nullptr;
  _front = 0;
  _rear = 0;
  _count = 0;
  
}

void NimBLEOTAClass::init(void) {
  // Create the OTA Service
  _pBLEOTAService = _pServer->createService(BLE_OTA_SERVICE_UUID);
  
  // Create a BLE Characteristic
  _pRecvFWchar = _pBLEOTAService->createCharacteristic(
    RECV_FW_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::INDICATE);
  _pRecvFWchar->setCallbacks(new NimBLErecvFWCallback(this));

  _pCommandchar = _pBLEOTAService->createCharacteristic(
    COMMAND_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::INDICATE);

  _pCommandchar->setCallbacks(new NimBLEcommandCallback(this));

  // Create a BLE Descriptor
  // _pRecvFWchar->addDescriptor(new BLE2902());
  // _pCommandchar->addDescriptor(new BLE2902());

  // Create the DIS Service
  if ((_model != "") || (_serial_num != "") || (_fw_version != "") || (_hw_version != "") || (_manufacturer != ""))
    _pDISService = _pServer->createService(DIS_SERVICE_UUID);

  if (_model != "") {
    _pModelchar = _pDISService->createCharacteristic(DIS_MODEL_CHAR_UUID, NIMBLE_PROPERTY::READ);
    _pModelchar->setValue(_model.c_str());
  }

  if (_serial_num != "") {
    _pSerialNumchar = _pDISService->createCharacteristic(DIS_SERIAL_N_CHAR_UUID, NIMBLE_PROPERTY::READ);
    _pSerialNumchar->setValue(_serial_num.c_str());
  }

  if (_fw_version != "") {
    _pFWVerchar = _pDISService->createCharacteristic(DIS_FW_VER_CHAR_UUID, NIMBLE_PROPERTY::READ);
    _pFWVerchar->setValue(_fw_version.c_str());
  }

  if (_hw_version != "") {
    _pHWVerchar = _pDISService->createCharacteristic(DIS_HW_VERSION_CHAR_UUID, NIMBLE_PROPERTY::READ);
    _pHWVerchar->setValue(_hw_version.c_str());
  }

  if (_manufacturer != "") {
    _pManufacturerchar = _pDISService->createCharacteristic(DIS_MNF_CHAR_UUID, NIMBLE_PROPERTY::READ);
    _pManufacturerchar->setValue(_manufacturer.c_str());
  }
  _pBLEOTAService->start();

  if ((_model != "") || (_serial_num != "") || (_fw_version != "") || (_hw_version != "") || (_manufacturer != ""))
    _pDISService->start();
}


void NimBLEOTAClass::sendCommandAnswer(uint16_t command_id, uint16_t status) {

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

void NimBLEOTAClass::sendFWAnswer(uint16_t index, uint16_t status) {

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