#!/bin/sh

#  install-swiftmodules.sh
#  Overlays
#
#  Copyright © 2019 Apple. All rights reserved.

set -e
#set -xv

# This only needs to run during installation, but that includes "installapi".
[ "$ACTION" = "installapi" -o "$ACTION" = "install" ] || exit 0

[ "$SKIP_INSTALL" != "YES" ] || exit 0
[ "$SWIFT_INSTALL_MODULES" = "YES" ] || exit 0

srcmodule="${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}.swiftmodule"
dstpath="${INSTALL_ROOT}/${INSTALL_PATH}/"

if [ ! -d "$srcmodule" ]; then
    echo "Cannot find Swift module at $srcmodule" >&2
    exit 1
fi

mkdir -p "$dstpath"
cp -r "$srcmodule" "$dstpath"
