#!/bin/sh

if [ "${PLATFORM_NAME}" = "iphoneos" ]; then
ditto "${BUILT_PRODUCTS_DIR}/zoneinfo" "${DSTROOT}/usr/share/zoneinfo.default"
ln -hfs "/var/db/timezone/zoneinfo" "${DSTROOT}/usr/share/zoneinfo"
else
ditto "${BUILT_PRODUCTS_DIR}/zoneinfo" "${DSTROOT}/usr/share/zoneinfo"
fi
