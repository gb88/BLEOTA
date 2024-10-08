#include "BLEOTA.h"

BLEOTABase::BLEOTABase() {
}

void BLEOTABase::setModel(String model) {
  _model = model;
}

void BLEOTABase::setSerialNumber(String serial_num) {
  _serial_num = serial_num;
}

void BLEOTABase::setFWVersion(String fw_version) {
  _fw_version = fw_version;
}

void BLEOTABase::setHWVersion(String hw_version) {
  _hw_version = hw_version;
}

void BLEOTABase::setManufactuer(String manufacturer) {
  _manufacturer = manufacturer;
}

void BLEOTABase::process(bool reset) {
  processCallback();
  if (_done) {
	if(reset)
	{
		delay(500);
		ESP.restart();
	}
  }
}

bool BLEOTABase::isRunning(void) {
  return FlashZ::getInstance().isRunning();
}
void BLEOTABase::abort(void) {
  _done = false;
  FlashZ::getInstance().abortz();
}

float BLEOTABase::progress(void) {
  if(_file_size > 0)
  {
	float temp = (float)_file_written;
	temp /= _file_size;
	temp *= 100;
	return temp;
  }
	return 0;
}


const char* BLEOTABase::getBLEOTAuuid(void) {
  return BLE_OTA_SERVICE_UUID;
}

void BLEOTABase::CommandHandler(uint8_t* data, uint16_t len) {
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
	  _type = U_FLASH;
	  if (FlashZ::getInstance().isRunning()) {
		FlashZ::getInstance().abortz();
	  }
	  _mode_z = false;
	  _file_written = 0;
	  _signature_index = 0;
	  if(_secure)
	  {
		hashBegin();
		memset(_signature,0,sizeof(_signature));
	  }
	  sendCommandAnswer(START_OTA, ACK);
	  deferCallback(invokeBeforeStartOTACallback);
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
	  if (FlashZ::getInstance().isRunning()) {
		FlashZ::getInstance().abortz();
	  }
	  _type = U_SPIFFS;
      _mode_z = false;
	  if(_secure)
	  {
			hashBegin();
			memset(_signature,0,sizeof(_signature));
	  }
      _file_written = 0;
	  _signature_index = 0;
      sendCommandAnswer(START_SPIFFS, ACK);
      deferCallback(invokeBeforeStartSPIFFSCallback);
    } else if ((data[0] == 0x02) && (data[1] == 0x00))  //stop command
    {
      if (_file_written != _file_size) {
        sendCommandAnswer(STOP_OTA, NACK);
      }
	  if(_secure)
	  {
	    hashEnd();
		if(signatureVerify())
		{
		  FlashZ::getInstance().abortz();
          sendCommandAnswer(STOP_OTA, SIGN_ERROR);
		  deferCallback(invokeAfterAbortCallback);
		  return;
		}
	  }
      if (FlashZ::getInstance().endz()) {
        if (FlashZ::getInstance().isFinished()) {
          sendCommandAnswer(STOP_OTA, ACK);
		  deferCallback(invokeAfterStopCallback);
          _done = true;
        } else {
          FlashZ::getInstance().abortz();
          sendCommandAnswer(STOP_OTA, NACK);
		  deferCallback(invokeAfterAbortCallback);
        }
      }
    }
  }
}

void BLEOTABase::FWHandler(uint8_t* data, uint16_t len) {
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
	  if((_block_size == 0) && (_file_written == 0))
	  {
	    _mode_z = (data[3] == ZLIB_HEADER);    // check if we have a compressed image 
	    if (!(_mode_z ? FlashZ::getInstance().beginz(UPDATE_SIZE_UNKNOWN, _type) : FlashZ::getInstance().begin(UPDATE_SIZE_UNKNOWN, _type)))
		{
         FlashZ::getInstance().abortz();
		 sendFWAnswer(sector_index, START_ERROR);
		 return;
        }
	  }
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
			bool _final = false;
			if((_file_written+_block_size) == _file_size)
				_final = true;
			if (FlashZ::getInstance().writez(_block, _block_size, _final) == _block_size) {
				if(_secure)
					hashAdd(_block,_block_size);
			  _file_written += _block_size;
			  sendFWAnswer(_expected_sector_index, ACK);
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
				sendFWAnswer(_expected_sector_index, ACK);
				_expected_sector_index++;
			}
			else if((_file_written+_block_size) > _file_size)
			{
				uint32_t to_write = _file_size - _file_written;
				//only part of the block is signature
				if (FlashZ::getInstance().writez(_block, to_write, true) == to_write) {
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
				  sendFWAnswer(_expected_sector_index, ACK);
				  _expected_sector_index++;
				}
			}
		}
      } else {
        //crc error
        sendFWAnswer(sector_index, CRC_ERROR);
      }
	  _block_size = 0;
      _block_crc = 0;
    }
  } else {
    //sector index error
    sendFWAnswer(sector_index, INDEX_ERROR);
  }
}

void BLEOTABase::hashBegin(void)
{
	mbedtls_sha256_free( &_sha256_ctx );
	mbedtls_sha256_init( &_sha256_ctx );
	mbedtls_sha256_starts( &_sha256_ctx, 0 );
}

void BLEOTABase::hashAdd(const void *data, uint32_t len)
{
	mbedtls_sha256_update( &_sha256_ctx, (const unsigned char *)data, len);
}

void BLEOTABase::hashEnd(void)
{
	mbedtls_sha256_finish( &_sha256_ctx, _hash );
}

bool BLEOTABase::setKey(const char * key, uint32_t len)
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

uint16_t BLEOTABase::getSignatureLen(void)
{
	return (uint16_t)sizeof(_signature);
}

bool BLEOTABase::signatureVerify(void)
{
	if(mbedtls_pk_verify( &_pk_context, (mbedtls_md_type_t)MBEDTLS_MD_SHA256, (const unsigned char*)_hash, sizeof(_hash), _signature, getSignatureLen()))
	{
		return true;
	}
	return false;
}

uint16_t BLEOTABase::crc16(uint16_t init, uint8_t* data, uint16_t length) {
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

void BLEOTABase::invokeBeforeStartOTACallback(BLEOTACallbacks* p) {
	if (p != nullptr) {
		p->beforeStartOTA();
	}
}

void BLEOTABase::invokeBeforeStartSPIFFSCallback(BLEOTACallbacks* p) {
	if (p != nullptr) {
		p->beforeStartSPIFFS();
	}
}

void BLEOTABase::invokeAfterStopCallback(BLEOTACallbacks* p) {
	if (p != nullptr) {
		p->afterStop();
	}
}

void BLEOTABase::invokeAfterAbortCallback(BLEOTACallbacks* p) {
	if (p != nullptr) {
		p->afterAbort();
	}
}

void BLEOTABase::setCallbacks(BLEOTACallbacks* cb){
	_pCallbacks = cb;
}

bool BLEOTABase::deferCallback(void (*callback)(BLEOTACallbacks* p)) {
	if (_pCallbacks == nullptr) return false;
    if (_count == QUEUE_SIZE) {
        // Queue is full
        return false;
    }
    _callbacks[_rear] = callback;
    _rear = (_rear + 1) % QUEUE_SIZE;
    _count++;
    return true;
}

// Process all callbacks in the queue
void BLEOTABase::processCallback(void) {
	if (_pCallbacks == nullptr) return;
	if (_count == 0) return;
    while (_count > 0) {
        void (*callback)(BLEOTACallbacks*) = _callbacks[_front];
        callback(_pCallbacks);
        _front = (_front + 1) % QUEUE_SIZE;
        _count--;
    }
}


