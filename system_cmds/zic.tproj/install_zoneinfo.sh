#!/bin/sh
set -e
set -x

if [ -n "$RC_BRIDGE" ]; then
    ACTUAL_PLATFORM_NAME="bridgeos"
else
    ACTUAL_PLATFORM_NAME="${PLATFORM_NAME}"
fi

case "$ACTUAL_PLATFORM_NAME" in
iphone*|appletv*|watch*|macosx|bridge*)
    ditto "${BUILT_PRODUCTS_DIR}/zoneinfo" "${DSTROOT}/usr/share/zoneinfo.default"
    ln -hfs "/var/db/timezone/zoneinfo" "${DSTROOT}/usr/share/zoneinfo"
    case "$ACTUAL_PLATFORM_NAME" in
    macosx|bridge*)
        mkdir -p "${DSTROOT}/private/var/db/timezone"
        chmod 555 "${DSTROOT}/private/var/db/timezone"
        ln -hfs "/usr/share/zoneinfo.default" "${DSTROOT}/private/var/db/timezone/zoneinfo"
        ;;
    esac
    ;;
*)
    echo "Unsupported platform: $ACTUAL_PLATFORM_NAME"
    exit 1
    ;;
esac
