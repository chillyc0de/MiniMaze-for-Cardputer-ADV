Import("env")
import os

def merge_binaries(target, source, env):
    build_dir = env.subst("$BUILD_DIR")
    fw_name = env.subst("$PROGNAME")
    
    bootloader = f'"{os.path.join(build_dir, "bootloader.bin")}"'
    partitions = f'"{os.path.join(build_dir, "partitions.bin")}"'
    app = f'"{os.path.join(build_dir, fw_name + ".bin")}"'
    merged = f'"{os.path.join(build_dir, "firmware_merged.bin")}"'

    esptool_path = f'"{os.path.join(env.PioPlatform().get_package_dir("tool-esptoolpy") or "", "esptool.py")}"'

    cmd = [
        '"$PYTHONEXE"', 
        esptool_path,
        "--chip", "esp32s3",
        "merge_bin",
        "-o", merged,
        "--flash_mode", "dio",
        "--flash_size", "8MB",
        "0x0000", bootloader,
        "0x8000", partitions,
        "0x10000", app
    ]
    
    print("\n[MERGER] Attempting to create merged binary...")
    env.Execute(" ".join(cmd))
    
    merged_raw = os.path.join(build_dir, "firmware_merged.bin")
    if os.path.exists(merged_raw):
        print(f"[MERGER] SUCCESS! File created: {merged_raw}\n")
    else:
        print("[MERGER] FAILED to create merged binary.\n")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_binaries)