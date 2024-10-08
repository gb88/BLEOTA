#ifndef BLEOTABASE_H
#define BLEOTABASE_H

#include "Arduino.h"
#include "flashz.hpp"
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
#define START_ERROR 0x0005

#define QUEUE_SIZE	8

class BLEOTACallbacks {

public:
	virtual ~BLEOTACallbacks() {};
	/**
	 * @brief Called before the OTA Update starts.
	 *
	 * This callback method is invoked just before the OTA update begins.
	 */
	virtual void beforeStartOTA(void);
	/**
	 * @brief Called before the SPIFFS Update starts.
	 *
	 * This callback method is invoked just before the SPIFFS update begins.
	 */
	virtual void beforeStartSPIFFS(void);
	/**
	 * @brief Called after the Update ends.
	 *
	 * This callback method is invoked just after the update completes.
	 */
	virtual void afterStop(void);
	/**
	 * @brief Called after the OTA Update is aborted.
	 *
	 * This callback method is invoked just after the OTA update is aborted.
	 */
	virtual void afterAbort(void);
}; // BLEOTACallbacks

class BLEOTABase {
public:
  BLEOTABase();
  bool setKey(const char * key, uint32_t len);
  void setModel(String model);
  void setSerialNumber(String serial_num);
  void setFWVersion(String fw_version);
  void setHWVersion(String hw_version);
  void setManufactuer(String manufacturer);
  virtual void init() = 0;
  void process(bool reset = true);
  bool isRunning(void);
  void abort(void);
  float progress();
  const char* getBLEOTAuuid(void);

  void CommandHandler(uint8_t* data, uint16_t len);
  void FWHandler(uint8_t* data, uint16_t len);
  
  void setCallbacks(BLEOTACallbacks* cb);

private:


  int _type;



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

  virtual void sendCommandAnswer(uint16_t command_id, uint16_t status) = 0;
  virtual void sendFWAnswer(uint16_t index, uint16_t status) = 0;
  
  /* Callback methods */
 
  static void invokeBeforeStartOTACallback(BLEOTACallbacks* p);
  static void invokeBeforeStartSPIFFSCallback(BLEOTACallbacks* p);
  static void invokeAfterStopCallback(BLEOTACallbacks* p);
  static void invokeAfterAbortCallback(BLEOTACallbacks* p);
  
  /* Callback defered */ 
  void (*_callbacks[QUEUE_SIZE])(BLEOTACallbacks*);
  bool deferCallback(void (*callback)(BLEOTACallbacks* p));
  void processCallback(void);

  
  
protected:
  String _model;
  String _serial_num;
  String _fw_version;
  String _hw_version;
  String _manufacturer;
  uint8_t _ble_answer[20];
  
  bool _done;
  bool _secure;
  bool _mode_z;
  
  BLEOTACallbacks* _pCallbacks;
  
  uint8_t _front;
  uint8_t _rear;
  uint8_t _count;
  
  
  uint16_t crc16(uint16_t init, uint8_t* data, uint16_t length);
  
};



#endif