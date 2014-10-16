#!/bin/bash

# This is a script that does incremental builds, while 
# making some minimal effort to build in a buildit/xbs 
# compatible way.  It is not intended as a buildit 
# replacement, but can be useful during development for
# quick turn testing.
# ./buildmelocal.sh [-d] [-j] [-p projname] <directory>
# -d installs the dstroot with darwinup
# -j builds in parallel
# -p project to build

PROJECT=""
VERSION="999"
ARCH=x86_64
BUILDDIR="/tmp/buildmelocal"

USEPARALLEL="no"
USEDARWINUP="no"

while getopts "jdhp:" i; do
	case "$i" in
	"j")
		echo "Enabling parallel build"
		USEPARALLEL="yes"
		;;
	"d")
		USEDARWINUP="yes"
		;;
	"p")
		PROJECT=${OPTARG}
		;;
	"h")
		echo "Usage: $0 [-d] [-j] [-p projname] <directory>"
		echo "-d	Uses darwinup to install the dstroot after building"
		echo "-j	Enables parallel building on projects that support it"
		echo "-p projname	Specify the project name to build"
		exit 1
		;;
	"--")
		break
		;;
	esac
done
shift $((OPTIND-1))

if [ "$PROJECT" == "" ]; then
	if [ "$1" == "." ]; then
		PROJECT=`basename "$PWD" | sed -e 's/\-.*//'`
	else
		PROJECT=`basename "$1"`
	fi
fi

SRCROOT=$1
if [ "${SRCROOT}" == "." ]; then
	SRCROOT=$PWD
fi

# Where SDKs are located
XXXSDKPATH="/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/"

CUROS=`sw_vers | grep ProductVersion | awk '{print $2}'`

# The SDK to use
XXXSDK="MacOSX${CUROS}.Internal.sdk"

case "$CUROS" in
"10.10") 
	CURRELEASE="OSX"
	;;
*)
	CURRELEASE="Unknown"
	;;
esac

XXXSDKROOT="${XXXSDKPATH}${XXXSDK}"
OBJROOT="${BUILDDIR}/${PROJECT}/obj"
SYMROOT="${BUILDDIR}/${PROJECT}/sym"
DSTROOT="${BUILDDIR}/${PROJECT}/dst"

mkdir -p "${BUILDDIR}"
mkdir -p "${OBJROOT}"
mkdir -p "${SYMROOT}"
mkdir -p "${DSTROOT}"

export PATH="/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin"
export RC_ProjectName=${PROJECT}
export RC_ProjectSourceVersion=${VERSION}
export RC_ProjectNameAndSourceVersion=${PROJECT}-${VERSION}
export RC_ProjectBuildVersion=1
export RC_CFLAGS="-arch ${ARCH} -pipe"
export RC_NONARCH_CFLAGS="-pipe"
export RC_ARCHS=${ARCH}
export SRCROOT=${SRCROOT}
export OBJROOT=${OBJROOT}
export SYMROOT=${SYMROOT}
export DSTROOT=${DSTROOT}
export SDKROOT="macosx${CUROS}internal"
export SEPARATE_STRIP="YES"
export APPLEGLOT_CONTINUE_ON_ERROR=YES
export CC_PRINT_OPTIONS="YES"
#export CC_LOG_DIAGNOSTICS=1
export DT_TOOLCHAIN_DIR="/Applications/Xcode.app/Contents/Developer/Toolchains/OSX${CUROS}.xctoolchain"
export TERM="vt100"
export UNAME_RELEASE="14.0"
export UNAME_SYSNAME="Darwin"
export USER="root"
export USERNAME="root"
export USE_PER_CONFIGURATION_BUILD_LOCATIONS="NO"
export XBS_MORE_BUILD_FLAGS="-Wno-error=deprecated-declarations -Wno-error=#warnings"
export XCODE_XCCONFIG_FILE="/usr/share/buildit/xcodeBuildSettingOverrides.xcconfig"
export XTYPE_IGNORE_SERVER="1"
export __CFPREFERENCES_AVOID_DAEMON="1"
export __CF_USER_TEXT_ENCODING="0x0:0:0"
export SHELL=/bin/sh
export DEVELOPER_DIR="/Applications/Xcode.app/Contents/Developer"
export LIBDISPATCH_DISABLE_KWQ="1"
export CCHROOT="${OBJROOT}/Caches"
export INSTALLED_PRODUCT_ASIDES="YES"
export INTERFACER_TRACE_INTERFACES="YES"
export RC_ARCHS=${ARCH}
export RC_BUILDIT="YES"
export RC_DEBUG_OPTIONS="YES"
export RC_FORCE_SSE3="YES"
export RC_MAJOR_RELEASE_TRAIN="OSX"
export RC_TRACE_ARCHIVES="YES"
export RC_TRACE_DYLIBS="YES"
export RC_XBS="YES"
export REACH_SERVER="DO_NOT_TALK_WITH_REACH_SERVER"
export SCDontUseServer="1"

