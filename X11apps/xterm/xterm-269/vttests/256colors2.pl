#!/usr/bin/perl
# $XTermId: 256colors2.pl,v 1.10 2009/10/10 14:45:26 tom Exp $
# -----------------------------------------------------------------------------
# this file is part of xterm
#
# Copyright 1999-2007,2009 by Thomas E. Dickey
# Copyright 2002 by Steve Wall
# Copyright 1999 by Todd Larason
# 
#                         All Rights Reserved
# 
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
# 
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
# 
# Except as contained in this notice, the name(s) of the above copyright
# holders shall not be used in advertising or otherwise to promote the
# sale, use or other dealings in this Software without prior written
# authorization.
# -----------------------------------------------------------------------------
#
# use the resources for colors 0-15 - usually more-or-less a
# reproduction of the standard ANSI colors, but possibly more
# pleasing shades

use strict;

use Getopt::Std;

our ($opt_h, $opt_q, $opt_r);
&getopts('hqr') || die("Usage: $0 [-q] [-r]");
die("Usage: $0 [options]\n
Options:
  -h  display this message
  -q  quieter output by merging all palette initialization
  -r  display the reverse of the usual palette
") if ( $opt_h);

our ($red, $green, $blue);
our ($gray, $level, $color);

sub map_cube($) {
	my $value = $_[0];
	$value = (5 - $value) if defined($opt_r);
	return $value;
}

sub map_gray($) {
	my $value = $_[0];
	$value = (23 - $value) if defined($opt_r);
	return $value;
}

printf("\x1b]4") if ($opt_q);
# colors 16-231 are a 6x6x6 color cube
for ($red = 0; $red < 6; $red++) {
    for ($green = 0; $green < 6; $green++) {
	for ($blue = 0; $blue < 6; $blue++) {
	    printf("\x1b]4") unless ($opt_q);
	    printf(";%d;rgb:%2.2x/%2.2x/%2.2x",
		   16 + (map_cube($red) * 36) + (map_cube($green) * 6) + map_cube($blue),
		   ($red ? ($red * 40 + 55) : 0),
		   ($green ? ($green * 40 + 55) : 0),
		   ($blue ? ($blue * 40 + 55) : 0));
	    printf("\x1b\\") unless ($opt_q);
	}
    }
}

# colors 232-255 are a grayscale ramp, intentionally leaving out
# black and white
for ($gray = 0; $gray < 24; $gray++) {
    $level = (map_gray($gray) * 10) + 8;
    printf("\x1b]4") unless ($opt_q);
    printf(";%d;rgb:%2.2x/%2.2x/%2.2x",
	   232 + $gray, $level, $level, $level);
    printf("\x1b\\") unless ($opt_q);
}
printf("\x1b\\") if ($opt_q);


# display the colors

# first the system ones:
print "System colors:\n";
for ($color = 0; $color < 8; $color++) {
    print "\x1b[48;5;${color}m  ";
}
print "\x1b[0m\n";
for ($color = 8; $color < 16; $color++) {
    print "\x1b[48;5;${color}m  ";
}
print "\x1b[0m\n\n";

# now the color cube
print "Color cube, 6x6x6:\n";
for ($green = 0; $green < 6; $green++) {
    for ($red = 0; $red < 6; $red++) {
	for ($blue = 0; $blue < 6; $blue++) {
	    $color = 16 + ($red * 36) + ($green * 6) + $blue;
	    print "\x1b[48;5;${color}m  ";
	}
	print "\x1b[0m ";
    }
    print "\n";
}


# now the grayscale ramp
print "Grayscale ramp:\n";
for ($color = 232; $color < 256; $color++) {
    print "\x1b[48;5;${color}m  ";
}
print "\x1b[0m\n";
