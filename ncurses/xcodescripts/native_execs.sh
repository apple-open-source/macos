#!/bin/sh
set -e -x

SDK="-sdk macosx.internal"

export -n SDKROOT
export -n ARCHS CURRENT_ARCH RC_ARCHS RC_CFLAGS
export -n RC_TARGET_CONFIG RC_TARGET_UPDATE EMBEDDED_PROFILE_NAME
# Remove all platform variables from the environment
export -n `env | grep -E "^PLATFORM_" | cut -d= -f1` \
	
# Build the native_tic target using the current SDK for the build host
xcodebuild \
	"$@" \
	$SDK \
	SYMROOT="$SYMROOT" \
	DSTROOT="$BUILT_PRODUCTS_DIR" \
	install
