#!/usr/bin/perl
# $XFree86: xc/programs/xterm/vttests/256colors.pl,v 1.1 1999/07/11 08:49:54 dawes Exp $

for ($bg = 0; $bg < 256; $bg++) {
    print "\x1b[9;1H\x1b[2J";
    for ($fg = 0; $fg < 256; $fg++) {
	print "\x1b[48;5;${bg}m\x1b[38;5;${fg}m";
	printf "%03.3d/%03.3d ", $fg, $bg;
    }
    sleep 1;
    print "\n";
}
