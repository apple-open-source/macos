#!/bin/sh
mkdir -p "${DERIVED_SOURCES_DIR}/IOKit" && ln -sf "${SRCROOT}/IOSerialFamily.kmodproj" "${DERIVED_SOURCES_DIR}/IOKit/serial";
script="${SYSTEM_DEVELOPER_DIR}/ProjectBuilder Extras/Kernel Extension Support/KEXTPreprocess";
if [ -x "$script" ]; then
    . "$script"
fi
