#!/bin/sh

# check if we're building for the simulator
[ "${RC_ProjectName%_Sim}" != "${RC_ProjectName}" ] && exit 0

cd ncurses/misc
DESTDIR="$DSTROOT" prefix=/usr exec_prefix=/usr bindir=/usr/bin top_srcdir="$SRCROOT/ncurses" srcdir="$SRCROOT/ncurses/misc" datadir=/usr/share /bin/sh ./run_tic.sh
