# Changelog

## 1.0.6 — 2026-04-21

BLEOTA 1.0.6 automatically picks the right BLE backend depending on the
installed arduino-esp32 core version:

- **core ≥ 3.3.0** — uses only the core's `BLEDevice.h`, which on
  classic ESP32 wraps BlueDroid and on ESP32-S3/C3/C6/H2 wraps the
  NimBLE host bundled in `libbt.a`. Zero dependency on `NimBLE-Arduino`
  on modern cores; the library can coexist with `NimBLE-Arduino`
  installed for unrelated projects.
- **core < 3.3.0** — preserves the v1.0.x behaviour: if
  `NimBLE-Arduino` is installed it is used (NimBLE backend), otherwise
  BlueDroid is used.

### Fixed
- No more **`multiple definition of npl_freertos_*`** link errors on
  ESP32-S3/C3/C6/H2 with arduino-esp32 core 3.3.0+ when
  `NimBLE-Arduino` is installed alongside BLEOTA. The library now
  skips all references to `<NimBLEDevice.h>` on modern cores.
- `BLEOTA.h` no longer silently disables itself when
  `NimBLE-Arduino` is detected via `__has_include` on a classic ESP32
  BlueDroid sketch — fixes the "`'BLEOTAClass' does not name a type`"
  regression on mixed installations.

### Changed
- Added `BLECharacteristic::PROPERTY_WRITE_NR` to both OTA
  characteristics on the BlueDroid/modern-core path. The Web
  Bluetooth client uses `writeValueWithoutResponse` for maximum
  throughput; the NimBLE-Arduino legacy backend already had this
  property.
- The three BlueDroid examples in `examples/` now compile on every
  chip (including ESP32-S3/C3/C6/H2 with core 3.3.0+). The
  BlueDroid-specific `onConnect(BLEServer*, esp_ble_gatts_cb_param_t*)`
  overload is now guarded with `#if defined(CONFIG_BLUEDROID_ENABLED)`
  and a portable `onConnect(BLEServer*)` is always provided as the
  base.

### NimBLE-Arduino examples

The three sketches under `examples/nimble/` use the NimBLE-Arduino
API directly (`NimBLEOTAClass`, `NimBLEConnInfo`,
`NIMBLE_PROPERTY::*`, NimBLE-specific advertising helpers). They are
kept for backwards compatibility and **require arduino-esp32 core
< 3.3.0 with NimBLE-Arduino 2.x installed**. On core 3.3.0+ these
sketches no longer compile because:

1. On ESP32-S3/C3/C6/H2 `NimBLE-Arduino` cannot be linked at the same
   time as the core's bundled NimBLE.
2. On classic ESP32 `NimBLE-Arduino` still works, but BLEOTA 1.0.6
   keeps the header on the modern-core path disabled so users don't
   need to install the extra library.

Users on core 3.3.0+ should use the sketches in `examples/` (the
`BLEOTAClass` API), which already yield the best available stack
(BlueDroid on classic ESP32, NimBLE on other variants) via the core's
wrapper.

## 1.0.5 and earlier
Initial two-backend architecture: separate `BLEOTAClass` (BlueDroid)
and `NimBLEOTAClass` (NimBLE-Arduino). See the git log for details.
