#include "BLEOTA.h"

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_UPDATE)
BLEOTAClass BLEOTA;
#endif

class recvFWCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    uint8_t* data;
    if (pCharacteristic->getLength() >= 4) {
      data = pCharacteristic->getData();
      BLEOTA.FWHandler(pCharacteristic, data, pCharacteristic->getLength());
    }
  }
};

class commandCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    uint8_t* data;
    if (pCharacteristic->getLength() >= 20) {
      data = pCharacteristic->getData();
      BLEOTA.CommandHandler(pCharacteristic, data, pCharacteristic->getLength());
    }
  }
};

BLEOTAClass::BLEOTAClass() {
}

void BLEOTAClass::begin(BLEServer* pServer, bool secure) {
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
  _secure = secure;

  _model = "";
  _serial_num = "";
  _fw_version = "";
  _hw_version = "";
  _manufacturer = "";
  
  _pCallbacks = nullptr;
}

void BLEOTAClass::setModel(String model) {
  _model = model;
}

void BLEOTAClass::setSerialNumber(String serial_num) {
  _serial_num = serial_num;
}

void BLEOTAClass::setFWVersion(String fw_version) {
  _fw_version = fw_version;
}

void BLEOTAClass::setHWVersion(String hw_version) {
  _hw_version = hw_version;
}

void BLEOTAClass::setManufactuer(String manufacturer) {
  _manufacturer = manufacturer;
}

