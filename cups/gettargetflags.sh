#!/bin/sh
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

target=""
variant=""
host=""

cflags="`echo $RC_CFLAGS | sed -e '1,$s/-pipe//'`"
dsoflags=""
ldflags="$cflags"

cppflags=""
for arch in $RC_ARCHS; do
	if test $arch = x86_64 -o $arch = arm64 -o $arch = arm64_32; then
		cppflags="-arch $arch"
		break
	fi
done

sdk="`basename \"$SDKROOT\"`"

case "$sdk" in
	# Current OS's...
	MacOSX*)
		version="`echo $sdk | sed -E -e '1,$s/^[A-Za-z]+10\.([0-9]+).*$/\1/'`"
		iosvers="`expr $version - 2`"
		ldflags=""
		for arch in $RC_ARCHS; do
			target="$target -target $arch-apple-macosx10.$version"
			if test $arch != i386; then
				ldflags="$ldflags -arch $arch"
				variant="$variant -target-variant $arch-apple-ios$iosvers-macabi"
			fi
		done
		;;

	iPhoneOS*)
		version="`echo $sdk | sed -E -e '1,$s/^[A-Za-z]+([0-9]+).*$/\1/'`"
		host="--build `${SRCROOT}/cups/config.guess` --host arm-apple-darwin"
		for arch in $RC_ARCHS; do
			target="$target -target $arch-apple-ios$version"
		done
		;;
	iPhoneSimulator*)
		version="`echo $sdk | sed -E -e '1,$s/^[A-Za-z]+([0-9]+).*$/\1/'`"
		for arch in $RC_ARCHS; do
			target="$target -target $arch-apple-ios$version-simulator"
		done
		;;

	AppleTVOS*)
		version="`echo $sdk | sed -E -e '1,$s/^[A-Za-z]+([0-9]+).*$/\1/'`"
		host="--build `${SRCROOT}/cups/config.guess` --host arm-apple-darwin"
		for arch in $RC_ARCHS; do
			target="$target -target $arch-apple-tvos$version"
		done
		;;
	AppleTVSimulator*)
		version="`echo $sdk | sed -E -e '1,$s/^[A-Za-z]+([0-9]+).*$/\1/'`"
		for arch in $RC_ARCHS; do
			target="$target -target $arch-apple-tvos$version-simulator"
		done
		;;

	WatchOS*)
		version="`echo $sdk | sed -E -e '1,$s/^[A-Za-z]+([0-9]+).*$/\1/'`"
		host="--build `${SRCROOT}/cups/config.guess` --host arm-apple-darwin"
		for arch in $RC_ARCHS; do
			target="$target -target $arch-apple-watchos$version"
		done
		;;
	WatchSimulator*)
		version="`echo $sdk | sed -E -e '1,$s/^[A-Za-z]+([0-9]+).*$/\1/'`"
		for arch in $RC_ARCHS; do
			target="$target -target $arch-apple-watchos$version-simulator"
		done
		;;

	# Otherwise default to no target options...
	*)
		;;
esac

case "$1" in
	cflags | cxxflags)
		output="$target $cflags"
		;;
	cppflags)
		output="$cppflags"
		;;
	dsoflags)
		output="$dsoflags $target $cflags $variant"
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

echo "gettargetflags.sh $1 returning '$output'" 1>&2
echo $output
