#!/usr/bin/perl
#
# ucs2any.pl -- Markus Kuhn <mkuhn@acm.org>
#
# This Perl script allows you to generate from an ISO10646-1 encoded
# BDF font other BDF fonts in any possible encoding. This way, you can
# derive from a single ISO10646-1 master font a whole set of 8-bit
# fonts in all ISO 8859 and various other encodings. (Hopefully
# a future XFree86 release will have a similar facility built into
# the server, which can reencode ISO10646-1 on the fly, because
# storing the same fonts in many different encodings is clearly
# a waste of storage capacity).
#
# Id: ucs2any.pl,v 1.12 2001-02-17 15:21:05+00 mgk25 Rel
# $XFree86: xc/fonts/util/ucs2any.pl,v 1.5 2002/10/12 16:06:42 herrb Exp $

use strict 'subs';

# DEC VT100 graphics characters in the range 1-31 (as expected by
# some old xterm versions and a few other applications)
%decmap = ( 0x01 => 0x25C6, # BLACK DIAMOND
	    0x02 => 0x2592, # MEDIUM SHADE
	    0x03 => 0x2409, # SYMBOL FOR HORIZONTAL TABULATION
	    0x04 => 0x240C, # SYMBOL FOR FORM FEED
	    0x05 => 0x240D, # SYMBOL FOR CARRIAGE RETURN
	    0x06 => 0x240A, # SYMBOL FOR LINE FEED
	    0x07 => 0x00B0, # DEGREE SIGN
	    0x08 => 0x00B1, # PLUS-MINUS SIGN
	    0x09 => 0x2424, # SYMBOL FOR NEWLINE
	    0x0A => 0x240B, # SYMBOL FOR VERTICAL TABULATION
	    0x0B => 0x2518, # BOX DRAWINGS LIGHT UP AND LEFT
	    0x0C => 0x2510, # BOX DRAWINGS LIGHT DOWN AND LEFT
	    0x0D => 0x250C, # BOX DRAWINGS LIGHT DOWN AND RIGHT
	    0x0E => 0x2514, # BOX DRAWINGS LIGHT UP AND RIGHT
	    0x0F => 0x253C, # BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL
	    0x10 => 0x23BA, # HORIZONTAL SCAN LINE-1 (Unicode 3.2 draft)
	    0x11 => 0x23BB, # HORIZONTAL SCAN LINE-3 (Unicode 3.2 draft)
	    0x12 => 0x2500, # BOX DRAWINGS LIGHT HORIZONTAL
	    0x13 => 0x23BC, # HORIZONTAL SCAN LINE-7 (Unicode 3.2 draft)
	    0x14 => 0x23BD, # HORIZONTAL SCAN LINE-9 (Unicode 3.2 draft)
	    0x15 => 0x251C, # BOX DRAWINGS LIGHT VERTICAL AND RIGHT
	    0x16 => 0x2524, # BOX DRAWINGS LIGHT VERTICAL AND LEFT
	    0x17 => 0x2534, # BOX DRAWINGS LIGHT UP AND HORIZONTAL
	    0x18 => 0x252C, # BOX DRAWINGS LIGHT DOWN AND HORIZONTAL
	    0x19 => 0x2502, # BOX DRAWINGS LIGHT VERTICAL
	    0x1A => 0x2264, # LESS-THAN OR EQUAL TO
	    0x1B => 0x2265, # GREATER-THAN OR EQUAL TO
	    0x1C => 0x03C0, # GREEK SMALL LETTER PI
	    0x1D => 0x2260, # NOT EQUAL TO
	    0x1E => 0x00A3, # POUND SIGN
	    0x1F => 0x00B7  # MIDDLE DOT
	  );

sub is_control {
    my ($ucs) = @_;

    return (($ucs >= 0x00 && $ucs <= 0x1f) ||
	    ($ucs >= 0x7f && $ucs <= 0x9f));
}

sub is_blockgraphics {
    my ($ucs) = @_;

    return $ucs >= 0x2500 && $ucs <= 0x25FF;
}

# calculate the bounding box that covers both provided bounding boxes
sub combine_bbx {
    my ($awidth, $aheight, $axoff, $ayoff,
        $cwidth, $cheight, $cxoff, $cyoff) = @_;

    if ($axoff < $cxoff) {
        $cwidth += $cxoff - $axoff;
        $cxoff = $axoff;
    }
    if ($ayoff < $cyoff) {
        $cheight += $cyoff - $ayoff;
        $cyoff = $ayoff;
    }
    if ($awidth + $axoff > $cwidth + $cxoff) {
        $cwidth = $awidth + $axoff - $cxoff;
    }
    if ($aheight + $ayoff > $cheight + $cyoff) {
        $cheight = $aheight + $ayoff - $cyoff;
    }

    return ($cwidth, $cheight, $cxoff, $cyoff);
}

