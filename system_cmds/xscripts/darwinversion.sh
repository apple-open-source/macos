#!/bin/sh

# This script is intended to generate version information for single-file
# products (i.e. things that don't ship in bundles). It takes an input file
# with the suffix ".version" that defines:
#
# DARWIN_BUNDLE_IDENTIFIER
#	The CFBundleIdentifier for the product.
# DARWIN_DISPLAY_NAME
#	The "marketing" name of the product ("Darwin System Bootstrapper" as opposed
#	to "launchd" or "Darwin Kernel" as opposed to "xnu").
# DARWIN_DISPLAY_VERSION
#	The major release version (think "7.0" for iOS 7, "10.9" for Mavericks,
#	etc.).
# DARWIN_INCREMENTAL_VERSION
#	The incremental version (think 12A132, svn revision, project tag, etc.).
# DARWIN_COPYRIGHT
#	The copyright string.
#
# It produces a header (darwin_version.h) that declares:
#
# __darwin_builder_version
#	The integer representation of the version of OS X which built the project
#	(think 1090, 1091, etc.).
# __darwin_builder_build
#	The integer representation of the build of OS X which built the project,
#	represented in hex (think 0x12A132).
# __darwin_build_inc_version
#	A string representation of the given DARWIN_INCREMENTAL_VERSION.
# __darwin_version_string
#	A composed version string which can serve as useful for identifying the
#	version, variant and origin of a given build. It is formatted as:
#
#		$DARWIN_DISPLAY_NAME Version $DARWIN_DISPLAY_VERSION `date`; `whoami`:<objects>
#
#	<objects> is a symbolic link in the OBJROOT pointing to the subdirectory
#	containing the objects for the target being built. The link's name is
#	formatted as:
#
#		${BASE_PRODUCT_NAME}/${DARWIN_VARIANT}_${UPPER_CASE_CURRENT_ARCH}
#
#	The BASE_PRODUCT_NAME is the first part of the target's PRODUCT_NAME, prior
#	to a '.' character (so the base product name of "launchd.development" is
#	simply "launchd").
#
#	This link points to the appropriate location in the build root. If the SDK
#	being built for is the Simulator, the variant is formatted as:
#
#		${DARWIN_VARIANT}_SIMULATOR_${UPPER_CASE_CURRENT_ARCH}
#
# It produces an XML Info.plist from this information and embeds it in the
# __TEXT,__info_plist section of the resulting binary.

. ${INPUT_FILE_PATH}

baseproductname=${PRODUCT_NAME/.*/}
builder_version=`sw_vers -productVersion`
builder_build=`sw_vers -buildVersion`
brewedondate=`date`
brewedby=`whoami`

if [ $SUDO_USER ]; then
	brewedby="$SUDO_USER"
fi

release="Unknown"
if [[ "$DARWIN_VARIANT" != "RELEASE" && -n "$RC_RELEASE" ]]; then
	release="$RC_RELEASE"
fi

# Distill the version down to its base OS. The builders could be running 10.7.2,
# for example, but the availability macros on OS X only handle major version
# availability. 
builder_version_int=${builder_version/.}
builder_version_int=${builder_version_int/.*}
builder_version_int="${builder_version_int}0"

# Builders don't typically run on later SU trains. They'll usually move to the
# next major release.
if [[ "$builder_build" =~ [g-zG-Z] ]]; then
	builder_build="1A1"
fi

destdir="${DERIVED_SOURCES_DIR}/${CURRENT_ARCH}"
mkdir -p "$destdir"

thehfile="$destdir/darwin_version.h"
thecfile="$destdir/darwin_version.c"

# Hack to emulate how xnu's version works. It has the nice feature of printing
# the OBJROOT of the current xnu, which is different based on build variant and
# architecture. But in our case, the OBJROOT is buried a few levels deep, so we
# create a symlink in the OBJROOT to point to that, or else we'd have to embed
# a much longer path in the version.
mkdir -p "${OBJROOT}/$baseproductname"
cd "${OBJROOT}/$baseproductname"

rootwithslash="${OBJROOT}/"
objpath=`eval echo -n \\$OBJECT_FILE_DIR_\$CURRENT_VARIANT`

