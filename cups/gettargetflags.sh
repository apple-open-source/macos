#!/bin/sh
set -euo pipefail
#
# Determine which target options, if any, are needed for the build.
#
# Uses the SDKROOT environment variable to determine the SDK version,
# other RC_* environment variables as needed...
#
# Usage:
#
#   ./gettargetflags.sh cflags
#   ./gettargetflags.sh cppflags
#   ./gettargetflags.sh cxxflags
#   ./gettargetflags.sh dsoflags
#   ./gettargetflags.sh host
#   ./gettargetflags.sh ldflags
#   ./gettargetflags.sh tapi
#

SRCROOT=${SRCROOT:-"."}
SDKROOT=${SDKROOT:-"`xcrun -sdk macosx.internal --show-sdk-path`"}
#SDKROOT=${SDKROOT:-"`xcrun --show-sdk-path`"}
RC_CFLAGS=${RC_CFLAGS:-""}
RC_ARCHS=${RC_ARCHS:-"`uname -m`"}

target=""
variant=""
host=""

cflags="`echo $RC_CFLAGS | sed -e '1,$s/-pipe//'`"
dsoflags=""
ldflags="$cflags"

cppflags=""
for arch in $RC_ARCHS; do
	if test $arch = x86_64 -o $arch = arm64e -o $arch = arm64 -o $arch = arm64_32; then
		cppflags="-arch $arch"
		break
	fi
done

sdk="`basename \"$SDKROOT\" | tr '[A-Z]' '[a-z]'`"
version=`/usr/libexec/PlistBuddy -c "print Version" $SDKROOT/SDKSettings.plist`

_xos=${RC_PROJECT_COMPILATION_PLATFORM:-""}	# what os (macos, ios, tvos, watchos)

case "$sdk" in
	# Current OS's...
	macosx*)
		_xos="macos"
		;;
 	iphone*)
		_xos="ios"
		;;
	appletv*)
		_xos="tvos"
		;;
	watch*)
		_xos="watchos"
		;;
esac

if [[ $_xos == "macos" ]]; then
	iosvers=`/usr/libexec/PlistBuddy -c "print VersionMap:macOS_iOSMac:$version" $SDKROOT/SDKSettings.plist`
	ldflags=""
	for arch in $RC_ARCHS; do
		target="$target -target $arch-apple-macosx10.$version"
		if test $arch != i386; then
			ldflags="$ldflags -arch $arch"
			variant="$variant -target-variant $arch-apple-ios$iosvers-macabi"
		fi
	done
else
	_sim=
	_machine=

	case "$sdk" in
		*simulator*)
			_machine=`uname -m`
			_sim="-simulator"
			;;
		*)
			_machine="arm"
			_sim=""
			;;
	esac

	# we always build on this system
	_host_build="--build `${SRCROOT}/cups/config.guess`"

	# host is what we're building for
	_host_host="--host $_machine-apple-darwin"

	# target is the third part of the triple and if _machine is the same as the build machine, this lets
	# us indicate to configure that we're building a cross compiler
	_host_target="--target $_machine-apple-$_xos$version$_sim"

	# put it all together...
	host="$_host_build  $_host_host $_host_target"

	for arch in $RC_ARCHS; do
		target="$target -target $arch-apple-$_xos$version$_sim"
	done
fi

case "$@" in
	cflags | cxxflags)
		output="$target $cflags $cppflags $variant"
		;;
	cppflags)
		output="$cppflags"
		;;
	dsoflags)
		output="$dsoflags $target $cflags $variant -L../cups"
		;;
	host)
		output="$host"
		;;
	ldflags)
		output="$target $ldflags"
		;;
	tapi)
		output="$target $variant"
		;;
	*)
		echo "Usage: ./gettargetflags.sh {cflags,cppflags,cxxflags,dsoflags,host,ldflags,tapi}"
		exit 1
		;;
esac

echo "# gettargetflags.sh $1 returning '$output'" 1>&2
echo $output
