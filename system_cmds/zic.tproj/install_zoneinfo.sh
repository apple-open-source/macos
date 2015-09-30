#!/bin/sh
set -e
set -x

case "$PLATFORM_NAME" in
iphone*|appletv*|watch*)
    ditto "${BUILT_PRODUCTS_DIR}/zoneinfo" "${DSTROOT}/usr/share/zoneinfo.default"
    ln -hfs "/var/db/timezone/zoneinfo" "${DSTROOT}/usr/share/zoneinfo"
    ;;
macosx)
    ditto "${BUILT_PRODUCTS_DIR}/zoneinfo" "${DSTROOT}/usr/share/zoneinfo"
    ;;
*)
    echo "Unsupported platform: $PLATFORM_NAME"
    exit 1
    ;;
esac
