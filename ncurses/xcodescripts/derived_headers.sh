#!/bin/sh
set -e -x

export AWK=awk
CAPS="$PROJECT_DIR"/ncurses/include/Caps

# curses.h
cat "$PROJECT_DIR"/ncurses/include/curses.head > "$BUILT_PRODUCTS_DIR"/curses.h
sh "$PROJECT_DIR"/ncurses/include/MKkey_defs.sh "$CAPS" \
	>> "$BUILT_PRODUCTS_DIR"/curses.h
cat "$PROJECT_DIR"/ncurses/include/curses.wide >> "$BUILT_PRODUCTS_DIR"/curses.h
cat "$PROJECT_DIR"/ncurses/include/curses.tail >> "$BUILT_PRODUCTS_DIR"/curses.h

# keys.list
sh "$PROJECT_DIR/ncurses/ncurses/tinfo/MKkeys_list.sh" "$CAPS" \
	| sort > "$BUILT_PRODUCTS_DIR"/keys.list

# init_keytry.h
"$BUILT_PRODUCTS_DIR"/make_keys "$BUILT_PRODUCTS_DIR"/keys.list \
	> "$BUILT_PRODUCTS_DIR"/init_keytry.h

# ncurses_def.h
sh "$PROJECT_DIR"/ncurses/include/MKncurses_def.sh \
	"$PROJECT_DIR"/ncurses/include/ncurses_defs \
	> "$BUILT_PRODUCTS_DIR"/ncurses_def.h

# parametrized.h
sh "$PROJECT_DIR"/ncurses/include/MKparametrized.sh "$CAPS" \
	> "$BUILT_PRODUCTS_DIR"/parametrized.h

# term.h
"$AWK" -f "$PROJECT_DIR"/ncurses/include/MKterm.h.awk "$CAPS" \
	> "$BUILT_PRODUCTS_DIR"/term.h
sh "$PROJECT_DIR"/ncurses/include/edit_cfg.sh \
	"$PROJECT_DIR"/ncurses/include/ncurses_cfg.h \
	"$BUILT_PRODUCTS_DIR"/term.h

# transform.h
echo "#define PROG_CAPTOINFO \"captoinfo\"" >  "$BUILT_PRODUCTS_DIR"/transform.h
echo "#define PROG_INFOTOCAP \"infotocap\"" >> "$BUILT_PRODUCTS_DIR"/transform.h
echo "#define PROG_RESET     \"reset\""     >> "$BUILT_PRODUCTS_DIR"/transform.h
echo "#define PROG_INIT      \"init\""      >> "$BUILT_PRODUCTS_DIR"/transform.h
