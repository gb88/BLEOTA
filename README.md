<!--
  BLEOTA — README (v1.0.6)
  Repository: https://github.com/gb88/BLEOTA
  License: AGPL-3.0
-->

<h1 align="center">BLEOTA</h1>

<p align="center">
  <strong>Firmware &amp; filesystem Over-The-Air updates for every ESP32 variant — entirely over Bluetooth Low Energy.</strong>
</p>

<p align="center">
  <a href="https://github.com/gb88/BLEOTA"><img src="https://img.shields.io/badge/Arduino-Library-00979D?logo=arduino&logoColor=white" alt="Arduino Library"></a>
  <img src="https://img.shields.io/badge/Chips-ESP32%20%7C%20S2%20%7C%20S3%20%7C%20C3%20%7C%20C6%20%7C%20H2-E7352C" alt="Chips">
  <img src="https://img.shields.io/badge/arduino--esp32-2.x%20%7C%203.x-2A5ADA" alt="arduino-esp32">
  <a href="https://gb88.github.io/BLEOTA/"><img src="https://img.shields.io/badge/WebApp-live-10B981" alt="WebApp"></a>
  <a href="./LICENSE"><img src="https://img.shields.io/badge/License-AGPL_v3-blue.svg" alt="License: AGPL v3"></a>
</p>

---

**BLEOTA** lets any ESP32 chip receive new application firmware (`U_FLASH`) **and** filesystem images (`U_SPIFFS` / LittleFS) over a single BLE GATT service, write them to flash sector-by-sector, verify them, and reboot into the new image — with **no USB cable, no Wi-Fi, no MQTT broker**.

Since **v1.0.6**, BLEOTA automatically selects the right BLE backend depending on your setup:

- If you are on **arduino-esp32 core 3.3.0 or newer** (recommended), the library uses only the core's `BLEDevice.h`. That header transparently wraps **BlueDroid** on classic ESP32 and **NimBLE** on S3/C3/C6/H2 — same API both ways. **No external library needed**, no more `multiple definition of npl_freertos_*` link errors when `NimBLE-Arduino` is installed.
- If you are on **an older core** (2.x, 3.0, 3.1, 3.2), BLEOTA keeps its v1.0.x behaviour: it uses **NimBLE-Arduino** when installed, **BlueDroid** otherwise. Legacy sketches continue to work unchanged.

In both cases your sketch can do `#include "BLEOTA.h"` + `BLEOTAClass BLEOTA;` and compile everywhere.

