#!/bin/sh
set -e -x

if [ "$SCRIPT_INPUT_FILE_COUNT" -ne "$SCRIPT_OUTPUT_FILE_COUNT" ]; then
        1>&2 echo input and output file counts differ
        exit 1
fi

inplist=$SCRIPT_INPUT_FILE_0
outplist=$SCRIPT_OUTPUT_FILE_0

install -d -o "$INSTALL_OWNER" -g "$INSTALL_GROUP" -m 0755 \
	"$DSTROOT"/AppleInternal/CoreOS/BATS/unit_tests

plutil -lint "$inplist"
install -o "$INSTALL_OWNER" -g "$INSTALL_GROUP" -m "$ALTERNATE_MODE" \
	"$inplist" "$outplist"
