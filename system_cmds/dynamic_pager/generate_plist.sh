#!/bin/sh
set -e
set -x

cp "${SCRIPT_INPUT_FILE_0}" "${SCRIPT_OUTPUT_FILE_0}"

add_LaunchOnlyOnce_embedded() {
  /usr/libexec/PlistBuddy -c "Add :LaunchOnlyOnce bool true" "${SCRIPT_OUTPUT_FILE_0}"
}

case "$PLATFORM_NAME" in
iphone*|appletv*|watch*|bridge*)
    add_LaunchOnlyOnce_embedded
    ;;
macosx)
    ;;
*)
    case "$FALLBACK_PLATFORM" in
    iphone*|appletv*|watch*|bridge*)
        add_LaunchOnlyOnce_embedded
        ;;
    *)
        echo "Unsupported platform: $ACTUAL_PLATFORM_NAME"
        exit 1
        ;;
    esac
    ;;
esac
