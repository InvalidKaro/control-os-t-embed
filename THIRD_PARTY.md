# Third-party and reference notes

## Bruce firmware

Bruce firmware by BruceDevices is licensed under AGPL-3.0. Control OS uses Bruce as a technical and UX/functionality reference, especially for T-Embed board support, rotary-input robustness, shared-peripheral handling and the general multifunction-device feature set.

Project: https://github.com/BruceDevices/firmware

No offensive Bruce modules are included in this release candidate. In particular, Wi-Fi deauthentication, credential collection, jamming, brute-force RF transmission, BLE spam, reverse-shell functionality and automated attack workflows are excluded.

## LILYGO T-Embed CC1101

Board pin mapping and peripheral configuration are based on the official LILYGO T-Embed CC1101 repository and documentation.

Project: https://github.com/Xinyuan-LilyGO/T-Embed-CC1101

## Libraries

- FastLED
- TFT_eSPI
- RadioLib
- Adafruit PN532
- IRremoteESP8266
- ESP32 Arduino core libraries (WiFi, BLE, SD, Preferences, Wire, SPI)

Each library remains subject to its own license.
