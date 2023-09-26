#!/bin/sh
set -e -x

if [ "$SCRIPT_INPUT_FILE_COUNT" -ne "$SCRIPT_OUTPUT_FILE_COUNT" ]; then
        1>&2 echo input and output file counts differ
        exit 1
fi

inplist=$SCRIPT_INPUT_FILE_0
outplist=$SCRIPT_OUTPUT_FILE_0

install -d -o root -g wheel -m 0755 \
	"$DSTROOT"/AppleInternal/CoreOS/BATS/unit_tests

# The plist will use TARGET_* conditionals to avoid atf-sh tests on !macOS
xcrun clang -x c -P -E -imacros TargetConditionals.h	\
	-Wno-invalid-pp-token				\
	"$inplist" -o "$outplist"
