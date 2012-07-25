#!/bin/sh
set -e -x

export AWK=awk
CAPS="$PROJECT_DIR"/ncurses/include/Caps

mkdir -p "$BUILT_PRODUCTS_DIR"

# curses.h
cat "$PROJECT_DIR"/ncurses/include/curses.head > "$BUILT_PRODUCTS_DIR"/curses.h
sh "$PROJECT_DIR"/ncurses/include/MKkey_defs.sh "$CAPS" \
	>> "$BUILT_PRODUCTS_DIR"/curses.h
cat "$PROJECT_DIR"/ncurses/include/curses.wide >> "$BUILT_PRODUCTS_DIR"/curses.h
cat "$PROJECT_DIR"/ncurses/include/curses.tail >> "$BUILT_PRODUCTS_DIR"/curses.h

# hashsize.h
sh "$PROJECT_DIR"/ncurses/include/MKhashsize.sh "$CAPS" \
	> "$BUILT_PRODUCTS_DIR"/hashsize.h

# ncurses_def.h
sh "$PROJECT_DIR"/ncurses/include/MKncurses_def.sh \
	"$PROJECT_DIR"/ncurses/include/ncurses_defs \
	> "$BUILT_PRODUCTS_DIR"/ncurses_def.h

# term.h
"$AWK" -f "$PROJECT_DIR"/ncurses/include/MKterm.h.awk "$CAPS" \
	> "$BUILT_PRODUCTS_DIR"/term.h
sh "$PROJECT_DIR"/ncurses/include/edit_cfg.sh \
	"$PROJECT_DIR"/ncurses/include/ncurses_cfg.h \
	"$BUILT_PRODUCTS_DIR"/term.h
