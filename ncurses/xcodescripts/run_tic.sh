#!/bin/sh
set -e -x

# Do nothing for installhdrs
[ "$ACTION" == "installhdrs" ] && exit 0

# Do nothing for iPhoneSimulator build alias
[ "${RC_ProjectName%_Sim}" != "${RC_ProjectName}" ] && exit 0

sh "$PROJECT_DIR"/xcodescripts/native_execs.sh -target tic_static
mkdir -p "$BUILT_PRODUCTS_DIR/native"
cp "$BUILT_PRODUCTS_DIR/tic_static" "$BUILT_PRODUCTS_DIR/native/tic_static"

cd ncurses/misc
PATH="$BUILT_PRODUCTS_DIR/native:$PATH" \
DESTDIR="$DSTROOT" \
suffix=_static \
prefix=/usr \
exec_prefix=/usr \
bindir=/usr/bin \
datadir=/usr/share \
top_srcdir="$SRCROOT/ncurses" \
srcdir="$SRCROOT/ncurses/misc" \
/bin/sh ./run_tic.sh
