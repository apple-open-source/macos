#!/bin/sh

# We want to build these for the builder's arch and SDK, not for the target system's as we use them during the build process (and not ever on the target system)

set -e

export -n ARCHS SDKROOT CURRENT_ARCH RC_ARCHS RC_CFLAGS   EFFECTIVE_PLATFORM_NAME EMBEDDED_PROFILE_NAME RC_TARGET_CONFIG RC_TARGET_UPDATE SDK_DIR SDK_NAME `env | grep -E "^PLATFORM_" | cut -d= -f1` 

# export MACOSX_DEPLOYMENT_TARGET=`sw_vers -productVersion`
# NBSDK=`xcodebuild -showsdks | awk '/macosx10\./ { if (match($0, "-sdk +macosx10\.[0-9][^ ]*")) print substr($0, RSTART, RLENGTH); }' | sort | tail -1`

# Select the builder's default sdk
export SDKROOT=''

# native_make_keys depends on native_make_hash...  so we get them both for the price of one
xcodebuild -target native_make_keys MACOSX_DEPLOYMENT_TARGET="$MACOSX_DEPLOYMENT_TARGET" MACOSX_DEPLOYMENT_TARGET="$MACOSX_DEPLOYMENT_TARGET" SYMROOT="$SYMROOT" DSTROOT="$DSTROOT"

