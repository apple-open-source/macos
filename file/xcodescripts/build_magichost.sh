#!/bin/sh
# This script is a modified version of system_cmds/zic.tproj/build_zichost.sh
# for building file magic
set -e
set -x

if [ $# -ne 1 ]; then
    echo "Usage: $0 BUILT_PRODUCTS_DIR" 1>&2
    exit 1
fi

BUILT_PRODUCTS_DIR="$1"
	
# We may not be building for a platform we can natively
# run on the build machine. Build a dedicated copy of file
# for processing magic files

MAGICHOST_SYMROOT="${BUILT_PRODUCTS_DIR}/magic_host-sym"
MAGICHOST_DSTROOT="${BUILT_PRODUCTS_DIR}/magic_host-dst"
MAGICHOST="${MAGICHOST_DSTROOT}/magic_host"

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
    	XBS_IS_CHROOTED="${XBS_IS_CHROOTED}" \
	SCDontUseServer="${SCDontUseServer}" \
	__CFPREFERENCES_AVOID_DAEMON="${__CFPREFERENCES_AVOID_DAEMON}" \
	__CF_USER_TEXT_ENCODING="${__CF_USER_TEXT_ENCODING}" \
	LANG="${LANG}" \
	HOME="${HOME}" \
	$EXTRA_ARGS \
	TOOLCHAINS="${TOOLCHAINS}" \
	xcrun -sdk "${SDKROOT}" xcodebuild install \
		-target file \
		-sdk "macosx" \
		SRCROOT="${SRCROOT}" \
		OBJROOT="${OBJROOT}/build_magichost" \
		SYMROOT="${MAGICHOST_SYMROOT}" \
		DSTROOT="${MAGICHOST_DSTROOT}" \
		ARCHS='$(NATIVE_ARCH_ACTUAL)' \
		PRODUCT_NAME=magic_host \
		INSTALL_PATH="/"
