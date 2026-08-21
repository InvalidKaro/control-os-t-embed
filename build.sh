#!/usr/bin/env bash
set -euo pipefail
python3 scripts/release_check.py
pio run -e t_embed_cc1101_plus
python3 scripts/verify_build_outputs.py
python3 scripts/prepare_m5launcher.py
printf '\nRelease outputs:\n'
ls -lh firmware.bin control-os-full.bin ControlOS-TEmbed-CC1101-Plus-M5Launcher.bin
