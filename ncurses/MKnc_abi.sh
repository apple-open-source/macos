#!/bin/sh

scriptdir=$(dirname $(realpath "$0"))

# Exclude an ABI once we've generated the list for it, then if this script has
# any output those need to be implemented in nc_abi.c.  EXCLUDED is passed to
# egrep, so just pipe-separate the version macros here.
EXCLUDED="NCURSES_EXPORT_ABI60"

cat ${scriptdir}/ncurses/include/curses.wide |
    awk '$0 ~ /NCURSES_EXPORT_ABI[0-9].+/ { print $0 }' |
    grep -Ev -e "$EXCLUDED|define _NCURSES_EXPORT_ABI" |
    sed -E \
      -e 's/^extern //' \
      -e 's/;[^;]+$/\n{\n}\n/' \
      -e 's/NCURSES_EXPORT_ABI/_NCURSES_EXPORT_ABI/' \
      -e 's/\) ([^(]+) \(/) (\1) (/'
