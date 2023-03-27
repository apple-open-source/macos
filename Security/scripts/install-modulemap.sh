#!/bin/bash

set -e -x
DSTDIR="${DSTROOT}/${PUBLIC_HEADERS_FOLDER_PATH}"

install \
    -d \
    -g "$INSTALL_GROUP" \
    -o "$INSTALL_OWNER" \
    "$DSTDIR"

install \
    -g "$INSTALL_GROUP" \
    -m "$INSTALL_MODE_FLAG" \
    -o "$INSTALL_OWNER" \
    "$PROJECT_DIR"/"$MODULEMAP_FILE" \
    "$DSTDIR"/module.modulemap
