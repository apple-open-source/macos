#!/bin/sh
set -e -x

CC=`xcrun -find cc -sdk macosx.internal`
AWK=/usr/bin/awk

CAPS="$PROJECT_DIR"/ncurses/include/Caps

INCDIR="-I$BUILT_PRODUCTS_DIR -I$PROJECT_DIR/ncurses/ncurses -I$PROJECT_DIR/ncurses/include"
MACROS="-DHAVE_CONFIG_H -U_XOPEN_SOURCE -D_XOPEN_SOURCE=600 -D_XOPEN_SOURCE_EXTENDED -DNDEBUG -DSIGWINCH=28"
if [ -n "$SDKROOT" ]; then
        CFLAGS="-isysroot $SDKROOT"
fi

# codes.c
awk -f "$PROJECT_DIR/ncurses/ncurses/tinfo/MKcodes.awk" bigstrings=1 "$CAPS" \
	> "$BUILT_PRODUCTS_DIR"/codes.c

# comp_captab.c
pushd "$BUILT_PRODUCTS_DIR"
sh "$PROJECT_DIR"/ncurses/ncurses/tinfo/MKcaptab.sh "$AWK" 1 \
	"$PROJECT_DIR"/ncurses/ncurses/tinfo/MKcaptab.awk \
	"$CAPS" \
	> "$BUILT_PRODUCTS_DIR"/comp_captab.c
popd

# expanded.c
sh "$PROJECT_DIR"/ncurses/ncurses/tty/MKexpanded.sh \
        "$CC -E" $CFLAGS $INCDIR $MACROS \
        > "$BUILT_PRODUCTS_DIR"/expanded.c

# fallback.c
sh "$PROJECT_DIR/ncurses/ncurses/tinfo/MKfallback.sh" \
	/usr/share/terminfo \
	"$PROJECT_DIR"/ncurses/misc/terminfo.src \
	> "$BUILT_PRODUCTS_DIR"/fallback.c

# lib_gen.c
sh "$PROJECT_DIR"/ncurses/ncurses/base/MKlib_gen.sh \
        "$CC -E -DHAVE_CONFIG $CFLAGS $INCDIR $MACROS" \
        "$AWK" \
        generated \
        < "$BUILT_PRODUCTS_DIR"/curses.h \
        > "$BUILT_PRODUCTS_DIR"/lib_gen.c

# lib_keyname.c
awk -f "$PROJECT_DIR"/ncurses/ncurses/base/MKkeyname.awk bigstrings=1 \
	"$BUILT_PRODUCTS_DIR"/keys.list \
	> "$BUILT_PRODUCTS_DIR"/lib_keyname.c

# names.c
awk -f "$PROJECT_DIR"/ncurses/ncurses/tinfo/MKnames.awk bigstrings=1 \
	< "$CAPS" \
	> "$BUILT_PRODUCTS_DIR"/names.c

# termsort.c
sh "$PROJECT_DIR"/ncurses/progs/MKtermsort.sh "$AWK" "$CAPS" \
	> "$BUILT_PRODUCTS_DIR"/termsort.c

# unctrl.c
echo | awk -f "$PROJECT_DIR"/ncurses/ncurses/base/MKunctrl.awk bigstrings=1 \
	> "$BUILT_PRODUCTS_DIR"/unctrl.c
