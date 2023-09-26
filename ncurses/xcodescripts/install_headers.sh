#!/bin/sh
set -e -x

if [ $PLATFORM_NAME = "macosx" ]
then
    HEADERS_DIR=usr/include
else
    HEADERS_DIR=usr/local/include
fi

mkdir -p "$DSTROOT/$HEADERS_DIR"
install -g "$INSTALL_GROUP" -o "$INSTALL_OWNER" -m "$INSTALL_MODE_FLAG" \
	./ncurses/include/tic.h \
	./ncurses/menu/eti.h \
	./ncurses/panel/panel.h \
	./ncurses/include/ncurses_dll.h \
	./ncurses/include/unctrl.h \
	./ncurses/include/nc_tparm.h \
	"$BUILT_PRODUCTS_DIR"/term.h \
	./ncurses/form/form.h \
	./ncurses/include/termcap.h \
	"$BUILT_PRODUCTS_DIR"/curses.h \
	./ncurses/menu/menu.h \
	./ncurses/include/term_entry.h \
	"$BUILT_PRODUCTS_DIR"/ncurses.modulemap \
	"$DSTROOT/$HEADERS_DIR"
ln -s -f curses.h "$DSTROOT/$HEADERS_DIR/ncurses.h"
