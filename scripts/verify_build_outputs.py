#!/usr/bin/env python3
from __future__ import annotations

from hashlib import sha256
from pathlib import Path
import sys

APP = Path("firmware.bin")
FULL = Path("control-os-full.bin")
FLASH_SIZE = 16 * 1024 * 1024
APP_OFFSET = 0x10000
ESP_IMAGE_MAGIC = 0xE9


def fail(message: str) -> None:
    print(f"VERIFY ERROR: {message}", file=sys.stderr)
    raise SystemExit(1)


def digest(path: Path) -> str:
    return sha256(path.read_bytes()).hexdigest()


def main() -> None:
    if not APP.is_file():
        fail("firmware.bin is missing")
    if not FULL.is_file():
        fail("control-os-full.bin is missing")

    app_size = APP.stat().st_size
    full_size = FULL.stat().st_size

    if app_size < 1024:
        fail(f"firmware.bin is unexpectedly small: {app_size} bytes")
    if full_size <= APP_OFFSET + app_size:
        fail(
            "control-os-full.bin does not contain the complete application "
            f"at offset 0x{APP_OFFSET:X}"
        )
    if full_size > FLASH_SIZE:
        fail(f"control-os-full.bin exceeds 16 MB flash: {full_size} bytes")

    app = APP.read_bytes()
    full = FULL.read_bytes()

    if app[0] != ESP_IMAGE_MAGIC:
        fail(
            "firmware.bin does not start with ESP image magic 0xE9 "
            f"(got 0x{app[0]:02X})"
        )
    if full[0] != ESP_IMAGE_MAGIC:
        fail(
            "control-os-full.bin does not contain an ESP bootloader at 0x0 "
            f"(got 0x{full[0]:02X})"
        )
    if full[APP_OFFSET] != ESP_IMAGE_MAGIC:
        fail(
            "control-os-full.bin does not contain an ESP app image at "
            f"0x{APP_OFFSET:X} (got 0x{full[APP_OFFSET]:02X})"
        )

    merged_app = full[APP_OFFSET : APP_OFFSET + app_size]
    if merged_app != app:
        fail("application payload in control-os-full.bin differs from firmware.bin")

    print("Build output verification: PASS")
    print(f"firmware.bin: {app_size} bytes SHA256={digest(APP)}")
    print(f"control-os-full.bin: {full_size} bytes SHA256={digest(FULL)}")


if __name__ == "__main__":
    main()
