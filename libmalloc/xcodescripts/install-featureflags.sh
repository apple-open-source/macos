#!/bin/bash -ex

if [ "${DRIVERKIT}" = 1 ]; then exit 0; fi

INPUT=${SCRIPT_INPUT_FILE_0}
# Don't use output files: Xcode insists on creating the containing directory,
# which is wrong for DriverKit
OUTPUT=${DSTROOT}/System/Library/FeatureFlags/Domain/libmalloc.plist

mkdir -p "$(dirname "${OUTPUT}")"

xcrun clang -x c -P -E                          \
        ${MALLOC_IOS_FEATUREFLAG_OVERRIDE}      \
        -imacros TargetConditionals.h           \
        "$INPUT"                                \
        -o "$OUTPUT"
