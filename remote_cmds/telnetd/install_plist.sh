#!/bin/sh
set -e
set -x

copy_plist() {
    mkdir -p "${DSTROOT}/System/Library/LaunchDaemons"
    cp "${SCRIPT_INPUT_FILE_0}" "${DSTROOT}/System/Library/LaunchDaemons"
}

case "$PLATFORM_NAME" in
iphone*|appletv*|watch*|bridge*)
    copy_plist
    ;;
macosx)
    ;;
*)
    case "$FALLBACK_PLATFORM_NAME" in
        iphone*|appletv*|watch*|bridge*)
            copy_plist
            ;;
        *)
            echo "Unsupported platform: $PLATFORM_NAME"
            exit 1
            ;;
    esac
    ;;
esac
