# BLEOTA description

Library inspired by https://components.espressif.com/components/espressif/ble_ota that implement the firmware and SPIFFS/LittleFS 
OTA via BLE and writes it to flash, sector by sector, until the upgrade is complete.

## 1. Services definition

The library add two services:

- `DIS Service`: Displays software and hardware version information, manufacturer, model and serial number
- `OTA Service`: It is used for OTA upgrade

|  Service   | UUID |
|  :----  | ----  |
|  BLE_OTA_SERVICE  | 00008018-0000-1000-8000-00805f9b34fb | 
|  DIS_SERVICE_UUID | 				   180A                | 

## 2. DIS Service Characteristics definition

The `DIS Service` can contains 5 read only characteristics that can be setted up before `BLEOTA.init`, if none of these is assigned
the service is not added.

|  Characteristics   | UUID  |  Prop   | description  | Method |
|  ----  | ----  |  ----  | ----  | ----  |
|  DIS_MODEL_CHAR | 0x2A24 | Read  | Model Number String | setModel |
|  DIS_SERIAL_N_CHAR  | 0x2A25 | Read  | Serial Number String | setSerialNumber |
|  DIS_FW_VER_CHAR  | 0x2A26 | Read  | Firmware Revision String | setFWVersion |
|  DIS_HW_VERSION_CHAR  | 0x2A27 | Read  | Hardware Revision String | setHWVersion |
|  DIS_MNF_CHAR | 0x2A29 | Read  | Manufacturer Name String | setManufactuer |

## 3. OTA Service Characteristics definition

The `OTA Service` can contains 2 characteristics to perform the OTA process.

|  Characteristics   | UUID  |  Prop   | description  |
|  ----  | ----  |  ----  | ----  |
|  RECV_FW_CHAR | 00008020-0000-1000-8000-00805f9b34fb | Write, Notify  | Firmware received, send ACK |
|  COMMAND_CHAR  | 00008022-0000-1000-8000-00805f9b34fb | Write, Notify  | Send the command and ACK |


## 4. OTA Service data transmission details

### 4.1 Command package format

|  unit   | Command_ID  |  PayLoad   | CRC16  |
|  ----  | ----  |  ----  | ----  |
|  Byte | Byte: 0 ~ 1 | Byte: 2 ~ 17  | Byte: 18 ~ 19 |

Command_ID:

- 0x0001: Start Flash OTA, Payload bytes(2 to 5), indicates the length of the firmware. Other Payload is set to 0 by default. CRC16 calculates bytes(0 to 17).
- 0x0004: Start SPIFFS OTA, Payload bytes(2 to 5), indicates the length of the SPIFFS. Other Payload is set to 0 by default. CRC16 calculates bytes(0 to 17).
- 0x0002: Stop OTA, and the remaining Payload will be set to 0. CRC16 calculates bytes(0 to 17).
- 0x0003: The Payload bytes(2 or 3) is the payload of the Command_ID for which the response will be sent. Payload bytes(4 to 5) is a response to the command. 0x0000 indicates accept, 0x0001 indicates reject, 0x0003 indicate signature error. Other payloads are set to 0. CRC16 computes bytes(0 to 17).

### 4.2 Firmware package format

The format of the firmware package sent by the client is as follows:

|  unit   | Sector_Index  |  Packet_Seq   | PayLoad  |
|  ----  | ----  |  ----  | ----  |
|  Byte | Byte: 0 ~ 1 | Byte: 2  | Byte: 3 ~ (MTU_size - 4) |

- Sector_Index：Indicates the number of sectors, sector number increases from 0, cannot jump, must be send 4K data and then start transmit the next sector, otherwise it will immediately send the error ACK for request retransmission.
- Packet_Seq：If Packet_Seq is 0xFF, it indicates that this is the last packet of the sector, and the last 2 bytes of Payload is the CRC16 value of 4K data for the entire sector, the remaining bytes will set to 0x0. Server will check the total length and CRC of the data from the client, reply the correct ACK, and then start receive the next sector of firmware data.

The format of the reply packet is as follows:

|  unit   | Sector_Index  |  ACK_Status   | CRC6  |
|  ----  | ----  |  ----  | ----  |
|  Byte | Byte: 0 ~ 1 | Byte: 2 ~ 3  | Byte: 18 ~ 19 |

ACK_Status:

