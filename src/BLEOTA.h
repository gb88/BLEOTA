#ifndef BLEOTA_H
#define BLEOTA_H
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Update.h>
#include "mbedtls/pk.h"
#include "mbedtls/sha256.h"


#define BLE_OTA_SERVICE_UUID "00008018-0000-1000-8000-00805f9b34fb"
#define RECV_FW_UUID "00008020-0000-1000-8000-00805f9b34fb"
#define COMMAND_UUID "00008022-0000-1000-8000-00805f9b34fb"

#define DIS_SERVICE_UUID "180A"
#define DIS_MODEL_CHAR_UUID "2A24"
#define DIS_SERIAL_N_CHAR_UUID "2A25"
#define DIS_FW_VER_CHAR_UUID "2A26"
#define DIS_HW_VERSION_CHAR_UUID "2A27"
#define DIS_MNF_CHAR_UUID "2A29"

#define START_OTA 0x0001
#define START_SPIFFS 0x0004
#define STOP_OTA 0x0002

#define ACK 0x0000
#define NACK 0x0001
#define SIGN_ERROR 0x0003
#define CRC_ERROR 0x0001
#define INDEX_ERROR 0x0002


class BLEOTAClass {
public:
  BLEOTAClass();
  void begin(BLEServer* pServer, bool secure = false);
  bool setKey(const char * key, uint32_t len);
  void setModel(String model);
  void setSerialNumber(String serial_num);
  void setFWVersion(String fw_version);
  void setHWVersion(String hw_version);
  void setManufactuer(String manufacturer);
  void init();
  void process(void);
  bool isRunning(void);
  void abort(void);
  float progress();
  const char* getBLEOTAuuid(void);

  void CommandHandler(BLECharacteristic* pChar, uint8_t* data, uint16_t len);
  void FWHandler(BLECharacteristic* pChar, uint8_t* data, uint16_t len);

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

  bool _done;
  bool _secure;
  uint8_t _ble_answer[20];

  String _model;
  String _serial_num;
  String _fw_version;
  String _hw_version;
  String _manufacturer;

  uint32_t _file_size;
  uint32_t _file_written;
  uint8_t _block[4096];
  uint16_t _expected_sector_index;
  uint16_t _block_size;
  uint16_t _block_crc;
  /* secure */
  mbedtls_sha256_context _sha256_ctx;
  mbedtls_pk_context _pk_context;
  uint8_t _hash[32];
  const char * _key;
  uint8_t _signature[256];
  uint16_t _signature_index;
  
  
  void hashBegin(void);
  void hashAdd(const void *data, uint32_t len);
  void hashEnd(void);
  bool signatureVerify(void);
  uint16_t getSignatureLen(void);
  
  /* BLE answer */

  void sendCommandAnswer(BLECharacteristic* pChar, uint16_t command_id, uint16_t status);
  void sendFWAnswer(BLECharacteristic* pChar, uint16_t index, uint16_t status);
  uint16_t crc16(uint16_t init, uint8_t* data, uint16_t length);
};

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_UPDATE)
extern BLEOTAClass BLEOTA;
#endif

#endif  //BLEOTA_H