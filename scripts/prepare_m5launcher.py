#!/usr/bin/env python3
from pathlib import Path
import hashlib
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]
ENV = "t_embed_cc1101_plus"
SRC = ROOT / ".pio" / "build" / ENV / "firmware.bin"
OUT = ROOT / "ControlOS-TEmbed-CC1101-Plus-M5Launcher.bin"


def fail(msg: str) -> None:
    print(f"ERROR: {msg}", file=sys.stderr)
    raise SystemExit(1)


if not SRC.exists():
    fail(f"missing PlatformIO app binary: {SRC}")

data = SRC.read_bytes()
if not data:
    fail("firmware.bin is empty")
if data[0] != 0xE9:
    fail(f"unexpected ESP32 app image magic: 0x{data[0]:02X} (expected 0xE9)")

shutil.copyfile(SRC, OUT)
sha = hashlib.sha256(data).hexdigest()
(ROOT / "ControlOS-TEmbed-CC1101-Plus-M5Launcher.bin.sha256").write_text(
    f"{sha}  {OUT.name}\n", encoding="utf-8"
)
print(f"M5Launcher app binary: {OUT}")
print(f"Size: {len(data)} bytes")
print(f"SHA256: {sha}")