- 0x0000: Success
- 0x0001: CRC error
- 0x0002: Sector_Index error, bytes(4 ~ 5) indicates the desired Sector_Index
- 0x0003： Payload length error

## 5. Security

After the creation of BLE server call BLEOTA.begin with the Server pointer 
```
// Create the BLE Device
BLEDevice::init("ESP32");

// Create the BLE Server
pServer = BLEDevice::createServer();
pServer->setCallbacks(new ServerCallbacks());

// Begin BLE OTA with security
BLEOTA.begin(pServer, true);
``` 
Add the public key
```
// Add pub key
BLEOTA.setKey(pub_key, strlen(pub_key));
```

that has been previously defined as:
```
const char pub_key[] = "-----BEGIN PUBLIC KEY-----\nMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAw/rrOWrykXdTPFwZzljd\nPuuhkRDQUJQu0et5dWNd4ntbh+Qp9qDiZMEj9PcUkw6VUCWFTcSFkOR4i3M+H3g3\nJsKGe5y45DGK8HvgOAnGGUtb0/V2UVZAqiUzJ2cXSK+1688/kWRBSv6OTMXFg2Fa\nGnaIEupUIJZfnBjJmZOhqJll+kxvkxE3CjbnnP8SZ31ybItPV3DyML/7RZ3gMBB5\ngVh44kzAIzPD+NtSSU/RNbWOi3rgNPx1SLzUPjThkHAkVRJ96pWEctiblv2XwoIm\n1ZJEeeda3O46+zCpsI1Ph5oo8mi4QWj1MvkQldo3XtLWRtH/IbMLEgRSR5y054Tg\n0QIDAQAB\n-----END PUBLIC KEY-----";
```
with the content of the .pub file

### 5.1 OTA File signature

The process for signing the file is:

- generate the private key (keep this secret) and the corresponding public key:
```
openssl genrsa -out priv_key.pem 2048
openssl rsa -in priv_key.pem -pubout > rsa_key.pub
```
- export the compiled sketch or SPIFFS to get the bin file and create the digest of the file with SHA256 hash with the private key:
```
openssl dgst -sign priv_key.pem -keyform PEM -sha256 -out signature.sign -binary file.ino.bin
```
- throw it all in one file
```
cat file.ino.bin signature.sign > ota.bin
```
	
use the **ota.bin** to perform the update

## 6.  Sample code

[BLEOTA](https://github.com/gb88/BLEOTA/tree/main/examples/bleota)

After the creation of BLE server call BLEOTA.begin with the Server pointer 
```
// Create the BLE Device
BLEDevice::init("ESP32");

// Create the BLE Server
pServer = BLEDevice::createServer();
pServer->setCallbacks(new ServerCallbacks());

// Begin BLE OTA
BLEOTA.begin(pServer);
```  
Eventually set the values of the `DIS Service`
```
#ifdef MODEL
  BLEOTA.setModel(MODEL);
#endif
#ifdef SERIAL_NUM
  BLEOTA.setSerialNumber(SERIAL_NUM);
#endif
#ifdef FW_VERSION
  BLEOTA.setFWVersion(FW_VERSION);
#endif
#ifdef HW_VERSION
  BLEOTA.setHWVersion(HW_VERSION);
#endif
#ifdef MANUFACTURER
  BLEOTA.setManufactuer(MANUFACTURER);
#endif
```
Call BLEOTA.init to add the service
```
BLEOTA.init();
```

If you want to add the `OTA Service` to the advertising in the setup add 
```
pAdvertising->addServiceUUID(BLEOTA.getBLEOTAuuid());
```

Add to loop the process function
```
BLEOTA.process();
```

[BLEOTA_SECURE](https://github.com/gb88/BLEOTA/tree/main/examples/bleota_secure)

After the creation of BLE server call BLEOTA.begin with the Server pointer and security enabled and set the public key [Security](#5-security "Goto Security")
```
// Create the BLE Device
BLEDevice::init("ESP32");

// Create the BLE Server
pServer = BLEDevice::createServer();
pServer->setCallbacks(new ServerCallbacks());

// Begin BLE OTA with security
BLEOTA.begin(pServer, true);
// Add pub key
BLEOTA.setKey(pub_key, strlen(pub_key));
``` 
## 6. WebApp
[BLEOTA_WEBAPP](https://gb88.github.io/BLEOTA/)

Small web application that implement the OTA process over BLE with Web Bluetooth