print <<End if $#ARGV < 0;

Usage: ucs2any [+d|-d] <source-name> { <mapping-file> <registry-encoding> }

where

   +d                   put DEC VT100 graphics characters in the C0 range
                        (default for upright charcell fonts)

   -d                   do not put DEC VT100 graphics characters in the
                        C0 range (default for all other font types)

   <source-name>        is the name of an ISO10646-1 encoded BDF file

   <mapping-file>       is the name of a character set table like those on
                        <ftp://ftp.unicode.org/Public/MAPPINGS/>

   <registry-encoding>  are the CHARSET_REGISTRY and CHARSET_ENCODING
                        field values for the font name (XLFD) of the
                        target font, separated by a hyphen

Example:

   ucs2any 6x13.bdf 8859-1.TXT iso8859-1 8859-2.TXT iso8859-2

will generate the files 6x13-iso8859-1.bdf and 6x13-iso8859-2.bdf

End

exit if $#ARGV < 0;

# check options
if ($ARGV[0] eq '+d') {
    shift @ARGV;
    $dec_chars = 1;
} elsif ($ARGV[0] eq '-d') {
    shift @ARGV;
    $dec_chars = 0;
}

# open and read source file
$fsource = $ARGV[0];
open(FSOURCE,  "<$fsource")  || die ("Can't read file '$fsource': $!\n");

# read header
$properties = 0;
$default_char = 0;
while (<FSOURCE>) {
    last if /^CHARS\s/;
    if (/^STARTFONT/) {
	$startfont = $_;
    } elsif (/^_XMBDFED_INFO\s/ || /^_XFREE86_GLYPH_RANGES\s/) {
	$properties--;
    } elsif (/DEFAULT_CHAR\s+([0-9]+)\s*$/) {
	$default_char = $1;
	$header .= "DEFAULT_CHAR 0\n";
    } else {
	if (/^STARTPROPERTIES\s+(\d+)/) {
	    $properties = $1;
	} elsif (/^FONT\s+(.*-([^-]*-\S*))\s*$/) {
	    if ($2 ne "ISO10646-1") {
		die("FONT name in '$fsource' is '$1' and " .
		    "not '*-ISO10646-1'!\n");
	    };
	} elsif (/^CHARSET_REGISTRY\s+"(.*)"\s*$/) {
	    if ($1 ne "ISO10646") {
		die("CHARSET_REGISTRY in '$fsource' is '$1' and " .
		    "not 'ISO10646'!\n");
	    };
	} elsif (/^CHARSET_ENCODING\s+"(.*)"\s*$/) {
	    if ($1 ne "1") {
		die("CHARSET_ENCODING in '$fsource' is '$1' and " .
		    "not '1'!\n");
	    };
        } elsif (/^SLANT\s+"(.*)"\s*$/) {
	    $slant = $1;
	    $slant =~ tr/a-z/A-Z/;
	} elsif (/^SPACING\s+"(.*)"\s*$/) {
	    $spacing = $1;
	    $spacing =~ tr/a-z/A-Z/;
	}
	s/^COMMENT\s+\"(.*)\"$/COMMENT $1/;
	s/^COMMENT\s+\$[I]d: (.*)\$\s*$/COMMENT Derived from $1\n/;
        $header .= $_;
    }
}

die ("No STARTFONT line found in '$fsource'!\n") unless $startfont;
$header =~ s/\nSTARTPROPERTIES\s+(\d+)\n/\nSTARTPROPERTIES $properties\n/;

# read characters
while (<FSOURCE>) {
    if (/^STARTCHAR/) {
	$sc = $_;
	$code = -1;
    } elsif (/^ENCODING\s+(-?\d+)/) {
        $code = $1;
	$startchar{$code} = $sc;
	$char{$code} = "";
    } elsif (/^ENDFONT$/) {
	$code = -1;
	$sc = "STARTCHAR ???\n";
    } else {
        $char{$code} .= $_;
        if (/^ENDCHAR$/) {
            $code = -1;
	    $sc = "STARTCHAR ???\n";
        }
    }
}
close FSOURCE;
delete $char{-1};

