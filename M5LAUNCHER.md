# M5Launcher / Launcher

For Launcher, use the **application image**, not the merged full-flash image:

`ControlOS-TEmbed-CC1101-Plus-M5Launcher.bin`

It is copied directly from PlatformIO's:

`.pio/build/t_embed_cc1101_plus/firmware.bin`

The preparation script checks that byte 0 is the ESP32 application-image magic `0xE9` before publishing the file.

Do **not** install `control-os-full.bin` from Launcher. That file contains bootloader, partition table, OTA data and application and is intended for a direct full flash at offset `0x0`.

## Local build

```bash
pio run -e t_embed_cc1101_plus
python scripts/prepare_m5launcher.py
```

## Install from Launcher

Copy `ControlOS-TEmbed-CC1101-Plus-M5Launcher.bin` to a FAT32 SD card or upload it through Launcher's WebUI, then select it and choose **Install**.

If Launcher reports that the application does not fit, select a larger application partition scheme in Launcher first.
