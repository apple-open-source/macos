#!/bin/sh
script="${SYSTEM_DEVELOPER_DIR}/ProjectBuilder Extras/Kernel Extension Support/KEXTPostprocess";
if [ -x "$script" ]; then
    . "$script"
fi
