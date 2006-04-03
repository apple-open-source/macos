#!/bin/sh

set -x

BUILDDIR="${BUILT_PRODUCTS_DIR}"
LIBSYSTEM="/usr/lib/libSystem.B.dylib"

for a in ${ARCHS}; do
    if [ "x$a" = "xppc" ]; then
	mkdir -p "${BUILDDIR}/libs-$a"
	ln -sf "${LIBSYSTEM}" "${BUILDDIR}/libs-$a/libmx.dylib"
    fi
done


FRAMEWORKS="MediaKit ApplicationServices CoreServices"

for f in ${FRAMEWORKS}; do
    mkdir -p "${BUILDDIR}/${f}.framework"
    ln -sf "${LIBSYSTEM}" "${BUILDDIR}/${f}.framework/${f}"
done

ln -sf "/System/Library/Frameworks/CoreServices.framework/Headers" "${BUILDDIR}/CoreServices.framework/Headers"
ln -sf "/System/Library/Frameworks/CoreServices.framework/Frameworks" "${BUILDDIR}/CoreServices.framework/Frameworks"

