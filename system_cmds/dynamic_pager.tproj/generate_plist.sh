#!/bin/sh
set -e
set -x

cp "${SCRIPT_INPUT_FILE_0}" "${SCRIPT_OUTPUT_FILE_0}"
case "$PLATFORM_NAME" in
iphone*|appletv*|watch*|bridge*)
    /usr/libexec/PlistBuddy -c "Add :LaunchOnlyOnce bool true" "${SCRIPT_OUTPUT_FILE_0}"
    ;;
macosx)
    ;;
*)
    echo "Unsupported platform: $PLATFORM_NAME"
    exit 1
    ;;
esac