shift @ARGV;
while ($#ARGV > 0) {
    $fmap = $ARGV[0];
    if ($ARGV[1] =~ /^([^-]+)-([^-]+)$/) {
	$registry = $1;
	$encoding = $2;
    } else {
	die("Argument registry-encoding '$ARGV[1]' not in expected format!\n");
    }

    shift @ARGV;
    shift @ARGV;

    # open and read source file
    open(FMAP,  "<$fmap")
	|| die ("Can't read mapping file '$fmap': $!\n");
    %map = ();
    while (<FMAP>) {
        next if /^\s*(\#.*)?$/;
        if (/^\s*(0[xX])?([0-9A-Fa-f]{2})\s+(0[xX]|U\+|U-)?([0-9A-Fa-f]{4})/) {
	    $target = hex($2);
	    $ucs = hex($4);
	    if (!is_control($ucs)) {
		if ($startchar{$ucs}) {
		    $map{$target} = $ucs;
		} else {
		    printf STDERR "No glyph for character U+%04X " .
			"(0x%02x) available.\n", $ucs, $target
			    unless (is_blockgraphics($ucs) && $slant ne "R") ||
				   ($ucs >= 0x200e && $ucs <= 0x200f);
		}
	    }
	} else {
	    printf STDERR "Unrecognized line in '$fmap':\n$_";
	}
    }
    close FMAP;
    
    # add default character
    if (!(defined($map{0}) && $startchar{$map{0}})) {
	if (defined($default_char) && $startchar{$default_char}) {
	    $map{0} = $default_char;
	    $startchar{$default_char} = "STARTCHAR defaultchar\n";
	} else {
	    printf STDERR "No default character defined.\n";
	}
    }
    
    if ($dec_chars ||
	((!(defined $dec_chars) && $slant eq 'R' && $spacing eq 'C'))) {
	# add DEC VT100 graphics characters in the range 1-31
	# (as expected by some old xterm versions)
	for $i (keys(%decmap)) {
	    if ($startchar{$decmap{$i}}) {
		$map{$i} = $decmap{$i};
	    } else {
		#printf STDERR "No glyph for character U+%04X " .
		#    "(0x%02x) available.\n", $decmap{$i}, $i;
	    }
	}
    }

    # list of characters that will be written out
    @chars = sort {$a <=> $b} keys(%map);
    if ($#chars < 0) {
	print STDERR "No characters found for $registry-$encoding.\n";
	next;
    };

    # find overal font bounding box
    undef @bbx;
    for $target (@chars) {
	$ucs = $map{$target};
	if ($char{$ucs} =~ /^BBX\s+(\d+)\s+(\d+)\s+(-?\d+)\s+(-?\d+)\s*$/m) {
	    if (defined @bbx) {
		@bbx = combine_bbx(@bbx, $1, $2, $3, $4);
	    } else {
		@bbx = ($1, $2, $3, $4);
	    }
	} else {
	    printf STDERR "Warning: No BBX found for U+%04X!\n", $ucs;
	}
    }

    # generate output file name
    if ($fsource =~ /^(.*).bdf$/i) {
	$fout = $1 . "-$registry-$encoding.bdf";
    } else {
	$fout = $fsource . "-$registry-$encoding";
    }
    $fout =~ s/^(.*\/)?([^\/]+)$/$2/;  # remove path prefix

    # write new BDF file
    printf STDERR "Writing %d characters into file '$fout'.\n", $#chars + 1;
    open(FOUT,  ">$fout")
	|| die ("Can't write file '$fout': $!\n");
    
    print FOUT $startfont;
    print FOUT "COMMENT AUTOMATICALLY GENERATED FILE. DO NOT EDIT!\n";
    print FOUT "COMMENT Generated with 'ucs2any.pl $fsource $fmap " .
	"$registry-$encoding'\n";
    print FOUT "COMMENT from an ISO10646-1 encoded source BDF font.\n";
    print FOUT "COMMENT ucs2any.pl by Markus Kuhn <mkuhn\@acm.org>, 2000.\n";
    $newheader = $header;
    $newheader =~
	s/^FONTBOUNDINGBOX\s+.*$/FONTBOUNDINGBOX @bbx/m
	    || print STDERR "Warning: FONTBOUNDINGBOX not fixed!\n";
    $newheader =~
	s/^FONT\s+(.*)-\w+-\w+\s*$/FONT $1-$registry-$encoding/m
	    || print STDERR "Warning: FONT property not fixed!\n";
    $newheader =~
	s/^CHARSET_REGISTRY\s+.*$/CHARSET_REGISTRY "$registry"/m
	    || print STDERR "Warning: CHARSET_REGISTRY not fixed!\n";
    $newheader =~
	s/^CHARSET_ENCODING\s+.*$/CHARSET_ENCODING "$encoding"/m
	    || print STDERR "Warning: CHARSET_ENCODING not fixed!\n";
    print FOUT $newheader;
    printf FOUT "CHARS %d\n", $#chars + 1;

    # Write characters
    for $target (@chars) {
	$ucs = $map{$target};
	print FOUT $startchar{$ucs};
	print FOUT "ENCODING $target\n";
	print FOUT $char{$ucs};
    }

    print FOUT "ENDFONT\n";

    close(FOUT);
}
