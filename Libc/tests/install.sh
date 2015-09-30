#!/bin/bash

## Install results in /
noinstall=0
train=

while [ $# -gt 0 ]; do
	if [ "${1/=*/}" = "--build" ]; then
		build="${1/*=/}"
	elif [ "${1/=*/}" = "--train" ]; then
		train="${1/*=/}"
	elif [ "${1/=*/}" = "--sdk" ]; then
		sdk="${1/*=/}"
	elif [ "$1" = "--noinstall" ]; then
		noinstall=1
        elif [ "${1/=*/}" = "--arch" ]; then
                arch="${1/*=/}"
        elif [ "$1" = "--nightlies" ]; then
                nightlies=1
	else
		echo "install: [--sdk=macosx10.6] [--build=10A432] [--train=SnowLeopard] [--noinstall] [--arch=armv7s] [--nightlies]" 2>&1
		exit 1
	fi
	shift
done

if [ $EUID -ne 0 ]; then
	echo "install: script must be run as root" 2>&1
	exit 1
fi

if [ -n "$sdk" ]; then
	export SDKROOT="$(xcodebuild -version -sdk "$sdk" Path)"
fi

if [ -n "$nightlies" ]; then
        export BSDTESTS_NIGHTLIES=1
        export TMPDIR="/tmp"
fi

if [ -z "$build" ]; then
	if [ -n "$SDKROOT" ]; then
		build="$(xcodebuild -version -sdk "$SDKROOT" ProductBuildVersion)"
	else
		build="$(sw_vers -buildVersion)"
	fi
fi

# if [ -z "$train" -a -n "$SDKROOT" -a -d /Volumes/Build/Views ]; then
#   train="$(find /Volumes/Build/Views/*/Updates -maxdepth 1 -type d -name "*$build" | \
#     sed "s#.*/\\(.*\\)$build#\\1#")"
# fi
# if [ -z "$train" ]; then
#   train="$(~rc/bin/getTrainForBuild --quiet "$build")"
# fi

if [ -n "$BSDTESTS_NIGHTLIES" ]; then
        ROOTS="$(mktemp -d -t "LibcTestsRoots.$train$build")"
else
        ROOTS=/var/tmp/LibcTestsRoots."$train$build"
fi
: ${DSTROOT:="$ROOTS/LibcTests~dst"}

if [ -z "$DSTROOT" -o "$DSTROOT" = "/" ]; then
	echo "install: DSTROOT = \"$DSTROOT\"" 2>&1
	exit 1
fi
TESTROOTS="$ROOTS/libctest.roots/bsdtests.libc"

# Building for another version implies noinstall
if [ -n "$SDKROOT" -o "$build" != "$(sw_vers -buildVersion)" ]; then
	noinstall=1
fi

if [ -n "$arch" ]; then
    ARCHS="$arch"
else
    ARCHS="$(xcrun lipo -detailed_info "$SDKROOT"/usr/lib/libSystem.dylib | \
            awk '/^architecture /'"$([ -z "$SDKROOT" ] && \
            echo ' && !/armv6$/')"' {sub("ppc7400", "ppc"); ORS=" "; print $2}')"
fi

set -ex
mkdir -p "$ROOTS"
if [ -z "$BSDTESTS_NIGHTLIES" ]; then
    rm -rf "$(dirname $TESTROOTS)"
fi

xcodebuild install ARCHS="$ARCHS" \
	SYMROOT="$TESTROOTS~sym" OBJROOT="$TESTROOTS~obj" DSTROOT="$TESTROOTS~dst" \
	$([ -n "$SDKROOT" ] && echo "SDKROOT=$SDKROOT")

if [ $noinstall -eq 0 ]; then
	darwinup install "$TESTROOTS~dst"
else
	mkdir -p "$DSTROOT"
	ditto "$TESTROOTS~dst" "$DSTROOT"
        set -
        echo "TEST_ROOT: $DSTROOT"
  # if [ -n "$SDKROOT" ]; then
  #   mkdir -p "$DSTROOT"/usr/share/dict
  #   ln -f {,"$DSTROOT"}/usr/share/dict/words
  # fi
fi
