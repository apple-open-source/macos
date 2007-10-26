#!/bin/sh
# $XTermId: make-precompose.sh,v 1.5 2005/04/03 16:58:30 tom Exp $
# $XFree86: xc/programs/xterm/unicode/make-precompose.sh,v 1.4 2005/03/29 04:00:32 tsi Exp $
cat precompose.c.head | sed -e's/@/$/g'
cut UnicodeData.txt -d ";" -f 1,6 | \
 grep ";[0-9,A-F]" | grep " " | \
 sed -e "s/ /, 0x/;s/^/{ 0x/;s/;/, 0x/;s/$/},/" | (sort -k 3 || sort +2)
cat precompose.c.tail
