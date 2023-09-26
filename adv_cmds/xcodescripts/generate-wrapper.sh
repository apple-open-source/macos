#!/bin/sh

set -e

if [ "$SCRIPT_INPUT_FILE_COUNT" -ne 1 ]; then
	1>&2 echo "Need 1 input file, have $SCRIPT_INPUT_FILE_COUNT (wrapper spec)"
	exit 1
fi

if [ "$SCRIPT_OUTPUT_FILE_COUNT" -ne 1 ]; then
	1>&2 echo "Need 1 output file, have $SCRIPT_OUTPUT_FILE_COUNT (wrapper source)"
	exit 1
fi

"$BUILT_PRODUCTS_DIR"/genwrap_static -o "$SCRIPT_OUTPUT_FILE_0" \
    "$SCRIPT_INPUT_FILE_0"
