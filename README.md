# Control OS for LILYGO T-Embed CC1101 Plus

Standalone ESP32-S3 firmware for the LILYGO T-Embed CC1101 Plus. The project uses Bruce and the official LILYGO firmware as technical references, but has its own UI, input model and LED-wheel implementation. Release candidate: `0.9.0-rc2`.

`docs/ui-concept-preview.png` is a generated design concept, not a photograph or firmware screenshot.

## Implemented release-candidate features

- 320x170 ST7789 UI with seven themes
- Rotary encoder with dedicated 2 ms quadrature sampling task
- Proper click / long-press / release handling
- 8-pixel WS2812 control wheel with 25 selectable base modes and reactive overlays
- CC1101 RSSI analyzer on 315 / 433.92 / 868 / 915 MHz
- CC1101 configuration uses a RadioLib-valid 135 kHz RX bandwidth
- CC1101 packet receive view
- Explicit, manual CC1101 test-packet transmission only; requires encoder long-press
- IR protocol/value monitor
- Manual NEC test transmission
- PN532 ISO14443A UID reader
- Passive Wi-Fi network scan
- Passive BLE advertisement scan
- SD root browser
- Passive nRF24 2.4 GHz energy/RPD scan
- BQ27220 battery voltage / state-of-charge readout
- BQ25896 charging-state readout
- NVS/Preferences settings
- System information page
- Deep sleep with BACK-key wake
- ST7789 uses 40 MHz, SPI mode 0, HSPI and transaction support for shared-bus stability
- Automatic merged `control-os-full.bin` generation after a successful PlatformIO build

## Intentionally excluded

The firmware does not implement Wi-Fi deauthentication, jamming, credential theft, brute-force transmission, disruptive nRF24 carrier modes, automated replay attacks, reverse shells, captive-portal credential collection, or similar offensive functions.

## Build

Requirements:

- Python 3
- PlatformIO Core or PlatformIO IDE
- USB data connection to the T-Embed CC1101 Plus

Recommended reproducible build on Linux/macOS:

```bash
./build.sh
```

On Windows PowerShell:

```powershell
./build.ps1
```

Or manually:

```bash
python3 scripts/release_check.py
pio run -e t_embed_cc1101_plus -t clean
pio run -e t_embed_cc1101_plus
```

After a successful build, the post-build script creates:

- `.pio/build/t_embed_cc1101_plus/firmware.bin` — application image
- `firmware.bin` — copy of the application image
- `control-os-full.bin` — merged bootloader + partitions + boot_app0 + application image

Upload with PlatformIO:

```bash
pio run -e t_embed_cc1101_plus -t upload
```

Monitor:

```bash
pio device monitor -b 115200
```

Flash the merged binary directly:

```bash
python -m esptool --chip esp32s3 --port YOUR_PORT write_flash 0x0 control-os-full.bin
```

## First hardware smoke test

1. Confirm serial log reports approximately 16 MB flash and 8 MB PSRAM.
2. Confirm the screen is 320x170 landscape with no corruption.
3. Rotate the wheel slowly: one physical detent should produce one menu movement.
4. Rotate quickly and check that input does not stall while the display redraws.
5. Short press: one click only. Long press: no extra click on release.
6. Verify all eight RGB LEDs and change effects in LED Studio.
7. Open System Info and verify CC1101 / PN532 / SD / nRF24 presence.
8. Run Wi-Fi and BLE scans and verify the device remains responsive afterward.
9. Open Sub-GHz Analyzer on all four bands. TX is manual test-packet only and requires a long press. Observe applicable local RF rules before transmitting.
10. If SD is not detected, first test a FAT32 SanDisk card of 32 GB or less; LILYGO documents compatibility issues with some cards.
11. Let the firmware run for several minutes and check for resets or Guru Meditation errors.

## Shared SPI

The display, SD card, CC1101 and onboard nRF24 share SCK 11 / MOSI 9 / MISO 10. They are routed through the ESP32-S3 HSPI host with transaction support. The firmware keeps all chip-select lines high before selecting a peripheral. Display rendering and radio/storage actions stay on the Arduino loop task; the high-priority encoder task only reads GPIO and never touches SPI.

## License / attribution

This project is distributed under AGPL-3.0-or-later to keep reuse straightforward when functionality or implementation ideas are adapted from Bruce. See `THIRD_PARTY.md` for attribution.


## Build validation note

See `BUILD_STATUS.md`. The generated source bundle passed static release checks, but the generation runtime did not contain PlatformIO/ESP32-S3 compiler packages, so the included source bundle does not contain a falsely claimed prebuilt binary.
