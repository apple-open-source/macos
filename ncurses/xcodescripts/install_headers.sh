#!/bin/sh

# check if we're building for the simulator
[ "${RC_ProjectName%_Sim}" != "${RC_ProjectName}" ] && DSTROOT="${DSTROOT}${SDKROOT}"

mkdir -p "$DSTROOT/usr/include"
install -g "$INSTALL_GROUP" -o "$INSTALL_OWNER" -m "$INSTALL_MODE_FLAG" ./ncurses/include/tic.h ./ncurses/menu/eti.h ./ncurses/panel/panel.h ./ncurses/include/ncurses_dll.h ./ncurses/include/unctrl.h ./ncurses/include/nc_tparm.h ./ncurses/include/term.h ./ncurses/form/form.h ./ncurses/include/termcap.h ./ncurses/include/curses.h ./ncurses/menu/menu.h ./ncurses/include/term_entry.h "$DSTROOT/usr/include"
ln -s curses.h "$DSTROOT/usr/include/ncurses.h"