- **Small.** On modern cores the BLE stack is chosen by the core itself, so you only pay for whichever is lightest for your chip — no second copy of NimBLE gets linked in.
- **Fast.** zlib compression + 4 KB sector pipelining, typically 30–60% faster than raw BLE.
- **Safe.** Optional RSA-2048 + SHA-256 firmware signing rejects tampered images **before** the device reboots.
- **Friendly.** A companion [Web Bluetooth WebApp](https://gb88.github.io/BLEOTA/) runs in Chrome / Edge on desktop and Android — no installer, no driver, no SDK.

Protocol inspired by [espressif/ble_ota](https://components.espressif.com/components/espressif/ble_ota). On-device zlib decompression via [vortigont/esp32-flashz](https://github.com/vortigont/esp32-flashz) (bundled).

---

## Table of contents

- [1. At a glance](#1-at-a-glance)
- [2. Quick start](#2-quick-start)
- [3. Installation](#3-installation)
  - [3.1 Library Manager](#31-library-manager)
  - [3.2 Dependencies](#32-dependencies)
  - [3.3 Partition table](#33-partition-table)
- [4. How the automatic backend selection works](#4-how-the-automatic-backend-selection-works)
- [5. Which examples compile where](#5-which-examples-compile-where)
- [6. GATT layout](#6-gatt-layout)
  - [6.1 Device Information Service (0x180A)](#61-device-information-service-0x180a)
  - [6.2 OTA Service](#62-ota-service)
- [7. Protocol specification](#7-protocol-specification)
  - [7.1 Wire conventions](#71-wire-conventions)
  - [7.2 Command frame](#72-command-frame)
  - [7.3 Firmware frame](#73-firmware-frame)
  - [7.4 State machine](#74-state-machine)
  - [7.5 Status / error codes](#75-status--error-codes)
  - [7.6 Sector size, MTU and throughput](#76-sector-size-mtu-and-throughput)
- [8. Compression (zlib)](#8-compression-zlib)
- [9. Secure OTA (signed firmware)](#9-secure-ota-signed-firmware)
  - [9.1 How verification works](#91-how-verification-works)
  - [9.2 Enabling secure mode](#92-enabling-secure-mode)
  - [9.3 Generating keys & signing images](#93-generating-keys--signing-images)
  - [9.4 Image layout on the wire](#94-image-layout-on-the-wire)
- [10. API reference](#10-api-reference)
- [11. User callbacks](#11-user-callbacks)
- [12. WebApp (Web Bluetooth updater)](#12-webapp-web-bluetooth-updater)
- [13. Performance & tuning](#13-performance--tuning)
- [14. Writing your own client](#14-writing-your-own-client)
- [15. Troubleshooting / FAQ](#15-troubleshooting--faq)
- [16. Upgrading from 1.0.5 to 1.0.6](#16-upgrading-from-105-to-106)
- [17. Compatibility matrix](#17-compatibility-matrix)
- [18. License](#18-license)
- [Credits](#credits)

---

## 1. At a glance

| What                      | Detail                                                                                       |
| ------------------------- | -------------------------------------------------------------------------------------------- |
| **Targets**               | ESP32, ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6, ESP32-H2                                      |
| **Core requirement**      | arduino-esp32 ≥ 2.0.0 — **3.3.0+ recommended**                                                |
| **BLE stack (core ≥ 3.3)** | The core's bundled stack — BlueDroid on classic ESP32, NimBLE on S3/C3/C6/H2                 |
| **BLE stack (core < 3.3)** | Auto: `NimBLE-Arduino` if installed, else BlueDroid via `BLEDevice.h`                        |
| **Update types**          | Application (`U_FLASH`) and filesystem (`U_SPIFFS` / LittleFS)                               |
| **Transport**             | BLE GATT, single service (2 characteristics) + optional DIS (`0x180A`)                        |
| **Verification**          | CRC-16/CCITT on every 4 KB sector; optional RSA-2048 + SHA-256 signature on the whole image  |
| **Compression**           | zlib (autodetected by `0x78` header) via ESP32 in-ROM miniz decoder                          |
| **Reboot**                | Automatic 500 ms after OTA success (can be disabled per call)                                |
| **Client included**       | Static Web Bluetooth app in `docs/` — published at <https://gb88.github.io/BLEOTA/>          |
| **External dependencies** | None on core ≥ 3.3.0; `NimBLE-Arduino` (optional) on older cores                              |
| **License**               | AGPL-3.0                                                                                     |

---

## 2. Quick start

```cpp
#include <BLEDevice.h>
#include <BLEServer.h>
#include "BLEOTA.h"

BLEOTAClass BLEOTA;
BLEServer*  pServer = nullptr;
bool        connected = false, wasConnected = false;

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*)    { connected = true;  }
  void onDisconnect(BLEServer*) { connected = false; }
};

void setup() {
  Serial.begin(115200);

  BLEDevice::init("ESP32-OTA");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEOTA.begin(pServer);
  BLEOTA.setModel("MyBoard");
  BLEOTA.setFWVersion("1.0.0");
  BLEOTA.setManufactuer("ACME");    // note: historical typo preserved
  BLEOTA.init();

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(BLEOTA.getBLEOTAuuid());
  BLEDevice::startAdvertising();
}

void loop() {
  if (!connected && wasConnected) {
    delay(500);
    pServer->startAdvertising();
  }
  wasConnected = connected;
  BLEOTA.process();         // drives the state machine, auto-reboots on success
  delay(1000);
}
```

Flash it, open <https://gb88.github.io/BLEOTA/> in Chrome or Edge, click **Connect**, drop a `.bin`, press **Start**.

This sketch runs unchanged on classic ESP32, S2, S3, C3, C6 and H2 — and across arduino-esp32 versions from 2.0 to the latest.

---

## 3. Installation

### 3.1 Library Manager

- **Arduino IDE** → *Tools → Manage Libraries…* → search **BLEOTA** → Install.
- **PlatformIO** — add to `platformio.ini`:
  ```ini
  lib_deps = gb88/BLEOTA @ ^1.0.6
  ```
- Or clone the repository into your `libraries/` folder.

### 3.2 Dependencies

The only hard dependency is **arduino-esp32 core ≥ 2.0.0**. Everything else depends on your setup:

| Setup                                      | What BLEOTA uses                                                       | Extra library needed? |
| ------------------------------------------ | ---------------------------------------------------------------------- | --------------------- |
| core ≥ 3.3.0                               | The core's `BLEDevice.h` (wraps BlueDroid or NimBLE behind the scenes) | **None**              |
| core < 3.3.0 + BlueDroid                   | The core's `BLEDevice.h` (BlueDroid)                                   | **None**              |
| core < 3.3.0 + NimBLE-Arduino installed    | `NimBLE-Arduino` from h2zero                                           | **NimBLE-Arduino 2.x**    |

BLEOTA **never auto-installs** `NimBLE-Arduino`. If you want the NimBLE-Arduino backend on an older core, install it manually via Library Manager. If you don't, BLEOTA uses BlueDroid and everything just works.

### 3.3 Partition table

You need **at least two OTA app slots** and — for filesystem updates — a SPIFFS/LittleFS partition. The default Arduino scheme *"Default 4 MB with spiffs (1.2 MB APP / 1.5 MB SPIFFS)"* works out of the box. For larger firmware, pick *"Minimal SPIFFS (Large APPS with OTA)"* or define a custom `partitions.csv`.

If you try to push an image larger than the target OTA partition the ESP32 Updater aborts with `START_ERROR` (`0x0005`) — you will see it as a `0x0005` status on the first sector ACK.

---

## 4. How the automatic backend selection works

`BLEOTA.h` reads `ESP_ARDUINO_VERSION` (from `esp_arduino_version.h`, available since core 2.0.0) and branches at preprocessor time:

```cpp
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 3, 0)
  // Modern path: BLEDevice.h from the core.
  // Classic ESP32 → BlueDroid. S3/C3/C6/H2 → NimBLE (from core).
  // Crucially, nothing references <NimBLEDevice.h>, so having
  // NimBLE-Arduino installed cannot pollute the link.
#else
  #if __has_include(<NimBLEDevice.h>)
    // Legacy v1.x behaviour: NimBLE-Arduino picked up automatically.
  #else
    // Legacy: BlueDroid via BLEDevice.h.
  #endif
#endif
```

Because **Arduino CLI's library discovery runs the C++ preprocessor** (not a textual regex), the branches that are `#if`-ed out never contribute their `#include` directives to dependency resolution. On core ≥ 3.3.0, `NimBLE-Arduino` is invisible to the build — even if the user has it installed — which eliminates the `multiple definition of npl_freertos_*` link errors on ESP32-S3/C3/C6/H2.

### Why the core's `BLEDevice.h` is the preferred path

Starting with arduino-esp32 **3.3.0** (September 2025), the core's `BLEDevice.h` is a **dual-stack wrapper**. Internally it looks like:

```cpp
#if defined(CONFIG_BLUEDROID_ENABLED)
  // classic ESP32 glue
#elif defined(CONFIG_NIMBLE_ENABLED)
  // S3/C3/C6/H2 glue, using NimBLE from libbt.a
#endif
```

Those two branches implement the **same C++ API**: `BLEDevice`, `BLEServer`, `BLECharacteristic`, `BLECharacteristicCallbacks::onWrite(BLECharacteristic*)`, `BLEAdvertising`, `BLE2902`, etc. Stack-specific extensions exist but are opt-in. BLEOTA uses only the common API, so a single implementation covers all six chip families.

For users the perspective is simple:

- Sketch code uses `BLEDevice::init()`, `BLEServer`, `BLECharacteristic`, and friends — portable to every chip.
- BLEOTA library code does the same. One class, `BLEOTAClass`, compiled once, works everywhere.
- Users migrating from v1.x who wrote `NimBLEOTAClass` get a type alias `using NimBLEOTAClass = BLEOTAClass;` so their code keeps compiling.

---

## 5. Which examples compile where

Six example sketches ship with the library, in two folders:

- **`examples/`** — the three main sketches (`bleota`, `bleota_callback`, `bleota_secure`). They `#include "BLEOTA.h"`, declare `BLEOTAClass BLEOTA;`, and are portable across **all** arduino-esp32 versions and **all** ESP32 chips. These are the recommended starting points for every new project.
- **`examples/nimble/`** — the same three sketches but using `NimBLE-Arduino` **directly** (`NimBLEOTAClass`, `NimBLEConnInfo&`, `NIMBLE_PROPERTY::*`, etc.). These exist only for backwards compatibility with existing NimBLE-Arduino projects. They **require arduino-esp32 core < 3.3.0** with `NimBLE-Arduino 2.x` installed.

The main `examples/` sketches include a `#if defined(CONFIG_BLUEDROID_ENABLED)` guard around a BlueDroid-specific extra `onConnect` overload. When the active stack is BlueDroid (classic ESP32), the overload is active and you get tighter default connection parameters. When the active stack is NimBLE (core 3.3.0+ on S3/C3/C6/H2), that overload is skipped by the preprocessor and only the portable `onConnect(BLEServer*)` is compiled — so the sketch still works end-to-end.

| Example                          | Core 2.x / 3.0–3.2 | Core 3.3.0+ ESP32 classic | Core 3.3.0+ S3/C3/C6/H2 |
| -------------------------------- | :----------------: | :-----------------------: | :---------------------: |
| `examples/bleota/`               | ✅ BlueDroid       | ✅ BlueDroid              | ✅ NimBLE (core)        |
| `examples/bleota_callback/`      | ✅ BlueDroid       | ✅ BlueDroid              | ✅ NimBLE (core)        |
| `examples/bleota_secure/`        | ✅ BlueDroid       | ✅ BlueDroid              | ✅ NimBLE (core)        |
| `examples/nimble/bleota/`        | ✅ NimBLE-Arduino  | ❌ does not compile       | ❌ does not compile     |
| `examples/nimble/bleota_callback/` | ✅ NimBLE-Arduino  | ❌ does not compile       | ❌ does not compile     |
| `examples/nimble/bleota_secure/` | ✅ NimBLE-Arduino  | ❌ does not compile       | ❌ does not compile     |

> ⚠️ **If you are on core 3.3.0+, use the sketches in `examples/`**, not those in `examples/nimble/`. The wire protocol is identical in both, so the Web Bluetooth app doesn't care which backend the device is using.

---

## 6. GATT layout

### 6.1 Device Information Service (0x180A)

Standard Bluetooth SIG service. Each characteristic is added **only if** the corresponding setter was called before `init()`. If **none** are set, the whole DIS service is skipped — no overhead.

| Characteristic      | UUID     | Property | Content                     | Setter                      |
| ------------------- | -------- | -------- | --------------------------- | --------------------------- |
| Model Number        | `0x2A24` | Read     | Model String                | `setModel(String)`          |
| Serial Number ¹     | `0x2A25` | Read     | Serial Number String        | `setSerialNumber(String)`   |
| Firmware Revision   | `0x2A26` | Read     | Firmware Revision String    | `setFWVersion(String)`      |
| Hardware Revision   | `0x2A27` | Read     | Hardware Revision String    | `setHWVersion(String)`      |
| Manufacturer Name   | `0x2A29` | Read     | Manufacturer Name String    | `setManufactuer(String)` ²  |

> ¹ Chrome Web Bluetooth blocks `0x2A25` for anti-fingerprinting. Native apps (nRF Connect, LightBlue, …) can still read it.
> ² The setter name contains a historical typo (“Manufactuer”). Preserved intentionally to avoid breaking existing user code.

### 6.2 OTA Service

| Service           | UUID                                   |
| ----------------- | -------------------------------------- |
| `BLE_OTA_SERVICE` | `00008018-0000-1000-8000-00805f9b34fb` |

| Characteristic | UUID                                     | Properties                    | Role                                        |
| -------------- | ---------------------------------------- | ----------------------------- | ------------------------------------------- |
| `RECV_FW_CHAR` | `00008020-0000-1000-8000-00805f9b34fb`   | Write, Write-NR, Notify, Indicate | Firmware packets in; per-sector ACK out |
| `COMMAND_CHAR` | `00008022-0000-1000-8000-00805f9b34fb`   | Write, Write-NR, Notify, Indicate | Control channel (start / stop) + ACK    |

The client **must enable notifications** on both characteristics before writing the START command. The WebApp uses *write-without-response* (Write-NR) on the RECV characteristic for maximum throughput — flow control happens at the sector level, not at every BLE packet.

---

## 7. Protocol specification

### 7.1 Wire conventions

- **Endianness:** little-endian for all multi-byte integers.
- **CRC:** CRC-16/CCITT, polynomial `0x1021`, init `0x0000`, no reflection, no final XOR.
- **Command frames:** fixed 20 bytes on `COMMAND_CHAR`.
- **Firmware frames:** variable length on `RECV_FW_CHAR`, sized by the negotiated MTU. Usable payload per packet = `MTU − 3` (OTA header) − ATT overhead.

### 7.2 Command frame

Every command is exactly 20 bytes.

| Offset | Size | Field        | Notes                                        |
| ------ | ---- | ------------ | -------------------------------------------- |
| 0–1    | 2 B  | `Command_ID` | See table below                              |
| 2–17   | 16 B | `Payload`    | Command-specific; unused bytes = `0x00`      |
| 18–19  | 2 B  | `CRC16`      | Computed over bytes 0..17                    |

**Command IDs:**

| ID       | Direction | Meaning                                                                                                                  |
| -------- | --------- | ------------------------------------------------------------------------------------------------------------------------ |
| `0x0001` | C → S     | **Start FLASH OTA.** `Payload[0..3]` = total firmware length (uint32 LE, **including signature** when secure mode is on). |
| `0x0004` | C → S     | **Start SPIFFS / LittleFS OTA.** Same payload format as `0x0001`.                                                         |
| `0x0002` | C → S     | **Stop OTA.** Sent after the last sector; triggers signature verification (secure mode) and reboot.                       |
| `0x0003` | S → C     | **Command ACK.** `Payload[0..1]` = replied-to `Command_ID`; `Payload[2..3]` = status (see §7.5).                          |

### 7.3 Firmware frame

Images are split into 4 KB sectors; each sector is split into MTU-sized packets.

**Data packet (not last of sector):**

| Offset | Size      | Field          | Notes                                |
| ------ | --------- | -------------- | ------------------------------------ |
| 0–1    | 2 B       | `Sector_Index` | 0, 1, 2… contiguous, **cannot skip** |
| 2      | 1 B       | `Packet_Seq`   | 0-based counter within the sector    |
| 3…     | MTU − 3   | `Payload`      | Firmware bytes                       |

**Last packet of a sector** (marker `0xFF`):

| Offset     | Size     | Field          | Notes                                                                |
| ---------- | -------- | -------------- | -------------------------------------------------------------------- |
| 0–1        | 2 B      | `Sector_Index` | Same sector index                                                    |
| 2          | 1 B      | `0xFF`         | End-of-sector marker                                                 |
| 3…N        | variable | `Payload`      | Remaining firmware bytes                                             |
| N+1..N+2   | 2 B      | `CRC16`        | Incremental CRC of **all** firmware bytes of this sector (not of the packet) |

**Sector ACK** (20 bytes, notified on `RECV_FW_CHAR`):

| Offset | Size | Field          | Notes                                                               |
| ------ | ---- | -------------- | ------------------------------------------------------------------- |
| 0–1    | 2 B  | `Sector_Index` | Index being ACKed                                                   |
| 2–3    | 2 B  | `ACK_Status`   | `0x0000` OK, `0x0001` CRC err, `0x0002` index err, `0x0005` start err |
| 4–5    | 2 B  | `Expected_Idx` | Valid only on `0x0002` — tells the client what to re-send           |
| 6–17   | —    | reserved       | `0x00`                                                              |
| 18–19  | 2 B  | `CRC16`        | Over bytes 0..17                                                    |

### 7.4 State machine

```text
        ┌──────────┐   0x0001 / 0x0004 (START, total_size)
Client  │   IDLE   │  ──────────────────────────────────────►  Server
        └──────────┘
                        0x0003 ACK (ans=0x0000)
           ◄────────────────────────────────────────────────
        ┌──────────┐
        │   SEND   │   Sector N:  (seq 0) (seq 1) … (seq 0xFF + CRC)
        │  SECTOR  │  ──────────────────────────────────────►
        └──────────┘
                        Sector ACK (ok / CRC err / index err)
           ◄────────────────────────────────────────────────
           (loop until total_size bytes ACKed)

        ┌──────────┐   0x0002 (STOP)
        │   DONE   │  ──────────────────────────────────────►
        └──────────┘
                        0x0003 ACK=0x0000  (clean)
                        0x0003 ACK=0x0003  (signature err)
                        0x0003 ACK=0x0001  (size mismatch / flash err)
           ◄────────────────────────────────────────────────
            device reboots 500 ms later into the new image
```

Notes from the reference implementation:

- The server enforces `Sector_Index == _expected_sector_index`. On mismatch it replies with `0x0002` and keeps `_expected_sector_index` unchanged — the client must **rewind** to the expected index and replay from there.
- `Packet_Seq = 0x00` implicitly resets the per-sector buffer and CRC. There is no explicit retry command: the client simply re-sends the whole sector starting from seq 0.
- In **secure mode** the last 256 bytes of the stream are stripped by the device as the RSA signature; they are not written to flash. The client must include them in the total size sent with the START command.

### 7.5 Status / error codes

| Value    | Where                 | Meaning                                              |
| -------- | --------------------- | ---------------------------------------------------- |
| `0x0000` | any ACK               | `ACK` — success                                      |
| `0x0001` | command / sector      | CRC error **or** generic NACK (size mismatch, flash write error) |
| `0x0002` | sector                | Wrong `Sector_Index` — see `Expected_Idx` field      |
| `0x0003` | command / stop        | Payload length error **or** RSA signature error      |
| `0x0005` | start / first sector  | Cannot start OTA (partition too small, already running) |

### 7.6 Sector size, MTU and throughput

The sector size is **fixed at 4096 bytes** — it matches the SPI flash erase sector size of the ESP32, so each successful sector maps to exactly one erase+program cycle. This is a design invariant of the protocol; changing it would break wire compatibility.

The packet size is **negotiated at connect time via the ATT MTU**. Typical values:

| Stack / platform         | ATT MTU      | OTA payload per packet (= MTU − 3) |
| ------------------------ | ------------ | ---------------------------------- |
| Desktop Chrome + USB BLE | 247          | 244 B                              |
| Android Chrome           | 247          | 244 B                              |
| BLE 5 peripherals        | up to 517    | up to 514 B                        |
| Fallback / old stacks    | 23 (default) | 20 B                               |

The included WebApp does not rely on browser-reported MTU (Web Bluetooth does not expose it); instead it **probes** by attempting `writeValueWithoutResponse` with decreasing candidate sizes (510 → 247 → 185 → 122 → 23) until one succeeds.

---

## 8. Compression (zlib)

The library includes [esp32-flashz](https://github.com/vortigont/esp32-flashz) (under `src/flashz.*`), which uses the ESP32’s in-ROM **miniz** decoder to decompress zlib streams on the fly — no extra RAM allocation for a decoder, and the compressed image never has to be buffered in full before flashing.

**Autodetection.** On the first byte of the first sector the library peeks at `data[3]` (first payload byte). If it equals `0x78` (the zlib header magic) the transfer switches to compressed mode, otherwise it falls back to plain `UpdateClass.write()`.

```cpp
// From src/BLEOTABase.cpp (simplified)
_mode_z = (data[3] == ZLIB_HEADER);        // 0x78
_mode_z
  ? FlashZ::getInstance().beginz(UPDATE_SIZE_UNKNOWN, _type)
  : FlashZ::getInstance().begin (UPDATE_SIZE_UNKNOWN, _type);
```

There is **no client-side flag** to pass — dropping a compressed or uncompressed `.bin` in the WebApp just works.

**How much does it save?**

| Image type                         | Raw   | zlib (-9) | Saving |
| ---------------------------------- | ----- | --------- | ------ |
| Typical ESP32 application (.bin)   | 1.0×  | 0.65–0.75× | 25–35% |
| SPIFFS/LittleFS (text, JSON, HTML) | 1.0×  | 0.15–0.35× | 65–85% |

Translated to transfer time, it cuts FLASH updates by ~30% and SPIFFS updates by **several times** over the same BLE link.

**Compressing an image** (using [pigz](https://github.com/madler/pigz)):

```bash
pigz -9kzc file.ino.bin > file.compressed.bin
```

> ⚠️ **Always compress before signing.** The signature is computed over the exact bytes the device will receive, so the order is:
>
> `compile → (optional) compress → sign → concatenate (file + signature) → flash`.

---

## 9. Secure OTA (signed firmware)

Secure mode verifies an **RSA-2048** signature of the image against a **public key embedded in the firmware**. Without the matching private key, an attacker cannot craft an image that the device will accept — even if they have raw BLE access.

### 9.1 How verification works

While the update is streaming, the device incrementally computes the SHA-256 of everything it writes to flash (excluding the trailing 256-byte signature). When the STOP command arrives:

1. The SHA-256 is finalized.
2. The library calls `mbedtls_pk_verify` with the pre-parsed public key against the 256-byte signature appended to the image.
3. If verification fails, `FlashZ::abortz()` is called (the newly-written partition is **not** activated), a `0x0003` (SIGN_ERROR) ACK is sent, and `afterAbort()` is invoked. The device keeps running the old firmware.
4. If verification succeeds, `FlashZ::endz()` commits the new image, `afterStop()` is invoked, and `process()` triggers a reboot after 500 ms.

This means: **a tampered image never boots**. The verification happens before the partition is marked bootable.

### 9.2 Enabling secure mode

```cpp
// Turn on signature verification
BLEOTA.begin(pServer, /*secure=*/ true);
BLEOTA.setKey(pub_key, strlen(pub_key));
```

The public key lives as a `const char[]` in flash:

```cpp
const char pub_key[] =
  "-----BEGIN PUBLIC KEY-----\n"
  "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAw/rrOWrykXdTPFwZzljd\n"
  // …
  "0QIDAQAB\n"
  "-----END PUBLIC KEY-----";
```

The key is parsed once with `mbedtls_pk_parse_public_key()` when `setKey()` is called; the parsed `mbedtls_pk_context` is kept across updates.

### 9.3 Generating keys & signing images

1. **One-time:** generate a keypair. Keep `priv_key.pem` offline, commit `rsa_key.pub` to the firmware.
   ```bash
   openssl genrsa -out priv_key.pem 2048
   openssl rsa -in priv_key.pem -pubout > rsa_key.pub
   ```
2. **For each release:** build the `.bin`, optionally compress it with pigz, then sign it:
   ```bash
   openssl dgst -sign priv_key.pem -keyform PEM -sha256 \
     -out signature.sign -binary file.ino.bin
   ```
3. **Concatenate** the image and the signature into the file that will be uploaded:
   ```bash
   cat file.ino.bin signature.sign > ota.bin
   ```
4. Upload `ota.bin` via the WebApp or your own client.

### 9.4 Image layout on the wire

```
           ┌──────────────────────────────────────────────┬────────────────────┐
  ota.bin: │        firmware or filesystem image          │  256-byte RSA sig  │
           │     (optionally zlib-compressed, 0x78…)      │  (SHA-256 digest)  │
           └──────────────────────────────────────────────┴────────────────────┘
           ◄──────────────────── total_size sent in START command ─────────────►
```

The `total_size` in the START command **includes** the signature. Internally the library subtracts `getSignatureLen()` (256) from the size before measuring completion, so everything after `_file_size` is treated as signature data and captured into `_signature[]` instead of being flashed.

---

## 10. API reference

All public methods live on `BLEOTAClass` (which inherits from `BLEOTABase`). On legacy cores with `NimBLE-Arduino` installed, `NimBLEOTAClass` provides the same API — `BLEOTA.h` aliases `BLEOTAClass` to it automatically.

### 10.1 Lifecycle

| Method                                                 | Description                                                                                                                                    |
| ------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| `void begin(BLEServer* server, bool secure = false)`   | Bind the library to an already-created `BLEServer`. `secure=true` enables RSA signature verification (call `setKey()` right after).            |
| `void init()`                                          | Register the OTA GATT service and, if any DIS setter was called, also the DIS service. Call **after** all `setXxx()`.                          |
| `void process(bool reset = true)`                      | Must be called periodically from `loop()`. Fires deferred callbacks; if an OTA just completed, reboots the ESP32 after 500 ms (unless `reset = false`). |
| `bool isRunning()`                                     | `true` while a sector is actively being decompressed/written to flash.                                                                         |
| `void abort()`                                         | Abort the current transfer and roll back the flash writer (calls `FlashZ::abortz()`). Safe to call at any time.                                |
| `float progress()`                                     | Progress in percent (0.0 – 100.0), computed on bytes actually written to flash (i.e. after decompression).                                     |
| `const char* getBLEOTAuuid()`                          | Returns the OTA service UUID as a C-string — handy for `pAdvertising->addServiceUUID(...)`.                                                    |

### 10.2 Device Information Service setters

All optional. Call **before** `init()`. Any subset works; if **none** are set, the DIS service is skipped entirely.

| Method                                  | Characteristic |
| --------------------------------------- | -------------- |
| `void setModel(String model)`           | `0x2A24`       |
| `void setSerialNumber(String sn)`       | `0x2A25`       |
| `void setFWVersion(String fw)`          | `0x2A26`       |
| `void setHWVersion(String hw)`          | `0x2A27`       |
| `void setManufactuer(String mfr)`       | `0x2A29`       |

### 10.3 Security

| Method                                          | Description                                                                                                                                           |
| ----------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------- |
| `bool setKey(const char* key, uint32_t len)`    | Load the RSA-2048 public key (PEM string, NUL-terminated). The key is parsed via `mbedtls_pk_parse_public_key()` and stored for all subsequent updates. |

### 10.4 User callbacks

| Method                                   | Description                                                                                          |
| ---------------------------------------- | ---------------------------------------------------------------------------------------------------- |
| `void setCallbacks(BLEOTACallbacks* cb)` | Register a user callback object (see §11). Callbacks are invoked from `process()`, never inside BLE. |

---

## 11. User callbacks

User callbacks (contributed by [@drik](https://github.com/drik)) let your sketch react to OTA lifecycle events without touching the BLE task. Implement any subset of these four hooks:

```cpp
class OTACallbacks : public BLEOTACallbacks {
public:
  void beforeStartOTA()    override { /* prepare for FLASH update (stop sensors, etc.) */ }
  void beforeStartSPIFFS() override { /* prepare for filesystem update (close files)   */ }
  void afterStop()         override { /* update finished successfully                  */ }
  void afterAbort()        override { /* update aborted or signature rejected          */ }
};

// …in setup():
BLEOTA.begin(pServer);
BLEOTA.setCallbacks(new OTACallbacks());
```

### Why deferred?

The BLE stack runs on a dedicated FreeRTOS task with a small stack and timing constraints. If your callback did heavy work inside the BLE write handler, the stack could miss ACK deadlines or drop notifications. BLEOTA avoids this by **queuing** up to 8 callback invocations (`QUEUE_SIZE = 8`, ring buffer `_callbacks[]`) from inside BLE, and **draining the queue in `process()`** — which runs on your main loop, where long work is safe.

### Timing considerations around reboot

`process(true)` reboots the device **500 ms after `_done` becomes true**. Because `afterStop()` is drained from the same `process()` call, you essentially have those 500 ms to finish any persistence work.

If you need more time, disable the automatic reset and reboot yourself:

```cpp
void loop() {
  BLEOTA.process(false);    // do NOT auto-reset
}

// …in your callback:
void afterStop() override {
  persistState();
  closeFiles();
  Serial.flush();
  ESP.restart();
}
```

---

## 12. WebApp (Web Bluetooth updater)

A fully-static Web Bluetooth client lives in `docs/` and is published at **<https://gb88.github.io/BLEOTA/>**. No installer, no server code — pure HTML/JS served from GitHub Pages.

**What it does:**

- Scans for devices advertising the OTA service UUID and connects.
- Reads and displays the DIS fields (name, model, FW/HW version, manufacturer).
- Drag-and-drop for `.bin` files; FLASH or SPIFFS selection.
- **MTU probing** — since Web Bluetooth does not expose the negotiated MTU, the app attempts `writeValueWithoutResponse` with sizes 510 / 247 / 185 / 122 / 23 until one succeeds, then uses `probed_size − 3` as payload.
- Live progress, throughput (B/s), sector counter, ETA.
- Automatic per-sector retry (up to 3 attempts) on CRC / index error / ACK timeout.
- Multi-language UI (IT / EN / ES / RU), dark mode, optional log pane.

**Browser support.** Any Chromium-based browser (Chrome, Edge, Brave, Opera) on desktop or Android. Safari and Firefox do **not** implement Web Bluetooth. On iOS the third-party **Bluefy** browser works. The page must be served over **HTTPS** or from `localhost` — browsers block Web Bluetooth on plain HTTP.

**Self-hosting.** The app is a single HTML file under `docs/index.html` with no runtime dependencies. Copy it anywhere you can serve HTTPS.

---

## 13. Performance & tuning

### Reducing OTA time

- **Compress your images** with `pigz -9kz`. FLASH gets ~30% faster, SPIFFS much more.
- **Request tight connection parameters** early. On Android this is decisive — default intervals of 40–50 ms throttle throughput heavily. For BlueDroid, the example sketches do it inside `onConnect`; for NimBLE (core 3.3.0+) call `BLEDevice::setPreferredConnectionParams(0x06, 0x12, 0, 2000)` before `BLEDevice::createServer()`.
- **Use `writeValueWithoutResponse`** on the RECV characteristic. The included WebApp does; so should custom clients. Per-packet responses would roughly halve throughput.
- **Keep `loop()` light during updates.** Suspend non-essential tasks inside `beforeStartOTA` / `beforeStartSPIFFS` (e.g. `vTaskSuspend`, stop ADC DMA, disable LED PWM…).

### Flash footprint

The footprint depends on which stack the core picked for your chip — BlueDroid (classic ESP32) is larger, NimBLE (S3/C3/C6/H2) is lighter. BLEOTA itself adds on top of that:

| BLEOTA option       | Approximate cost |
| ------------------- | ---------------- |
| Core library        | ~12 KB           |
| Secure mode         | + ~20 KB (mbedtls PK + SHA-256 routines, linked lazily) |
| DIS service         | + < 1 KB         |
| Compression support | + ~6 KB          |

### Indicative numbers

Numbers measured on ESP32-WROOM-32 / MTU 247 / Android Chrome, give or take antenna & RF environment:

| Payload                            | Raw      | zlib-compressed |
| ---------------------------------- | -------- | --------------- |
| 1 MB firmware (`.bin`)              | ~14 s    | ~10 s           |
| 4 MB firmware                      | ~55 s    | ~38 s           |
| 1 MB SPIFFS (mostly HTML/JSON)     | ~15 s    | ~3 s            |

---

## 14. Writing your own client

If you want to update the device from your own iOS/desktop/embedded app instead of the WebApp, the minimum algorithm:

```text
1. Connect to the device and enable notifications on
   - 00008022-...  (COMMAND_CHAR)
   - 00008020-...  (RECV_FW_CHAR)

2. Send START:
   - build a 20-byte command frame with Command_ID = 0x0001 (FLASH) or 0x0004 (SPIFFS)
   - Payload[0..3] = total_size  (LE uint32; include signature bytes if secure mode)
   - bytes 18..19 = CRC16 over bytes 0..17
   - write to COMMAND_CHAR and wait for 0x0003 ACK with ans=0x0000.

3. For each 4 KB sector (last one may be shorter):
   a. Reset a running CRC.
   b. Slice sector into MTU-sized chunks. For each chunk:
      - header[0..1] = sector_index (LE)
      - header[2]    = seq (0, 1, 2, …), OR 0xFF on the final chunk
      - append the chunk bytes
      - update running CRC over the chunk bytes
      - on the final chunk, append the 2-byte CRC after the payload
      - writeWithoutResponse to RECV_FW_CHAR
   c. Wait for a notification on RECV_FW_CHAR where:
      - bytes 0..1 match sector_index
      - bytes 2..3 == 0x0000 (ACK)
      If CRC/INDEX error → restart the sector from seq 0 (up to 3 retries).

4. Send STOP:
   - Command_ID = 0x0002, payload = zero, CRC16 at bytes 18..19
   - Wait for ACK:
      - 0x0000 → success (device will reboot ~500 ms later)
      - 0x0003 → signature rejected (secure mode)
      - 0x0001 → size mismatch or flash write error
```

Reference implementation: see `docs/index.html` — the `buildCmd()`, `buildSectorPackets()`, `startOTA()`, `waitForCmd()`, `waitForFw()` functions are short enough to port to any language.

---

## 15. Troubleshooting / FAQ

<details>
<summary><strong><code>multiple definition of 'npl_freertos_*'</code> at link time on ESP32-S3/C3/C6/H2</strong></summary>

This is exactly the bug BLEOTA 1.0.6 fixes. Make sure you are on **1.0.6 or newer** (`Using library BLEOTA at version 1.0.6` in the compile log). If you are still on 1.0.5 or older, the `NimBLE-Arduino` library gets linked alongside the core's bundled NimBLE, and you get duplicate symbols.

If you have upgraded and the error persists, something else in your sketch or another library is pulling in `<NimBLEDevice.h>`. Search your project for that include and remove it, or migrate that code to the core's `<BLEDevice.h>`.
</details>

<details>
<summary><strong><code>'BLEOTAClass' does not name a type</code> on classic ESP32 after installing NimBLE-Arduino</strong></summary>

Fixed in 1.0.6. In earlier versions a `__has_include(<NimBLEDevice.h>)` check in `BLEOTA.h` silently disabled the header when `NimBLE-Arduino` was installed, even on a BlueDroid sketch. Upgrade to 1.0.6.
</details>

<details>
<summary><strong>One of the <code>examples/nimble/*</code> sketches doesn't compile on core 3.3.0+.</strong></summary>

That folder is only valid on **arduino-esp32 core < 3.3.0 with NimBLE-Arduino 2.x installed**. On core 3.3.0+ use the equivalent sketch in `examples/` instead — it uses `BLEOTA.h` with the core's `BLEDevice.h` and will pick NimBLE automatically on S3/C3/C6/H2 (and BlueDroid on classic ESP32). The over-the-air protocol is identical; the Web Bluetooth app doesn't care.
</details>

<details>
<summary><strong><code>'esp_ble_gatts_cb_param_t' does not name a type</code> when compiling my own sketch.</strong></summary>

Your `ServerCallbacks` uses the BlueDroid-only two-argument `onConnect(BLEServer*, esp_ble_gatts_cb_param_t*)` signature, and the target chip is running NimBLE (ESP32-S3/C3/C6/H2 with core 3.3.0+). Either:

- **Guard the BlueDroid overload** with `#if defined(CONFIG_BLUEDROID_ENABLED)` (see any sketch in `examples/`), or
- **Use only the portable form** `onConnect(BLEServer*)`.
</details>

<details>
<summary><strong>The WebApp does not see my device.</strong></summary>

Make sure `pAdvertising->addServiceUUID(BLEOTA.getBLEOTAuuid())` is called **before** `startAdvertising()`. The WebApp filters on the OTA service UUID — devices that advertise only their name will not match.
</details>

<details>
<summary><strong>I get <code>0x0005</code> (Can't start OTA) on the first sector.</strong></summary>

The ESP32 Updater refused `begin()`. Usual causes: image larger than the target OTA partition, a previous update still running (call `BLEOTA.abort()` to be safe), or insufficient free flash. Verify your partition table.
</details>

<details>
<summary><strong>I get <code>0x0003</code> (signature error) after the last sector.</strong></summary>

Secure mode is on, and either (a) the public key on the device does not match the private key used to sign, (b) the `.bin` was uploaded without the appended `signature.sign`, or (c) the file was compressed **after** signing. Re-read §9.3 — the order is *compress → sign → concat*.
</details>

<details>
<summary><strong>Update reaches 100% but the device never reboots.</strong></summary>

You are probably calling `BLEOTA.process(false)` somewhere — that disables the automatic reset. Either switch to `process()` or call `ESP.restart()` yourself from `afterStop()`.
</details>

<details>
<summary><strong>Sector NACK <code>0x0002</code> (index error) during transfer.</strong></summary>

The device expected a different `Sector_Index`. The `Expected_Idx` field (bytes 4–5 of the ACK) tells you where to rewind. The bundled WebApp retries up to 3 times per sector; custom clients should do the same.
</details>

<details>
<summary><strong>Safari / Firefox does not work.</strong></summary>

Neither implements the Web Bluetooth API. Use Chrome, Edge, or any Chromium-based browser. On iOS, third-party browser <strong>Bluefy</strong> works; on desktop Safari / Firefox there is no workaround.
</details>

<details>
<summary><strong>Chrome shows the Serial Number as <code>N/A</code>.</strong></summary>

Expected — `0x2A25` is on Chrome Web Bluetooth's GATT blocklist for anti-fingerprinting reasons. Native clients (nRF Connect, LightBlue) can still read it.
</details>

<details>
<summary><strong>Throughput on Android is much lower than on desktop.</strong></summary>

Android stacks default to a loose connection interval. On core 3.3.0+ call <code>BLEDevice::setPreferredConnectionParams(0x06, 0x12, 0, 2000)</code> before creating the server. On BlueDroid the example sketches already do it from `onConnect(BLEServer*, esp_ble_gatts_cb_param_t*)`.
</details>

<details>
<summary><strong>Can I run BLEOTA alongside other BLE services in my app?</strong></summary>

Yes. `BLEOTA` registers one extra GATT service (plus optional DIS). You keep ownership of the `BLEServer`, so you can register your own services on the same server either before or after `BLEOTA.init()`.
</details>

<details>
<summary><strong>Can I still use NimBLE-Arduino for other projects?</strong></summary>

Yes. On core 3.3.0+ BLEOTA has zero references to `NimBLE-Arduino`. You can keep it installed for other sketches without any interaction with BLEOTA. Be aware that on S3/C3/C6/H2 `NimBLE-Arduino` itself conflicts with the core's bundled NimBLE — but that's a NimBLE-Arduino ↔ core issue, independent of BLEOTA.
</details>

---

## 16. Upgrading from 1.0.5 to 1.0.6

For **most users, no code changes are required** — just update the library via Library Manager.

Two situations benefit from minor edits:

### You have NimBLE-Arduino installed and run on ESP32-S3/C3/C6/H2 with core 3.3.0+

Before 1.0.6 this combination gave link errors (`multiple definition of npl_freertos_*`) or silent header disable (`'BLEOTAClass' does not name a type`). In 1.0.6 it just works:

- If your sketch had `#include "NimBLEOTA.h"` and declared `NimBLEOTAClass BLEOTA;`, **swap to the portable form**:

  ```diff
  - #include "NimBLEOTA.h"
  - NimBLEOTAClass BLEOTA;
  + #include "BLEOTA.h"
  + BLEOTAClass BLEOTA;
  ```

- If your `ServerCallbacks` used `onConnect(BLEServer*, NimBLEConnInfo&)` or `onDisconnect(BLEServer*, NimBLEConnInfo&, int reason)`, switch to the portable single-argument form:

  ```diff
  - void onConnect(BLEServer* pServer, NimBLEConnInfo& connInfo) { … }
  - void onDisconnect(BLEServer* pServer, NimBLEConnInfo& connInfo, int reason) { … }
  + void onConnect(BLEServer* pServer) { … }
  + void onDisconnect(BLEServer* pServer) { … }
  ```

  Those types only exist in `NimBLE-Arduino`, which is no longer used on core 3.3.0+.

### You want tight connection parameters on S3/C3/C6/H2

Previously you might have been calling `pServer->updateConnParams(remote_bda, ...)` inside `onConnect`. That overload is BlueDroid-only. For a portable implementation, set preferred parameters at the device level instead — before `BLEDevice::createServer()`:

```cpp
BLEDevice::init("ESP32");
BLEDevice::setPreferredConnectionParams(0x06, 0x12, 0, 2000);
pServer = BLEDevice::createServer();
```

The three sketches under `examples/` show the pattern: a portable `onConnect(BLEServer*)` always compiles, plus a BlueDroid-only overload guarded by `#if defined(CONFIG_BLUEDROID_ENABLED)` that uses `updateConnParams(remote_bda, …)`.

### You are on a legacy core and everything works

Nothing to do — behaviour is unchanged from 1.0.5.

---

## 17. Compatibility matrix

| Component                    | Tested with                                                        |
| ---------------------------- | ------------------------------------------------------------------ |
| arduino-esp32 core           | 2.0.x ✅ • 3.0.x ✅ • 3.2.x ✅ • **3.3.x ✅ recommended** • 3.4.x ✅  |
| ESP32 variants               | ESP32 ✅, ESP32-S2 ✅, ESP32-S3 ✅, ESP32-C3 ✅, ESP32-C6 ✅, ESP32-H2 ✅ |
| `NimBLE-Arduino` (optional)  | 2.x tested on core ≤ 3.2 via `examples/nimble/*`                  |
| Chrome / Edge desktop        | ≥ v56 ✅ (Web Bluetooth enabled by default)                         |
| Chrome for Android           | ≥ v56 ✅                                                            |
| Bluefy (iOS)                 | tested, MTU typically ≤ 185                                        |
| Safari / Firefox             | ❌ (no Web Bluetooth)                                               |

---

## 18. License

Licensed under the **GNU Affero General Public License v3.0** — full text in [LICENSE](LICENSE).

In short: free to use, modify and redistribute (including over a network), **provided** you release your modifications under the same terms. If you need a different licensing arrangement, open an issue.

Bundled third-party components:

- [esp32-flashz](https://github.com/vortigont/esp32-flashz) — GPL-2.0 (compatible with §13 of AGPL-3.0).
- ESP32 in-ROM **miniz** — zlib license (permissive).
- `mbedtls` — Apache-2.0 (part of the arduino-esp32 core).

---

## Credits

- [espressif/ble_ota](https://components.espressif.com/components/espressif/ble_ota) — original protocol design.
- [vortigont/esp32-flashz](https://github.com/vortigont/esp32-flashz) — on-device zlib decompression.
- [@drik](https://github.com/drik) — user callbacks mechanism.

Contributions welcome — open an issue or a PR on [github.com/gb88/BLEOTA](https://github.com/gb88/BLEOTA).
