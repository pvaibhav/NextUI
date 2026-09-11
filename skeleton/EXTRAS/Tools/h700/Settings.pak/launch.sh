#!/bin/sh

SDCARD_PATH="${SDCARD_PATH:-/mnt/SDCARD}"
USERDATA_PATH="${USERDATA_PATH:-$SDCARD_PATH/.userdata/h700}"
SHARED_USERDATA_PATH="${SHARED_USERDATA_PATH:-$SDCARD_PATH/.userdata/shared}"

cd $(dirname "$0")
./settings.elf > settings.log 2>&1