capsarch=`echo $CURRENT_ARCH | tr "[:lower:]" "[:upper:]"`
# Xcode does not provide an OBJECT_FILE_DIR_$CURRENT_VARIANT_$CURRENT_ARCH, so
# we have to interpolate the last part of the path.
objpath=$objpath/$CURRENT_ARCH
subpath=${objpath#${rootwithslash}}

if [[ "${PLATFORM_NAME}" =~ "simulator" ]]; then
	linkname="${DARWIN_VARIANT}_SIMULATOR_${capsarch}"
else
	linkname="${DARWIN_VARIANT}_${capsarch}"
fi

objects=`basename ${OBJROOT}`
if [[ "$objects" = "Objects" ]]; then
	# Newer XBSs put the OBJROOT in an "Objects" subdirectory under the build
	# root.
	oldwd=`pwd`
	cd "${OBJROOT}"
	cd ..

	objects=`dirname ${OBJROOT}`
	objects=`basename $objects`
	objects="${objects/_install/}"

	ln -fs "Objects" "$objects"

	cd "$oldwd"
fi
objects="$objects/$baseproductname/$linkname"

ln -s ../${subpath} $linkname
version_string="$DARWIN_DISPLAY_NAME Version $DARWIN_DISPLAY_VERSION: $brewedondate; $brewedby:$objects"

# Generate the symbol root.
binarywithsyms="$SYMROOT/$PRODUCT_NAME"
if [[ $SYMROOT =~ ^/BinaryCache/ ]]; then
	# XBS tosses symbols and roots into /BinaryCache on the builder, so sniff
	# that out and generate the appropriate path. Otherwise, just use the given
	# local SYMROOT.
	symrootsubpath=${SYMROOT#"/BinaryCache/"}
	binarywithsyms="~rc/Software/$release/BuildRecords/$symrootsubpath/$PRODUCT_NAME"
fi

echo "*** Stamping project build:"
echo "*** Release: $release"
echo "*** Builder Version: $builder_version"
echo "*** Builder Build: $builder_build"
echo "*** Project Version: $CURRENT_PROJECT_VERSION"
echo "*** Version String: $version_string"
echo "*** Object Root: $objects"
echo "*** Debugging Binary: $binarywithsyms"

# Generate Info.plist
infoplist="$destdir"/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleIdentifier string" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier $DARWIN_BUNDLE_IDENTIFIER" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Add :CFBundleName string" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Set :CFBundleName $PRODUCT_NAME" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Add :CFBundleDisplayName string" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Set :CFBundleDisplayName $DARWIN_DISPLAY_NAME" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Add :CFBundleExecutable string" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Set :CFBundleExecutable $EXECUTABLE_NAME" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Add :CFBundleInfoDictionaryVersion string" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Set :CFBundleInfoDictionaryVersion 6.0" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Add :CFBundleShortVersionString string" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString $DARWIN_DISPLAY_VERSION" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Add :CFBundleVersion string" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Set :CFBundleVersion $DARWIN_INCREMENTAL_VERSION" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Add :NSHumanReadableCopyright string" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Set :NSHumanReadableCopyright $DARWIN_COPYRIGHT" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Add :DarwinVariant string" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Set :DarwinVariant $DARWIN_VARIANT" -c "Save" $infoplist > /dev/null
# codesign can't deal with the Info.plist for each slice having different
# content, so don't encode architecture-specific information for now.
#
# <rdar://problem/15459303>
#/usr/libexec/PlistBuddy -c "Add :DarwinArchitecture string" -c "Save" $infoplist > /dev/null
#/usr/libexec/PlistBuddy -c "Set :DarwinArchitecture $CURRENT_ARCH" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Add :DarwinBuilderVersion string" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Set :DarwinBuilderVersion $builder_version" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Add :DarwinBuilderBuild string" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Set :DarwinBuilderBuild $builder_build" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Add :DTSDKName string" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Set :DTSDKName $SDK_NAME" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Add :DTSDKBuild string" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Set :DTSDKBuild $PLATFORM_PRODUCT_BUILD_VERSION" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Add :DTXcodeBuild string" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Set :DTXcodeBuild $XCODE_PRODUCT_BUILD_VERSION" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Add :DTCompiler string" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Set :DTCompiler $DEFAULT_COMPILER" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Add :DTPlatformName string" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Set :DTPlatformName $PLATFORM_NAME" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Add :DTPlatformVersion string" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Set :DTPlatformVersion $IPHONEOS_DEPLOYMENT_TARGET" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Add :DTXcode string" -c "Save" $infoplist > /dev/null
/usr/libexec/PlistBuddy -c "Set :DTXcode $XCODE_VERSION_ACTUAL" -c "Save" $infoplist > /dev/null
infoplistcontents=`cat $infoplist`

rm -f "$thehfile"
echo "#ifndef __DARWIN_VERSION_H__" >> "$thehfile"
echo "#define __DARWIN_VERSION_H__" >> "$thehfile"
echo "const unsigned long __darwin_builder_version;" >> "$thehfile"
echo "const unsigned long __darwin_builder_build;" >> "$thehfile"
echo "const char *__darwin_build_inc_version;" >> "$thehfile"
echo "const char *__darwin_version_string;" >> "$thehfile"
echo "const char *__darwin_variant;" >> "$thehfile"
echo "const char *__darwin_debug_binary;" >> "$thehfile"
echo "#endif // __DARWIN_VERSION_H__" >> "$thehfile"
echo "" >> "$thehfile"

rm -f "$thecfile"
echo "__attribute__((__used__)) const unsigned long __darwin_builder_version = $builder_version_int;" >> "$thecfile"
echo "__attribute__((__used__)) const unsigned long __darwin_builder_build = 0x$builder_build;" >> "$thecfile"
echo "__attribute__((__used__)) const char *__darwin_build_inc_version = \"$CURRENT_PROJECT_VERSION\";" >> "$thecfile"
echo "__attribute__((__used__)) const char *__darwin_version_string = \"$version_string\";" >> "$thecfile"
echo "__attribute__((__used__)) const char *__darwin_variant = \"$DARWIN_VARIANT\";" >> "$thecfile"
echo "__attribute__((__used__)) const char *__darwin_version_string_heywhat = \"@(#)VERSION:$version_string\";" >> "$thecfile"
echo "__attribute__((__used__)) const char *__darwin_debug_binary = \"$binarywithsyms\";" >> "$thecfile"

# Embed the Info.plist in the __TEXT,__info_plist section.
echo "__attribute__((__used__))" >> "$thecfile"

echo "__attribute__((__section__(\"__TEXT,__info_plist\")))" >> "$thecfile"
echo -n "static const char __darwin_info_plist[] = \"" >> "$thecfile"
echo -n "$infoplistcontents" | sed -e 's/\"/\\"/g' | tr -d '\n' >> "$thecfile"
echo "\";" >> "$thecfile"

echo "" >> "$thecfile"
