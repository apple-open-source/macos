#!/usr/bin/perl
# Author: Steve Wall <swall@redcom.com>
# $XFree86: xc/programs/xterm/vttests/88colors2.pl,v 1.1 1999/09/25 14:38:51 dawes Exp $
# Made from 256colors2.pl

# use the resources for colors 0-15 - usually more-or-less a
# reproduction of the standard ANSI colors, but possibly more
# pleasing shades

# colors 16-79 are a 4x4x4 color cube
@steps=(0,139,205,255);
for ($red = 0; $red < 4; $red++) {
    for ($green = 0; $green < 4; $green++) {
	for ($blue = 0; $blue < 4; $blue++) {
	    printf("\x1b]4;%d;rgb:%2.2x/%2.2x/%2.2x\x1b\\",
		   16 + ($red * 16) + ($green * 4) + $blue,
		   int (@steps[$red]),
		   int (@steps[$green]),
		   int (@steps[$blue]));
	}
    }
}

# colors 80-87 are a grayscale ramp, intentionally leaving out
# black and white
for ($gray = 0; $gray < 8; $gray++) {
    $level = ($gray * 23.18181818) + 46.36363636;
    if( $gray > 0 ) { $level += 23.18181818; }
    printf("\x1b]4;%d;rgb:%2.2x/%2.2x/%2.2x\x1b\\",
	   80 + $gray, int($level), int($level), int($level));
}


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
print "Color cube, 4x4x4:\n";
for ($green = 0; $green < 4; $green++) {
    for ($red = 0; $red < 4; $red++) {
	for ($blue = 0; $blue < 4; $blue++) {
	    $color = 16 + ($red * 16) + ($green * 4) + $blue;
	    print "\x1b[48;5;${color}m  ";
	}
	print "\x1b[0m ";
    }
    print "\n";
}


# now the grayscale ramp
print "Grayscale ramp:\n";
for ($color = 80; $color < 88; $color++) {
    print "\x1b[48;5;${color}m  ";
}
print "\x1b[0m\n";
