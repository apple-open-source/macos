#!/usr/bin/env python3
import sys
import subprocess

# get the strings XNU build-folder strings for the given device
def main():
    sdkroot = sys.argv[1]
    target_name = sys.argv[2]  # e.g. j414c
    query = f"SELECT DISTINCT KernelMachOArchitecture, KernelPlatform, SDKPlatform FROM Targets WHERE TargetType == '{target_name}'"
    r = subprocess.check_output(["xcrun", "--sdk", sdkroot, "embedded_device_map", "-query", query], encoding="ascii")
    r = r.strip()
    if len(r) == 0:
        raise Exception(f"target not found {target_name}")
    arch, kernel_platform, sdk_platform = r.split("|")

    if arch.startswith("arm64"):  # can be arm64, arm64e
        arch = "ARM64"
    elif arch.startswith("arm"):
        arch = "ARM"
    else:
        raise Exception(f"unsupported arch {arch}")

    if sdk_platform == "macosx":
        file_name_prefix = "kernel"
    else:
        file_name_prefix = "mach"
    print(arch + " " + kernel_platform + " " + file_name_prefix)

if __name__ == "__main__":
    sys.exit(main())