void BLEOTAClass::init(void) {
  // Create the OTA Service
  _pBLEOTAService = _pServer->createService(BLE_OTA_SERVICE_UUID);

  // Create a BLE Characteristic
  _pRecvFWchar = _pBLEOTAService->createCharacteristic(
    RECV_FW_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE);
  _pRecvFWchar->setCallbacks(new recvFWCallback());

  _pCommandchar = _pBLEOTAService->createCharacteristic(
    COMMAND_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE);

  _pCommandchar->setCallbacks(new commandCallback());

  // Create a BLE Descriptor
  _pRecvFWchar->addDescriptor(new BLE2902());
  _pCommandchar->addDescriptor(new BLE2902());

  // Create the DIS Service
  if ((_model != "") || (_serial_num != "") || (_fw_version != "") || (_hw_version != "") || (_manufacturer != ""))
    _pDISService = _pServer->createService(DIS_SERVICE_UUID);

  if (_model != "") {
    _pModelchar = _pDISService->createCharacteristic(DIS_MODEL_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    _pModelchar->setValue(_model.c_str());
  }

  if (_serial_num != "") {
    _pSerialNumchar = _pDISService->createCharacteristic(DIS_SERIAL_N_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    _pSerialNumchar->setValue(_serial_num.c_str());
  }

  if (_fw_version != "") {
    _pFWVerchar = _pDISService->createCharacteristic(DIS_FW_VER_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    _pFWVerchar->setValue(_fw_version.c_str());
  }

  if (_hw_version != "") {
    _pHWVerchar = _pDISService->createCharacteristic(DIS_HW_VERSION_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    _pHWVerchar->setValue(_hw_version.c_str());
  }

  if (_manufacturer != "") {
    _pManufacturerchar = _pDISService->createCharacteristic(DIS_MNF_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    _pManufacturerchar->setValue(_manufacturer.c_str());
  }
  _pBLEOTAService->start();

  if ((_model != "") || (_serial_num != "") || (_fw_version != "") || (_hw_version != "") || (_manufacturer != ""))
    _pDISService->start();
}

void BLEOTAClass::process(void) {
  if (_done) {
    //TODO: add callback
    delay(500);
    ESP.restart();
  }
}

bool BLEOTAClass::isRunning(void) {
  return Update.isRunning();
}
void BLEOTAClass::abort(void) {
  _done = false;
  Update.abort();
}

float BLEOTAClass::progress(void) {
  if(_file_size > 0)
  {
	float temp = (float)_file_written;
	temp /= _file_size;
	temp *= 100;
	return temp;
  }
	return 0;
}


const char* BLEOTAClass::getBLEOTAuuid(void) {
  return BLE_OTA_SERVICE_UUID;
}

void BLEOTAClass::CommandHandler(BLECharacteristic* pChar, uint8_t* data, uint16_t len) {
  uint16_t crc = crc16(0, data, 18);
  uint16_t recv_crc = data[19];
  recv_crc <<= 8;
  recv_crc |= data[18];
  if (crc == recv_crc) {
    if ((data[0] == 0x01) && (data[1] == 0x00))  //start command
    {
      _file_size = data[5];
      _file_size <<= 8;
      _file_size |= data[4];
      _file_size <<= 8;
      _file_size |= data[3];
      _file_size <<= 8;
      _file_size |= data[2];
	  if(_secure)
	  {
		  _file_size -= getSignatureLen();
	  }
      _expected_sector_index = 0;
      _done = false;
	  if (Update.isRunning()) {
		Update.abort();
	  }
      if (Update.begin(_file_size, U_FLASH)) {
        _file_written = 0;
		_signature_index = 0;
		if(_secure)
		{
			hashBegin();
			memset(_signature,0,sizeof(_signature));
		}
        sendCommandAnswer(pChar, START_OTA, ACK);
		invokeBeforeStartOTACallback();
      } else {
        sendCommandAnswer(pChar, START_OTA, NACK);
      }
    }
    if ((data[0] == 0x04) && (data[1] == 0x00))  //start command
    {
      _file_size = data[5];
      _file_size <<= 8;
      _file_size |= data[4];
      _file_size <<= 8;
      _file_size |= data[3];
      _file_size <<= 8;
      _file_size |= data[2];
	  if(_secure)
	  {
		  _file_size -= getSignatureLen();
	  }
      _expected_sector_index = 0;
      _done = false;
	  if (Update.isRunning()) {
		Update.abort();
	  }
      if (Update.begin(_file_size, U_SPIFFS)) {
		if(_secure)
		{
			hashBegin();
			memset(_signature,0,sizeof(_signature));
		}
        _file_written = 0;
		_signature_index = 0;
        sendCommandAnswer(pChar, START_SPIFFS, ACK);
		invokeBeforeStartSPIFFSCallback();
      } else {
        sendCommandAnswer(pChar, START_SPIFFS, NACK);
      }
    } else if ((data[0] == 0x02) && (data[1] == 0x00))  //stop command
    {
      if (_file_written != _file_size) {
        sendCommandAnswer(pChar, STOP_OTA, NACK);
      }
	  if(_secure)
	  {
	    hashEnd();
		if(signatureVerify())
		{
		  Update.abort();
          sendCommandAnswer(pChar, STOP_OTA, SIGN_ERROR);
		  invokeAfterAbortCallback();
		  return;
		}
	  }
      if (Update.end()) {
        if (Update.isFinished()) {
          sendCommandAnswer(pChar, STOP_OTA, ACK);
		  invokeAfterStopCallback();
          _done = true;
        } else {
          Update.abort();
          sendCommandAnswer(pChar, STOP_OTA, NACK);
		  invokeAfterStopCallback();
        }
      }
    }
  }
}

void BLEOTAClass::FWHandler(BLECharacteristic* pChar, uint8_t* data, uint16_t len) {
  uint16_t sector_index;
  sector_index = data[1];
  sector_index <<= 8;
  sector_index |= data[0];
  uint8_t packet_sequence = data[2];
  if (sector_index == _expected_sector_index) {
    uint16_t size = len - 3;
    if (packet_sequence == 0xFF) {
      size = len - 5;
    } else if (packet_sequence == 0x00) {
      _block_size = 0;
      _block_crc = 0;
    }
    if ((_block_size + size) <= sizeof(_block)) {
      memcpy(&_block[_block_size], &data[3], size);
    }
    _block_crc = crc16(_block_crc, &data[3], size);
    _block_size += size;
    if (packet_sequence == 0xFF) {
      uint16_t recv_crc = data[len - 1];
      recv_crc <<= 8;
      recv_crc |= data[len - 2];
      if (_block_crc == recv_crc) {
		if((_file_written+_block_size) <= _file_size)
		{
			if (Update.write(_block, _block_size) == _block_size) {
				if(_secure)
					hashAdd(_block,_block_size);
			  _file_written += _block_size;
			  sendFWAnswer(pChar, _expected_sector_index, ACK);
			  _expected_sector_index++;
			}
		}
		else if(_secure) //TODO: check if needed
		{
			if(_file_written >= _file_size)
			{
				//inside the block there is the signature
				if(_signature_index < getSignatureLen())
				{
					memcpy(&_signature[_signature_index],_block, getSignatureLen() - _signature_index); 
					_signature_index += (getSignatureLen() - _signature_index);
					Serial.println(_signature_index);
				}
				sendFWAnswer(pChar, _expected_sector_index, ACK);
				_expected_sector_index++;
			}
			else if((_file_written+_block_size) > _file_size)
			{
				uint32_t to_write = _file_size - _file_written;
				//only part of the block is signature
				if (Update.write(_block, to_write) == to_write) {
				  if(_secure)
				   hashAdd(_block,to_write);
				  _file_written += to_write;
				  //the rest of the block is the signature
				  if(_signature_index < getSignatureLen())
				  {
					uint32_t sign_write;
					if((sizeof(_block) - to_write) > (getSignatureLen() - _signature_index))
					{
						sign_write = getSignatureLen() - _signature_index;
					}
					else
					{
						sign_write = sizeof(_block) - to_write;
					}
					memcpy(&_signature[_signature_index],&_block[to_write], sign_write); 
					_signature_index += sign_write;
					Serial.println(_signature_index);
				  }
				  sendFWAnswer(pChar, _expected_sector_index, ACK);
				  _expected_sector_index++;
				}
			}
		}
      } else {
        //crc error
        sendFWAnswer(pChar, sector_index, CRC_ERROR);
      }
	  _block_size = 0;
      _block_crc = 0;
    }
  } else {
    //sector index error
    sendFWAnswer(pChar, sector_index, INDEX_ERROR);
  }
}

void BLEOTAClass::hashBegin(void)
{
	mbedtls_sha256_free( &_sha256_ctx );
	mbedtls_sha256_init( &_sha256_ctx );
	mbedtls_sha256_starts( &_sha256_ctx, 0 );
}

void BLEOTAClass::hashAdd(const void *data, uint32_t len)
{
	mbedtls_sha256_update( &_sha256_ctx, (const unsigned char *)data, len);
}

void BLEOTAClass::hashEnd(void)
{
	mbedtls_sha256_finish( &_sha256_ctx, _hash );
}

bool BLEOTAClass::setKey(const char * key, uint32_t len)
{
	_key = key;
	if(len < 256)
		return true;
	mbedtls_pk_free(&_pk_context);
	mbedtls_pk_init(&_pk_context);
	if (mbedtls_pk_parse_public_key( &_pk_context, (const unsigned char*)key, (size_t)(len + 1)) != 0 )
	{
		return true;
	}
	return false;
}

uint16_t BLEOTAClass::getSignatureLen(void)
{
	return (uint16_t)sizeof(_signature);
}

bool BLEOTAClass::signatureVerify(void)
{
	if(mbedtls_pk_verify( &_pk_context, (mbedtls_md_type_t)MBEDTLS_MD_SHA256, (const unsigned char*)_hash, sizeof(_hash), _signature, getSignatureLen()))
	{
		return true;
	}
	return false;
}

void BLEOTAClass::sendCommandAnswer(BLECharacteristic* pChar, uint16_t command_id, uint16_t status) {

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
  pChar->setValue(_ble_answer, sizeof(_ble_answer));
  pChar->indicate();
  pChar->notify();
}

void BLEOTAClass::sendFWAnswer(BLECharacteristic* pChar, uint16_t index, uint16_t status) {

  memset(_ble_answer, 0, sizeof(_ble_answer));
  _ble_answer[0] = (index & 0xFF);
  _ble_answer[1] = (index >> 8) & 0xFF;
  _ble_answer[2] = (status & 0xFF);
  _ble_answer[3] = (status >> 8) & 0xFF;
  uint16_t crc = crc16(0, _ble_answer, 18);
  _ble_answer[18] = crc & 0xFF;
  _ble_answer[19] = (crc >> 8) & 0xFF;
  pChar->setValue(_ble_answer, sizeof(_ble_answer));
  pChar->indicate();
  pChar->notify();
}

uint16_t BLEOTAClass::crc16(uint16_t init, uint8_t* data, uint16_t length) {
  uint16_t crc = init;
  for (uint16_t i = 0; i < length; i++) {
    crc ^= (data[i] << 8);
    for (int j = 0; j < 8; j++) {
      if (crc & 0x8000) {
        crc = ((crc << 1) ^ 0x1021) & 0xFFFF;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc & 0xFFFF;
}


void BLEOTAClass::invokeBeforeStartOTACallback(void) {
	if (_pCallbacks != nullptr) {
		_pCallbacks->beforeStartOTA();
	}
}

void BLEOTAClass::invokeBeforeStartSPIFFSCallback(void) {
	if (_pCallbacks != nullptr) {
		_pCallbacks->beforeStartSPIFFS();
	}
}

void BLEOTAClass::invokeAfterStopCallback(void) {
	if (_pCallbacks != nullptr) {
		_pCallbacks->afterStop();
	}
}

void BLEOTAClass::invokeAfterAbortCallback(void) {
	if (_pCallbacks != nullptr) {
		_pCallbacks->afterAbort();
	}
}

void BLEOTAClass::setCallbacks(BLEOTACallbacks* cb){
	_pCallbacks = cb;
}

//Default Callback Methods
void BLEOTACallbacks::beforeStartOTA(void) {
	log_d("BLEOTACallbacks", ">> beforeStartOTA(): Default");
} // beforeStartOTA

void BLEOTACallbacks::beforeStartSPIFFS(void) {
	log_d("BLEOTACallbacks", ">> beforeStartSPIFFS(): Default");
} // beforeStartSPIFFS

void BLEOTACallbacks::afterStop(void) {
	log_d("BLEOTACallbacks", ">> afterStop(): Default");
} // afterStop

void BLEOTACallbacks::afterAbort(void) {
	log_d("BLEOTACallbacks", ">> afterAbort(): Default");
} // afterAbort
