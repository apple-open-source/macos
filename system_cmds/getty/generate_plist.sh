#!/bin/sh
set -e
set -x

cp "${SCRIPT_INPUT_FILE_0}" "${SCRIPT_OUTPUT_FILE_0}"
case "$PLATFORM_NAME" in
iphone*|appletv*|watch*|bridge*)
    ;;
macosx)
    /usr/libexec/PlistBuddy -c "Add :Disabled bool true" "${SCRIPT_OUTPUT_FILE_0}"
    ;;
*)
    case "$FALLBACK_PLATFORM" in
        iphone*|appletv*|watch*|bridge*)
            ;;
        *)
            echo "Unsupported platform: $ACTUAL_PLATFORM_NAME"
            exit 1
            ;;
    esac
    ;;
esac
