#!/bin/sh
set -e
set -x

if [ -n "$RC_BRIDGE" ]; then
    ACTUAL_PLATFORM_NAME="bridge${PLATFORM_NAME#watch}"
else
    ACTUAL_PLATFORM_NAME="${PLATFORM_NAME}"
fi

case "$ACTUAL_PLATFORM_NAME" in
iphone*|appletv*|watch*)
    ditto "${BUILT_PRODUCTS_DIR}/zoneinfo" "${DSTROOT}/usr/share/zoneinfo.default"
    ln -hfs "/var/db/timezone/zoneinfo" "${DSTROOT}/usr/share/zoneinfo"
    ;;
macosx|bridge*)
    ditto "${BUILT_PRODUCTS_DIR}/zoneinfo" "${DSTROOT}/usr/share/zoneinfo"
    ;;
*)
    echo "Unsupported platform: $ACTUAL_PLATFORM_NAME"
    exit 1
    ;;
esac