echo $SRCROOT

if [ -f "${SRCROOT}/Makefile" -o -f "${SRCROOT}/makefile" -o -f "${SRCROOT}/GNUMakefile" ]; then
	DSS_BUILD_PARALLEL=${USEPARALLEL} SHELL=/bin/sh SRCROOT="${SRCROOT}" OBJROOT="${OBJROOT}" SYMROOT="${SYMROOT}" DSTROOT="${DSTROOT}" PATH="/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin" "/Applications/Xcode.app/Contents/Developer/usr/bin/make" -Wno-error=deprecated-declarations -Wno-error=#warnings -C "${SRCROOT}" "install" "SRCROOT=${SRCROOT}" "SDKROOT=${XXXSDKROOT}" "OBJROOT=${OBJROOT}" "SYMROOT=${SYMROOT}" "DSTROOT=${DSTROOT}" "MACOSX_DEPLOYMENT_TARGET=${CUROS}" "CCHROOT=${OBJROOT}/Caches" "RC_ProjectName=${PROJECT}" "RC_ProjectSourceVersion=${VERSION}" "RC_ProjectNameAndSourceVersion=${PROJECT}-${VERSION}" "RC_ProjectBuildVersion=1" "RC_ReleaseStatus=Development" "RC_CFLAGS=-arch ${ARCH} -pipe"  "RC_NONARCH_CFLAGS=-pipe" "RC_ARCHS=${ARCH}" "RC_${ARCH}=YES" "RC_RELEASE=${CURRELEASE}" "RC_OS=macos" "RC_XBS=YES" "RC_BUILDIT=YES" 2>&1 | tee "${BUILDDIR}/${PROJECT}/log"
else
	SAVEPWD=$PWD
	cd "${SRCROOT}"
	SHELL=/bin/sh SRCROOT="${SRCROOT}" OBJROOT="${OBJROOT}" SYMROOT="${SYMROOT}" DSTROOT="${DSTROOT}" PATH="/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin" "/Applications/Xcode.app/Contents/Developer/usr/bin/xcodebuild" "install" "SRCROOT=${SRCROOT}" "SDKROOT=${XXXSDKROOT}" "OBJROOT=${OBJROOT}" "SYMROOT=${SYMROOT}" "DSTROOT=${DSTROOT}" "MACOSX_DEPLOYMENT_TARGET=${CUROS}" "CCHROOT=${OBJROOT}/Caches" "RC_ProjectName=${PROJECT}" "RC_ProjectSourceVersion=${VERSION}" "RC_ProjectNameAndSourceVersion=${PROJECT}-${VERSION}" "RC_ProjectBuildVersion=1" "RC_ReleaseStatus=Development" "RC_CFLAGS=-arch ${ARCH} -pipe"  "RC_NONARCH_CFLAGS=-pipe" "RC_ARCHS=${ARCH}" "RC_${ARCH}=YES" "RC_RELEASE=${CURRELEASE}" "RC_OS=macos" "RC_XBS=YES" "RC_BUILDIT=YES" 2>&1 | tee "${BUILDDIR}/${PROJECT}/log"
	cd "$SAVEPWD"
fi
if [ "${PIPESTATUS[0]}" == "0" -a "$USEDARWINUP" == "yes" ]; then
	echo Installing ${DSTROOT} with darwinup
	darwinup install "${DSTROOT}"
fi
