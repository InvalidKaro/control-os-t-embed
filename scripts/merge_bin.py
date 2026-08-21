Import("env")

import os
import shutil
import subprocess


def merge_firmware(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    project_dir = env.subst("$PROJECT_DIR")
    framework_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
    esptool_dir = env.PioPlatform().get_package_dir("tool-esptoolpy")

    firmware = os.path.join(build_dir, "firmware.bin")
    bootloader = os.path.join(build_dir, "bootloader.bin")
    partitions = os.path.join(build_dir, "partitions.bin")
    boot_app0 = os.path.join(framework_dir, "tools", "partitions", "boot_app0.bin")
    app_copy = os.path.join(project_dir, "firmware.bin")
    full_image = os.path.join(project_dir, "control-os-full.bin")

    required = [firmware, bootloader, partitions, boot_app0]
    missing = [path for path in required if not os.path.exists(path)]
    if missing:
        print("[merge_bin] Missing build artifacts; merged image not created:")
        for path in missing:
            print(f"  - {path}")
        return

    python = env.subst("$PYTHONEXE")
    esptool_script = os.path.join(esptool_dir, "esptool.py") if esptool_dir else ""

    if esptool_script and os.path.exists(esptool_script):
        command = [python, esptool_script]
    else:
        command = [python, "-m", "esptool"]

    command.extend(
        [
            "--chip",
            "esp32s3",
            "merge_bin",
            "-o",
            full_image,
            "0x0",
            bootloader,
            "0x8000",
            partitions,
            "0xE000",
            boot_app0,
            "0x10000",
            firmware,
        ]
    )

    print("[merge_bin] Creating single flash image...")
    subprocess.check_call(command)
    shutil.copyfile(firmware, app_copy)

    print(f"[merge_bin] Application image: {app_copy}")
    print(f"[merge_bin] Full flash image: {full_image}")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_firmware)
