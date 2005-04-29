#!/bin/sh

set -x


BUILDDIR="${BUILT_PRODUCTS_DIR}"
FRAMEWORKS="MediaKit ApplicationServices CoreServices"
LIBSYSTEM="/usr/lib/libSystem.B.dylib"

for f in ${FRAMEWORKS}; do
    mkdir -p "${BUILDDIR}/${f}.framework"
    ln -sf "${LIBSYSTEM}" "${BUILDDIR}/${f}.framework/${f}"
done
