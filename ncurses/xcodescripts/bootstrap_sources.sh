#!/bin/sh
set -e -x

CAPS="$PROJECT_DIR"/ncurses/include/Caps

# names.c
awk -f "$PROJECT_DIR"/ncurses/ncurses/tinfo/MKnames.awk bigstrings=1 \
	< "$CAPS" \
	> "$BUILT_PRODUCTS_DIR"/names.c
