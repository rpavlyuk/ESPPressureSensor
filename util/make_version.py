#!/usr/bin/env python3
"""
Script to generate version information for ESPRelayBoard project
Usage: python make_version.py <source_folder_root_path> <major_version>
"""

import sys
import os
from datetime import datetime

def get_idf_target(sdkconfig_path: str) -> str:
    if not os.path.exists(sdkconfig_path):
        raise SystemExit(f"ERROR: sdkconfig not found: {sdkconfig_path}")

    with open(sdkconfig_path, "r") as f:
        for line in f:
            line = line.strip()
            if line.startswith("CONFIG_IDF_TARGET="):
                # Remove key and quotes
                value = line.split("=", 1)[1].strip()
                return value.strip('"')

    raise SystemExit("ERROR: CONFIG_IDF_TARGET not found in sdkconfig")

if len(sys.argv) != 3:
    print(f"Usage: {sys.argv[0]} <source_folder_root_path> <major_version>")
    sys.exit(1)

SOURCE_FOLDER_ROOT_PATH = sys.argv[1]
MAJOR_VERSION = sys.argv[2]

VERSION_H_FILE = os.path.join(SOURCE_FOLDER_ROOT_PATH, 'main', 'version.h')
SDKCONFIG_FILE = os.path.join(SOURCE_FOLDER_ROOT_PATH, "sdkconfig")

# Generate the build number based on the current date and time
DEVICE_SW_BUILD_NUM = datetime.now().strftime("%Y%m%d%H%M%S")
DEVICE_SW_VERSION_NUM = MAJOR_VERSION
DEVICE_SW_VERSION = f'{DEVICE_SW_VERSION_NUM} build {DEVICE_SW_BUILD_NUM}'
IDF_TARGET = get_idf_target(SDKCONFIG_FILE)

# Create the content for version.h
version_content = f"""#ifndef VERSION_H
#define VERSION_H

#define DEVICE_SW_BUILD_NUM "{DEVICE_SW_BUILD_NUM}"
#define DEVICE_SW_VERSION_NUM "{DEVICE_SW_VERSION_NUM}"
#define DEVICE_SW_VERSION DEVICE_SW_VERSION_NUM " build " DEVICE_SW_BUILD_NUM

#endif // VERSION_H
"""

# Write the content to version.h
os.makedirs(os.path.dirname(VERSION_H_FILE), exist_ok=True)
with open(VERSION_H_FILE, 'w') as file:
    file.write(version_content)

print(f"Version header file generated at {VERSION_H_FILE}")

# Write content to build_info.json in ./build/ directory
BUILD_INFO_FILE = os.path.join(SOURCE_FOLDER_ROOT_PATH, 'build', 'build_info.json')
os.makedirs(os.path.dirname(BUILD_INFO_FILE), exist_ok=True)
build_info_content = f"""{{
    "DEVICE_SW_BUILD_NUM": "{DEVICE_SW_BUILD_NUM}",
    "DEVICE_SW_VERSION_NUM": "{DEVICE_SW_VERSION_NUM}",
    "DEVICE_SW_VERSION": "{DEVICE_SW_VERSION}",
    "IDF_TARGET": "{IDF_TARGET}"
}}
"""
with open(BUILD_INFO_FILE, 'w') as file:
    file.write(build_info_content)

print(f"Build info file generated at {BUILD_INFO_FILE}")