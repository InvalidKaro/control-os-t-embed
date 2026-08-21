$ErrorActionPreference = "Stop"
python scripts/release_check.py
pio run -e t_embed_cc1101_plus
python scripts/verify_build_outputs.py
python scripts/prepare_m5launcher.py
Get-Item firmware.bin, control-os-full.bin, ControlOS-TEmbed-CC1101-Plus-M5Launcher.bin | Format-Table Name,Length
