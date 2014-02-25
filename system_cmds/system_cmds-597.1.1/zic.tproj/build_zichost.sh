#!/bin/sh
set -e
set -x

if [ $# -ne 1 ]; then
    echo "Usage: $0 BUILT_PRODUCTS_DIR" 1>&2
    exit 1
fi

BUILT_PRODUCTS_DIR="$1"
	
# We may not be building for a platform we can natively
# run on the build machine. Build a dedicate copy of zic
# for processing zoneinfo files

ZICHOST_SYMROOT="${BUILT_PRODUCTS_DIR}/zic_host-sym"
ZICHOST_DSTROOT="${BUILT_PRODUCTS_DIR}/zic_host-dst"
ZICHOST="${ZICHOST_DSTROOT}/zic_host"

# A full environment causes build settings from a cross
# build (like PLATFORM_NAME) to leak into a native
# host tool build

EXTRA_ARGS=""
if [ -n "${XCODE_DEVELOPER_USR_PATH}" ]; then
    EXTRA_ARGS="XCODE_DEVELOPER_USR_PATH=${XCODE_DEVELOPER_USR_PATH}"
fi

env -i \
	TMPDIR="${TMPDIR}" \
	PATH="${PATH}" \
	SCDontUseServer="${SCDontUseServer}" \
	__CFPREFERENCES_AVOID_DAEMON="${__CFPREFERENCES_AVOID_DAEMON}" \
	__CF_USER_TEXT_ENCODING="${__CF_USER_TEXT_ENCODING}" \
	LANG="${LANG}" \
	HOME="${HOME}" \
	$EXTRA_ARGS \
	xcrun -sdk "${SDKROOT}" xcodebuild install \
		-target zic \
		-sdk "macosx" \
		SRCROOT="${SRCROOT}" \
		OBJROOT="${OBJROOT}" \
		SYMROOT="${ZICHOST_SYMROOT}" \
		DSTROOT="${ZICHOST_DSTROOT}" \
		ARCHS='$(NATIVE_ARCH_ACTUAL)' \
		PRODUCT_NAME=zic_host \
		INSTALL_PATH="/"
