# Build status

## Release candidate

Version: `0.9.0-rc2`

Local static release validation in the generation environment: **PASS**.

Validated without the ESP32 toolchain:

- all required project files are present
- board JSON parses correctly and declares ESP32-S3, 16 MB flash, qio_opi and PSRAM
- partition map is non-overlapping and ends exactly at 16 MB
- required FastLED RMT, ST7789 mode-0, HSPI and SPI transaction flags are present
- all project `.cpp` files pass a C++17 syntax/type pass against Arduino/TFT/FreeRTOS API stubs
- no TODO/FIXME or blocking `delay()` exists in project source
- no excluded offensive Wi-Fi/RF APIs are implemented
- post-build merge script passes Python syntax compilation

## Real PlatformIO build

A real ESP32-S3 binary was **not** produced in the generation environment because PlatformIO Core and the Xtensa ESP32-S3 toolchain are not installed there, and that runtime cannot resolve/download PlatformIO packages.

Run either `./build.sh` or `build.ps1` on a machine with PlatformIO. A successful build generates:

- `firmware.bin` — application image
- `control-os-full.bin` — merged image for flashing at address `0x0`

Do not treat any file as a tested release binary until the real PlatformIO build and hardware smoke test both pass.

## CI build path

The repository now includes `.github/workflows/build-firmware.yml`.
On GitHub it installs PlatformIO 6.1.18, runs source checks, builds the
`t_embed_cc1101_plus` environment, verifies both ESP images, and uploads:

- `firmware.bin`
- `control-os-full.bin`
- `SHA256SUMS.txt`

The full image is verified to contain an ESP bootloader at `0x0` and the
application image at `0x10000` before it is uploaded.
