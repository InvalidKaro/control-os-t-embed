#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

REQUIRED_FILES = (
    "platformio.ini",
    "partitions.csv",
    "boards/t_embed_cc1101_plus.json",
    "include/BoardPins.h",
    "include/Config.h",
    "include/Input.h",
    "include/Display.h",
    "include/LedEngine.h",
    "include/Modules.h",
    "include/Settings.h",
    "include/Application.h",
    "src/main.cpp",
    "src/Input.cpp",
    "src/Display.cpp",
    "src/LedEngine.cpp",
    "src/Modules.cpp",
    "src/Settings.cpp",
    "src/Application.cpp",
)

REQUIRED_BUILD_FLAGS = (
    "-DFASTLED_RMT_BUILTIN_DRIVER=1",
    "-DFASTLED_RMT_MAX_CHANNELS=1",
    "-DTFT_SPI_MODE=SPI_MODE0",
    "-DUSE_HSPI_PORT=1",
    "-DSUPPORT_TRANSACTIONS=1",
    "-DSPI_FREQUENCY=40000000",
)

FORBIDDEN_SOURCE_PATTERNS = (
    r"\bTODO\b",
    r"\bFIXME\b",
    r"esp_wifi_80211_tx",
    r"sendDeauth",
    r"deauth(Frame|Packet|Task)",
    r"credential.{0,12}(steal|harvest|collect)",
    r"reverseShell\s*\(",
    r"jammer(Task|Start|Run|Loop)",
)


def parse_int(value: str) -> int:
    value = value.strip()
    return int(value, 16) if value.lower().startswith("0x") else int(value)


def check_required_files(errors: list[str]) -> None:
    for relative in REQUIRED_FILES:
        if not (ROOT / relative).is_file():
            errors.append(f"missing required file: {relative}")


def check_board(errors: list[str]) -> None:
    board_path = ROOT / "boards/t_embed_cc1101_plus.json"
    try:
        board = json.loads(board_path.read_text(encoding="utf-8"))
    except Exception as exc:
        errors.append(f"invalid board JSON: {exc}")
        return

    build = board.get("build", {})
    arduino = build.get("arduino", {})
    upload = board.get("upload", {})
    flags = set(build.get("extra_flags", []))

    if build.get("mcu") != "esp32s3":
        errors.append("board MCU must be esp32s3")
    if arduino.get("memory_type") != "qio_opi":
        errors.append("board memory_type must be qio_opi")
    if upload.get("flash_size") != "16MB":
        errors.append("board flash_size must be 16MB")
    if "-DBOARD_HAS_PSRAM" not in flags:
        errors.append("BOARD_HAS_PSRAM is missing")


def check_platformio(errors: list[str]) -> None:
    text = (ROOT / "platformio.ini").read_text(encoding="utf-8")
    for flag in REQUIRED_BUILD_FLAGS:
        if flag not in text:
            errors.append(f"missing build flag: {flag}")
    if "espressif32@6.13.0" not in text:
        errors.append("platform must be pinned to espressif32@6.13.0")
    if "board = t_embed_cc1101_plus" not in text:
        errors.append("custom T-Embed board is not selected")


def check_partitions(errors: list[str]) -> None:
    rows: list[tuple[str, int, int]] = []
    with (ROOT / "partitions.csv").open(encoding="utf-8", newline="") as handle:
        for raw in csv.reader(handle):
            if not raw or raw[0].lstrip().startswith("#"):
                continue
            if len(raw) < 5:
                errors.append(f"invalid partition row: {raw}")
                continue
            name = raw[0].strip()
            try:
                offset = parse_int(raw[3])
                size = parse_int(raw[4])
            except ValueError as exc:
                errors.append(f"invalid partition number in {name}: {exc}")
                continue
            rows.append((name, offset, size))

    rows.sort(key=lambda item: item[1])
    flash_end = 16 * 1024 * 1024
    previous_end = 0x9000
    for name, offset, size in rows:
        if offset < previous_end:
            errors.append(f"partition overlap near {name}")
        previous_end = offset + size
        if previous_end > flash_end:
            errors.append(f"partition {name} exceeds 16MB flash")
    if rows and rows[-1][1] + rows[-1][2] != flash_end:
        errors.append("partition layout does not end exactly at 16MB")


def check_sources(errors: list[str]) -> None:
    source_files = list((ROOT / "src").glob("*.cpp")) + list((ROOT / "include").glob("*.h"))
    for path in source_files:
        text = path.read_text(encoding="utf-8", errors="replace")
        for pattern in FORBIDDEN_SOURCE_PATTERNS:
            if re.search(pattern, text, flags=re.IGNORECASE | re.DOTALL):
                errors.append(f"forbidden/unfinished source pattern {pattern!r} in {path.relative_to(ROOT)}")
        if re.search(r"\bdelay\s*\(", text):
            errors.append(f"blocking delay() found in {path.relative_to(ROOT)}")


def main() -> int:
    errors: list[str] = []
    check_required_files(errors)
    if errors:
        for error in errors:
            print(f"FAIL: {error}")
        return 1

    check_board(errors)
    check_platformio(errors)
    check_partitions(errors)
    check_sources(errors)

    if errors:
        for error in errors:
            print(f"FAIL: {error}")
        return 1

    print("Release checks: PASS")
    print("- required files present")
    print("- ESP32-S3 16MB/qio_opi/PSRAM board definition valid")
    print("- required TFT/FastLED build flags present")
    print("- partition map fits exactly in 16MB")
    print("- no TODO/FIXME/blocking delay()/excluded offensive APIs found in source")
    return 0


if __name__ == "__main__":
    sys.exit(main())
