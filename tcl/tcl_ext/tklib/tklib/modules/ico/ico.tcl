# ico.tcl --
#
# Win32 ico manipulation code
#
# Copyright (c) 2003 Aaron Faupell
# Copyright (c) 2003-2004 ActiveState Corporation
#
# RCS: @(#) $Id: ico.tcl,v 1.10 2004/07/28 03:32:19 afaupell Exp $

# JH: speed has been considered in these routines, although they
# may not be fully optimized.  Running EXEtoICO on explorer.exe,
# which has nearly 100 icons, takes .2 secs on a P4/2.4ghz machine.
#

# Sample usage:
#	set file bin/wish.exe
#	set icos [::ico::getIconList $file]
#	set img  [::ico::getIcon $file 1 -format image]

package require Tcl 8.4

# Instantiate vars we need for this package
namespace eval ::ico {
    # don't look farther than this for icos past beginning or last ico found
    variable maxIcoSearch 32768; #16384 ; #32768

    # stores cached indices of icons found
    variable  ICONS
    array set ICONS {}

    # used for 4bpp number conversion
    variable BITS
    array set BITS [list {} 0 0000 0 0001 1 0010 2 0011 3 0100 4 \
			0101 5 0110 6 0111 7 1000 8 1001 9 \
			1010 10 1011 11 1100 12 1101 13 1110 14 1111 15 \
			\
			00000 00 00001 0F 00010 17 00011 1F \
			00100 27 00101 2F 00110 37 00111 3F \
			01000 47 01001 4F 01010 57 01011 5F \
			01100 67 01101 6F 01110 77 01111 7F \
			10000 87 10001 8F 10010 97 10011 9F \
			10100 A7 10101 AF 10110 B7 10111 BF \
			11000 C7 11001 CF 11010 D7 11011 DF \
			11100 E7 11101 EF 11110 F7 11111 FF]
}


# getIconList --
#
# List of icons in the file (each element a list of w h and bpp)
#
# ARGS:
#	file	File to extra icon info from.
#	?-type?	Type of file.  If not specified, it is derived from
#		the file extension.  Currently recognized types are
#		EXE, DLL, ICO and ICL
#
# RETURNS:
#	list of icons' dimensions as tuples {width height bpp}
#
proc ::ico::getIconList {file args} {
    parseOpts type $args
    if {![info exists type]} {
        # $type wasn't specified - get it from the extension
        set type [fileext $file]
    }
    if {![llength [info commands getIconList$type]]} {
	return -code error "unsupported file format $type"
    }
    getIconList$type [file normalize $file]
}

# getIcon --
#
# Get pixel data or image of icon @ index in file
#
# ARGS:
#	file		File to extra icon info from.
#	index		Index of icon in the file to use.  The ordering is the
#			same as returned by getIconList.  (0-based)
#	?-type?		Type of file.  If not specified, it is derived from
#			the file extension.  Currently recognized types are
#			EXE, DLL, ICO and ICL
#	?-format?	Output format. Must be one of "image" or "colors"
#			'image' will return the name of a Tk image.
#			'colors' will return a list of pixel values
#	?-name?		If output is image, use this as the name of Tk image
#			created
#
# RETURNS:
#	pixel data as a list that could be passed to 'image create'
#
proc ::ico::getIcon {file index args} {
    set name {}
    set format image
    parseOpts {type format name} $args
    if {![info exists type]} {
        # $type wasn't specified - get it from the extension
        set type [fileext $file]
    }
    if {![llength [info commands getRawIconData$type]]} {
        return -code error "unsupported file format $type"
    }
    # ICODATA is a pure data type - not a real file
    if {$type ne "ICODATA"} {
	set file [file normalize $file]
    }
    set colors [eval [linsert [getRawIconData$type $file $index] 0 getIconAsColorList]]
    if {$format eq "image"} {
        return [createImage $colors $name]
    }
    return $colors
}

# writeIcon --
#
# Overwrite write icon @ index in file of specific type with depth/pixel data
#
# ARGS:
#	file	File to extra icon info from.
#	index	Index of icon in the file to use.  The ordering is the
#		same as returned by getIconList.  (0-based)
#	bpp	bit depth of icon we are writing
#	data	Either pixel color data (as returned by getIcon -format color)
#		or the name of a Tk image.
#	?-type?	Type of file.  If not specified, it is derived from
#		the file extension.  Currently recognized types are
#		EXE, DLL, ICO and ICL
#
# RETURNS:
#	Tk image based on the specified icon
#
proc ::ico::writeIcon {file index bpp data args} {
    parseOpts type $args
    if {![info exists type]} {
        # $type wasn't specified - get it from the extension
        set type [fileext $file]
    }
    if {![llength [info commands writeIcon$type]]} {
	return -code error "unsupported file format $type"
    }
    if {[llength $data] == 1} {
        set data [getColorListFromImage $data]
    } elseif {[lsearch -glob [join $data] #*] > -1} {
        set data [translateColors $data]
    }
    if {$bpp != 1 && $bpp != 4 && $bpp != 8 && $bpp != 24 && $bpp != 32} {
	return -code error "invalid color depth"
    }
    set palette {}
    if {$bpp <= 8} {
	set palette [getPaletteFromColors $data]
	if {[lindex $palette 0] > (1 << $bpp)} {
	    return -code error "specified color depth too low"
	}
	set data  [lindex $palette 2]
	set palette [lindex $palette 1]
	append palette [string repeat \000 [expr {(1 << ($bpp + 2)) - [string length $palette]}]]
    }
    set and [getAndMaskFromColors $data]
    set xor [getXORFromColors $bpp $data]
    # writeIcon$type file index w h bpp palette xor and
    writeIcon$type [file normalize $file] $index \
	[llength [lindex $data 0]] [llength $data] $bpp $palette $xor $and
}


# copyIcon --
#
# Copies an icon directly from one file to another
#
# ARGS:
#	file	        File to extract icon info from.
#	index	        Index of icon in the file to use.  The ordering is the
#		        same as returned by getIconList.  (0-based)
#	?-fromtype?	Type of source file.  If not specified, it is derived from
#		        the file extension.  Currently recognized types are
#		        EXE, DLL, ICO and ICL
#	?-totype?	Type of destination file.  If not specified, it is derived from
#		        the file extension.  Currently recognized types are
#		        EXE, DLL, ICO and ICL
#
# RETURNS:
#	nothing
#
proc ::ico::copyIcon {f1 i1 f2 i2 args} {
    parseOpts {fromtype totype} $args
    if {![info exists fromtype]} {
        # $type wasn't specified - get it from the extension
        set fromtype [fileext $f1]
    }
    if {![info exists totype]} {
        # $type wasn't specified - get it from the extension
        set totype [fileext $f2]
    }
    if {![llength [info commands writeIcon$totype]]} {
	return -code error "unsupported file format $totype"
    }
    if {![llength [info commands getRawIconData$fromtype]]} {
	return -code error "unsupported file format $fromtype"
    }
    set src [getRawIconData$fromtype $f1 $i1]
    writeIcon $f2 $i2 [lindex $src 2] [eval getIconAsColorList $src] -type $totype
}

#
# transparentColor --
#
# Turns on transparency for all pixels in the Tk image that match the color
#
# ARGS:
#	img	        Name of the Tk image to modify
#	color	        Color in #hex format which will be made transparent
#
# RETURNS:
#	nothing
#
proc ::ico::transparentColor {img color} {
    package require Tk
    if {[string match "#*" $color]} {
	set color [scan $color "#%2x%2x%2x"]
    }
    set w [image width $img]
    set h [image height $img]
    for {set y 0} {$y < $h} {incr y} {
	for {set x 0} {$x < $w} {incr x} {
	    if {[$img get $x $y] eq $color} {$img transparency set $x $y 1}
	}
    }
}

#
# clearCache --
#
# Clears the cache of icon offsets
#
# ARGS:
#	file	optional filename
#
#
# RETURNS:
#	nothing
#
proc ::ico::clearCache {{file {}}} {
    variable ICONS
    if {$file ne ""} {
	array unset ICONS $file,*
    } else {
	unset ICONS
	array set ICONS {}
    }
}

#
# EXEtoICO --
#
# Convert all icons found in exefile into a regular icon file
#
# ARGS:
#	exeFile	        Input EXE filename
#	icoFile	        Output ICO filename
#
# RETURNS:
#	nothing
#
proc ::ico::EXEtoICO {exeFile icoFile} {
    variable ICONS

    set file [file normalize $exeFile]
    set fh   [checkEXE $file]
    set cnt  [SearchForIcos $file $fh]

    for {set i 0} {$i <= $cnt} {incr i} {
	set ico $ICONS($file,$i,data)
	seek $fh $ICONS($file,$i) start
	eval [list lappend dir] $ico
	append data [read $fh [eval calcSize $ico 40]]
    }
    close $fh

    # write them out to a file
    set ifh [open $icoFile w+]
    fconfigure $ifh -translation binary

    bputs $ifh sss 0 1 [expr {$cnt + 1}]
    set offset [expr {6 + (($cnt + 1) * 16)}]
    foreach {w h bpp} $dir {
	set colors 0
	if {$bpp <= 8} {set colors [expr {1 << $bpp}]}
	set s [calcSize $w $h $bpp 40]
	lappend fix $offset $s
	bputs $ifh ccccssii $w $h $colors 0 1 $bpp $s $offset
	set offset [expr {$offset + $s}]
    }
    puts -nonewline $ifh $data
    foreach {offset size} $fix {
	seek $ifh [expr {$offset + 20}] start
	bputs $ifh i $s
    }
    close $ifh
}

##
## Internal helper commands.
## Some may be appropriate for exposing later, but would need docs
## and make sure they "fit" in the API.
##

# gets the file extension as we use it internally (upper case, no '.')
proc ::ico::fileext {file} {
    return [string trimleft [string toupper [file extension $file]] .]
}

# helper proc to parse optional arguments to some of the public procs
proc ::ico::parseOpts {acc opts} {
    foreach {key val} $opts {
        set key [string trimleft $key -]
	if {[lsearch -exact $acc $key] >= 0} {
	    upvar $key $key
	    set $key $val
	} elseif {$key ne ""} {
	    return -code error "unknown option \"$key\": must be one of $acc"
	}
    }
}

# formats a single color from a binary decimal list format to the #hex format
proc ::ico::formatColor {r g b} {
    format "#%02X%02X%02X" [scan $r %c] [scan $g %c] [scan $b %c]
}

# translates a color list from the #hex format to the decimal list format
#                                #0000FF                  {0 0 255}
proc ::ico::translateColors {colors} {
    set new {}
    foreach line $colors {
	set tline {}
	foreach x $line {
	    if {$x eq ""} {lappend tline {}; continue}
	    lappend tline [scan $x "#%2x%2x%2x"]
	}
	set new [linsert $new 0 $tline]
    }
    return $new
}

proc ::ico::getdword {fh} {
    binary scan [read $fh 4] i* tmp
    return $tmp
}

proc ::ico::getword {fh} {
    binary scan [read $fh 2] s* tmp
    return $tmp
}

# binary puts
proc ::ico::bputs {fh format args} {
    puts -nonewline $fh [eval [list binary format $format] $args]
}

# creates a Tk image from a list of colors in the #hex format
proc ::ico::createImage {colors {name {}}} {
    package require Tk
    set h [llength $colors]
    set w [llength [lindex $colors 0]]
    if {$name ne ""} {
	set img [image create photo $name -width $w -height $h]
    } else {
	set img [image create photo -width $w -height $h]
    }
    if {0} {
	# if image supported "" colors as transparent pixels,
	# we could use this much faster op
	$img put -to 0 0 $colors
    } else {
	for {set x 0} {$x < $w} {incr x} {
	    for {set y 0} {$y < $h} {incr y} {
		set clr [lindex $colors $y $x]
		if {$clr ne ""} {
		    $img put -to $x $y $clr
		}
	    }
	}
    }
    return $img
}

# return a list of colors in the #hex format from raw icon data
# returned by readDIB
proc ::ico::getIconAsColorList {w h bpp palette xor and} {
    # Create initial empty color array that we'll set indices in
    set colors {}
    set row    {}
    set empty  {}
    for {set x 0} {$x < $w} {incr x} { lappend row $empty }
    for {set y 0} {$y < $h} {incr y} { lappend colors $row }

    set x 0
    set y [expr {$h-1}]
    if {$bpp == 1} {
	binary scan $xor B* xorBits
	foreach i [split $xorBits {}] a [split $and {}] {
	    if {$x == $w} { set x 0; incr y -1 }
	    if {$a == 0} {
		lset colors $y $x [lindex $palette $i]
	    }
	    incr x
	}
    } elseif {$bpp == 4} {
	variable BITS
	binary scan $xor B* xorBits
	set i 0
	foreach a [split $and {}] {
	    if {$x == $w} { set x 0; incr y -1 }
	    if {$a == 0} {
		set bits [string range $xorBits $i [expr {$i+3}]]
		lset colors $y $x [lindex $palette $BITS($bits)]
	    }
	    incr i 4
	    incr x
	}
    } elseif {$bpp == 8} {
	foreach i [split $xor {}] a [split $and {}] {
	    if {$x == $w} { set x 0; incr y -1 }
	    if {$a == 0} {
		lset colors $y $x [lindex $palette [scan $i %c]]
	    }
	    incr x
	}
    } elseif {$bpp == 16} {
        variable BITS
        binary scan $xor b* xorBits
        set i 0
	foreach a [split $and {}] {
	    if {$x == $w} { set x 0; incr y -1 }
	    if {$a == 0} {
		set b1 [string range $xorBits      $i        [expr {$i+4}]]
		set b2 [string range $xorBits [expr {$i+5}]  [expr {$i+9}]]
		set b3 [string range $xorBits [expr {$i+10}] [expr {$i+14}]]
		lset colors $y $x "#$BITS($b3)$BITS($b2)$BITS($b1)"
	    }
	    incr i 16
	    incr x
	}
    } elseif {$bpp == 24} {
	foreach {b g r} [split $xor {}] a [split $and {}] {
	    if {$x == $w} { set x 0; incr y -1 }
	    if {$a == 0} {
		lset colors $y $x [formatColor $r $g $b]
	    }
	    incr x
	}
    } elseif {$bpp == 32} {
	foreach {b g r n} [split $xor {}] a [split $and {}] {
	    if {$x == $w} { set x 0; incr y -1 }
	    if {$a == 0} {
		lset colors $y $x [formatColor $r $g $b]
	    }
	    incr x
	}
    }
    return $colors
}

# creates a binary formatted AND mask by reading a list of colors in the decimal list format
# and checking for empty colors which designate transparency
proc ::ico::getAndMaskFromColors {colors} {
    set and {}
    foreach line $colors {
	set l {}
	foreach x $line {append l [expr {$x eq ""}]}
	append l [string repeat 0 [expr {[string length $l] % 32}]]
	foreach {a b c d e f g h} [split $l {}] {
	    append and [binary format B8 $a$b$c$d$e$f$g$h]
	}
    }
    return $and
}

# creates a binary formatted XOR mask in the specified depth format from
# a list of colors in the decimal list format
proc ::ico::getXORFromColors {bpp colors} {
    set xor {}
    if {$bpp == 1} {
	foreach line $colors {
	    foreach {a b c d e f g h} $line {
		foreach x {a b c d e f g h} {
		    if {[set $x] == ""} {set $x 0}
		}
		binary scan $a$b$c$d$e$f$g$h bbbbbbbb h g f e d c b a
		append xor [binary format b8 $a$b$c$d$e$f$g$h]
	    }
	}
    } elseif {$bpp == 4} {
	foreach line $colors {
	    foreach {a b} $line {
		if {$a == ""} {set a 0}
		if {$b == ""} {set b 0}
		binary scan $a$b b4b4 b a
		append xor [binary format b8 $a$b]
	    }
	}
    } elseif {$bpp == 8} {
	foreach line $colors {
	    foreach x $line {
		if {$x == ""} {set x 0}
		append xor [binary format c $x]
	    }
	}
    } elseif {$bpp == 24} {
	foreach line $colors {
	    foreach x $line {
		if {![llength $x]} {
		    append xor [binary format ccc 0 0 0]
		} else {
		    foreach {a b c n} $x {
			append xor [binary format ccc $c $b $a]
		    }
		}
	    }
	}
    } elseif {$bpp == 32} {
	foreach line $colors {
	    foreach x $line {
		if {![llength $x]} {
		    append xor [binary format cccc 0 0 0 0]
		} else {
		    foreach {a b c n} $x {
			if {$n == ""} {set n 0}
			append xor [binary format cccc $c $b $a $n]
		    }
		}
	    }
	}
    }
    return $xor
}

# translates a Tk image into a list of colors in the #hex format
# one element per pixel and {} designating transparent
# used by writeIcon when writing from a Tk image
proc ::ico::getColorListFromImage {img} {
    package require Tk
    set w [image width $img]
    set h [image height $img]
    set r {}
    for {set y [expr $h - 1]} {$y > -1} {incr y -1} {
	set l {}
	for {set x 0} {$x < $w} {incr x} {
	    if {[$img transparency get $x $y]} {
		lappend l {}
	    } else {
		lappend l [$img get $x $y]
	    }
	}
	lappend r $l
    }
    return $r
}

# creates a palette from a list of colors in the decimal list format
# a palette consists of 3 values, the number of colors, the palette entry itself,
# and the color list transformed to point to palette entries instead of color names
# the palette entry itself is stored as 32bpp in "G B R padding" order
proc ::ico::getPaletteFromColors {colors} {
    set palette "\x00\x00\x00\x00"
    array set tpal {{0 0 0} 0}
    set new {}
    set i 1
    foreach line $colors {
	set tline {}
	foreach x $line {
	    if {$x eq ""} {lappend tline {}; continue}
	    if {![info exists tpal($x)]} {
		foreach {a b c n} $x {
		    append palette [binary format cccc $c $b $a 0]
		}
		set tpal($x) $i
		incr i
	    }
	    lappend tline $tpal($x)
	}
	lappend new $tline
    }
    return [list $i $palette $new]
}

# read a Device Independent Bitmap from the current offset, return:
#	{width height depth palette XOR_mask AND_mask}
proc ::ico::readDIB {fh} {
    binary scan [read $fh 16] x4iix2s w h bpp
    set h [expr {$h / 2}]
    seek $fh 24 current

    set palette [list]
    if {$bpp == 1 || $bpp == 4 || $bpp == 8} {
	set colors [read $fh [expr {1 << ($bpp + 2)}]]
	foreach {b g r x} [split $colors {}] {
	    lappend palette [formatColor $r $g $b]
	}
    } elseif {$bpp == 16 || $bpp == 24 || $bpp == 32} {
	# do nothing here
    } else {
	return -code error "unsupported color depth: $bpp"
    }

    set xor  [read $fh [expr {int(($w * $h) * ($bpp / 8.0))}]]
    set and1 [read $fh [expr {(($w * $h) + ($h * ($w % 32))) / 8}]]

    set and {}
    set row [expr {($w + abs($w - 32)) / 8}]
    set len [expr {$row * $h}]
    for {set i 0} {$i < $len} {incr i $row} {
	binary scan [string range $and1 $i [expr {$i + $row}]] B$w tmp
	append and $tmp
    }

    return [list $w $h $bpp $palette $xor $and]
}

# read a Device Independent Bitmap from raw data, return:
#	{width height depth palette XOR_mask AND_mask}
proc ::ico::readDIBFromData {data loc} {
    # Read info from location
    binary scan $data @${loc}x4iix2s w h bpp
    set h [expr {$h / 2}]
    # Move over w/h/bpp info + magic offset to start of DIB
    set cnt [expr {$loc + 16 + 24}]

    set palette [list]
    if {$bpp == 1 || $bpp == 4 || $bpp == 8} {
	# Could do: [binary scan $data @${cnt}c$len colors]
	# and iter over colors, but this is more consistent with $fh version
	set len    [expr {1 << ($bpp + 2)}]
	set colors [string range $data $cnt [expr {$cnt + $len - 1}]]
	foreach {b g r x} [split $colors {}] {
	    lappend palette [formatColor $r $g $b]
	}
	incr cnt $len
    } elseif {$bpp == 16 || $bpp == 24 || $bpp == 32} {
	# do nothing here
    } else {
	return -code error "unsupported color depth: $bpp"
    }

    # Use -1 to account for string range inclusiveness
    set end  [expr {$cnt + int(($w * $h) * ($bpp / 8.0)) - 1}]
    set xor  [string range $data $cnt $end]
    set and1 [string range $data [expr {$end + 1}] \
		  [expr {$end + ((($w * $h) + ($h * ($w % 32))) / 8) - 1}]]

    set and {}
    set row [expr {($w + abs($w - 32)) / 8}]
    set len [expr {$row * $h}]
    for {set i 0} {$i < $len} {incr i $row} {
	# Has to be decoded by row, in order
	binary scan [string range $and1 $i [expr {$i + $row}]] B$w tmp
	append and $tmp
    }

    return [list $w $h $bpp $palette $xor $and]
}

proc ::ico::getIconListICO {file} {
    set fh [open $file r]
    fconfigure $fh -translation binary

    # both words must be read to keep in sync with later reads
    if {"[getword $fh] [getword $fh]" ne "0 1"} {
	return -code error "not an icon file"
    }
    set num [getword $fh]
    set r {}
    for {set i 0} {$i < $num} {incr i} {
	set info {}
	lappend info [scan [read $fh 1] %c] [scan [read $fh 1] %c]
	set bpp [scan [read $fh 1] %c]
	if {$bpp == 0} {
	    set orig [tell $fh]
	    seek $fh 9 current
	    seek $fh [expr {[getdword $fh] + 14}] start
	    lappend info [getword $fh]
	    seek $fh $orig start
	} else {
	    lappend info [expr {int(sqrt($bpp))}]
	}
	lappend r $info
	seek $fh 13 current
    }
    close $fh
    return $r
}

proc ::ico::getIconListICODATA {data} {
    if {[binary scan $data sss h1 h2 num] != 3 || $h1 != 0 || $h2 != 1} {
	return -code error "not icon data"
    }
    set r {}
    set cnt 6
    for {set i 0} {$i < $num} {incr i} {
	if {[binary scan $data @${cnt}ccc w h bpp] != 3} {
	    return -code error "error decoding icon data"
	}
	incr cnt 3
	set info [list $w $h]
	if {$bpp == 0} {
	    set off [expr {$cnt + 9}]
	    binary scan $data @${off}i off
	    incr off 14
	    binary scan $data @${off}s bpp
	    lappend info $bpp
	} else {
	    lappend info [expr {int(sqrt($bpp))}]
	}
	lappend r $info
	incr cnt 13
    }
    return $r
}

# returns an icon in the form:
#	{width height depth palette xor_mask and_mask}
proc ::ico::getRawIconDataICO {file index} {
    set fh [open $file r]
    fconfigure $fh -translation binary

    # both words must be read to keep in sync with later reads
    if {"[getword $fh] [getword $fh]" ne "0 1"} {
        close $fh
	return -code error "not an icon file"
    }
    if {$index < 0 || $index >= [getword $fh]} {
	return -code error "index out of range"
    }

    seek $fh [expr {(16 * $index) + 12}] current
    seek $fh [getdword $fh] start

    # readDIB returns: {w h bpp palette xor and}
    set dib [readDIB $fh]

    close $fh
    return $dib
}

proc ::ico::getRawIconDataICODATA {data index} {
    if {[binary scan $data sss h1 h2 num] != 3 || $h1 != 0 || $h2 != 1} {
	return -code error "not icon data"
    }
    if {$index < 0 || $index >= $num} {
	return -code error "index out of range: must be between 0 and $num"
    }
    # Move to ico location
    set cnt [expr {6 + (16 * $index) + 12}]
    binary scan $data @${cnt}i loc

    # readDIB returns: {w h bpp palette xor and}
    set dib [readDIBFromData $data $loc]

    return $dib
}

proc ::ico::writeIconICO {file index w h bpp palette xor and} {
    if {![file exists $file]} {
	set fh [open $file w+]
	fconfigure $fh -translation binary
	bputs $fh sss 0 1 0
	seek $fh 0 start
    } else {
	set fh [open $file r+]
	fconfigure $fh -translation binary
    }
    if {[file size $file] > 4 && "[getword $fh] [getword $fh]" ne "0 1"} {
	close $fh
	return -code error "not an icon file"
    }
    set num [getword $fh]
    if {$index eq "end"} { set index $num }
    if {$index < 0 || $index > $num} {
	close $fh
	return -code error "index out of range"
    }
    set colors 0
    if {$bpp <= 8} {set colors [expr {1 << $bpp}]}
    set size [expr {[string length $palette] + [string length $xor] + [string length $and]}]

    # if we are adding a new icon at the end
    if {$index == $num} {
        # increment the icon count
	seek $fh -2 current
	bputs $fh s [expr {$num + 1}]
	# save all the data past the icon dir entries
	seek $fh [expr {$num * 16}] current
	set olddata [read $fh]
	# increment all the offsets in the existing dir entries by 16 to account for our new entry
	set cur 0
	while {$cur < $num} {
	    seek $fh [expr {($cur * 16) + 18}] start
	    set toff [getdword $fh]
	    seek $fh -4 current
	    bputs $fh i [expr {$toff + 16}]
	    incr cur
	}
	# insert new icon dir entry
	bputs $fh ccccss $w $h $colors 0 0 $bpp
	bputs $fh ii [expr {$size + 40}] [expr {[string length $olddata] + [tell $fh] + 8}]
	# put all the icon data back
	puts -nonewline $fh $olddata
	# put our new icon at the end
	bputs $fh iiissiiiiii 40 $w [expr {$h * 2}] 1 $bpp 0 $size 0 0 0 0
	puts -nonewline $fh $palette
	puts -nonewline $fh $xor
	puts -nonewline $fh $and
    } else {
        # we are overwriting an icon - not necesarily the same size
        # get existing icon offset and length
	seek $fh [expr {($index * 16) + 8}] current
	set len [getdword $fh]
	set offset [getdword $fh]
	# adjust offset in existing icon dir entries higher than our new icon to account
	# for new icon length
	set cur [expr {$index + 1}]
	while {$cur < $num} {
	    seek $fh [expr {($cur * 16) + 18}] start
	    set toff [getdword $fh]
	    seek $fh -4 current
	    bputs $fh i [expr {$toff + (($size + 40) - $len)}]
	    incr cur
	}
	# save all data after new icon
	seek $fh [expr {$offset + $len}] start
	set olddata [read $fh]
	# overwrite icon dir entry
	seek $fh [expr {($index * 16) + 6}] start
	bputs $fh ccccssi $w $h $colors 0 0 $bpp [expr {$size + 40}]
	# insert new icon and saved data
	seek $fh $offset start
	bputs $fh iiissiiiiii 40 $w [expr {$h * 2}] 1 $bpp 0 $size 0 0 0 0
	puts -nonewline $fh $palette
	puts -nonewline $fh $xor
	puts -nonewline $fh $and
	puts -nonewline $fh $olddata
    }
    close $fh
}

# checks if file is a windows executable and returns an open file handle at the start of the data segment
proc ::ico::checkEXE {exe {mode r}} {
    set fh [open $exe $mode]
    fconfigure $fh -translation binary

    # verify PE header
    if {[read $fh 2] ne "MZ"} {
	close $fh
	return -code error "not a DOS executable"
    }
    seek $fh 60 start
    seek $fh [getword $fh] start
    set sig [read $fh 4]
    if {$sig eq "PE\000\000"} {
	# move past header data
	seek $fh 24 current
	seek $fh [getdword $fh] start
    } elseif {[string match "NE*" $sig]} {
	seek $fh 34 current
	seek $fh [getdword $fh] start
    } else {
	close $fh
	return -code error "executable header not found"
    }

    # return file handle
    return $fh
}

# calculate byte size of an icon.
# often passed $w twice because $h is double $w in the binary data
proc ::ico::calcSize {w h bpp {offset 0}} {
    set s [expr {int(($w*$h) * ($bpp/8.0)) \
		     + ((($w*$h) + ($h*($w%32)))/8) + $offset}]
    if {$bpp <= 8} { set s [expr {$s + (1 << ($bpp + 2))}] }
    return $s
}

# find all the icons in an executable and cache their size and offsets
proc ::ico::SearchForIcos {file fh {index -1}} {
    variable ICONS	  ; # stores icos offsets by index, and [list w h bpp]
    variable maxIcoSearch ; # don't look farther than this for icos
    set readsize 512	  ; # chunked read size

    if {[info exists ICONS($file,$index)]} {
	return $ICONS($file,$index)
    }

    set last   0 ; # tell point of last ico found
    set idx   -1 ; # index of icos found
    set pos    0
    set offset [tell $fh]
    set data   [read $fh $readsize]
    set lastoffset $offset

    while {1} {
	if {$pos > ($readsize - 20)} {
	    if {[eof $fh] || ($last && ([tell $fh]-$last) >= $maxIcoSearch)} {
		# set the -1 index to indicate we've read the whole file
		set ICONS($file,-1) $idx
		break
	    }

	    seek $fh [expr {$pos - $readsize}] current
	    set offset [tell $fh]

	    if {$offset <= $lastoffset} {
		# We made no progress (anymore). This means that we
		# have reached the end of the file and processed a
		# short block of 16 byte. And that we are now trying
		# to read and process the same block again. Squashing
		# the infinite loop just starting up right now.

		set ICONS($file,-1) $idx
		break
	    }
	    set lastoffset $offset

	    set pos 0
	    set data [read $fh $readsize]
	}

	binary scan [string range $data $pos [expr {$pos + 20}]] \
	    iiissi s w h p bpp comp
	if {$s == 40 && $p == 1 && $comp == 0 && $w == ($h / 2)} {
	    set ICONS($file,[incr idx]) [expr {$offset + $pos}]
	    set ICONS($file,$idx,data)	[list $w $w $bpp]
	    # stop if we found requested index
	    if {$index >= 0 && $idx == $index} { break }
	    incr pos [calcSize $w $w $bpp 40]
	    set last [expr {$offset + $pos}]
	} else {
	    incr pos 4
	}
    }
    return $idx
}

proc ::ico::getIconListEXE {file} {
    variable ICONS

    set file [file normalize $file]
    set fh   [checkEXE $file]
    set cnt  [SearchForIcos $file $fh]

    set icons [list]
    for {set i 0} {$i <= $cnt} {incr i} {
	lappend icons $ICONS($file,$i,data)
    }

    close $fh
    return $icons
}

# returns an icon in the form:
#	{width height depth palette xor_mask and_mask}
proc ::ico::getRawIconDataEXE {file index} {
    variable ICONS

    set file [file normalize $file]
    set fh   [checkEXE $file]
    set cnt  [SearchForIcos $file $fh $index]

    if {$cnt < $index} { return -code error "index out of range" }

    seek $fh $ICONS($file,$index) start

    # readDIB returns: {w h bpp palette xor and}
    set dib [readDIB $fh]
    close $fh
    return $dib
}

proc ::ico::writeIconEXE {file index w h bpp palette xor and} {
    variable ICONS

    set file [file normalize $file]
    set fh   [checkEXE $file r+]
    set cnt  [SearchForIcos $file $fh $index]

    if {$index eq "end"} {set index $cnt}
    if {$cnt < $index} { return -code error "index out of range" }

    if {[list $w $h $bpp] != $ICONS($file,$index,data)} {
	close $fh
	return -code error "icon format differs from original"
    }

    seek $fh [expr {$ICONS($file,$index) + 40}] start

    puts -nonewline $fh $palette
    puts -nonewline $fh $xor
    puts -nonewline $fh $and
    close $fh
}

interp alias {} ::ico::getIconListDLL    {} ::ico::getIconListEXE
interp alias {} ::ico::getRawIconDataDLL {} ::ico::getRawIconDataEXE
interp alias {} ::ico::writeIconDLL      {} ::ico::writeIconEXE
interp alias {} ::ico::getIconListICL    {} ::ico::getIconListEXE
interp alias {} ::ico::getRawIconDataICL {} ::ico::getRawIconDataEXE
interp alias {} ::ico::writeIconICL      {} ::ico::writeIconEXE

proc ::ico::showaux {files} {
    if {[llength $files]} {
	set file [lindex $files 0]
	Show $f
	update
	after 50 [list ::ico::showaux [lrange $files 1 end]]
    }
}

# Application level command: Find icons in a file and show them.
proc ::ico::Show {file args} {
    package require BWidget
    
    set parent .
    parseOpts {type parent} $args
    if {![info exists type]} {
        # $type wasn't specified - get it from the extension
        set type [fileext $file]
    }

    set file [file normalize $file]
    set icos  [getIconList $file -type $type]
    set wname [string map {. _ : _} $file]

    if {$parent eq "."} { set w ""} else { set w $parent }

    set mf $w.mainsw
    if {![winfo exists $mf]} {
	set sw [ScrolledWindow $mf]
	set sf [ScrollableFrame $mf.sf -constrainedwidth 1]
	$sw setwidget $sf
	pack $sw -fill both -expand 1
	grid columnconfigure [$mf.sf getframe] 0 -weight 1
    }
    set mf [$mf.sf getframe]

    set lf $mf.f$wname
    if {[winfo exists $lf]} { destroy $lf }
    if {![llength $icos]} {
	label $lf -text "No icons in '$file'" -anchor w
	grid $lf -sticky ew
    } else {
	labelframe $lf -text "[llength $icos] Icons in '$file'"
	grid $lf -sticky news
	set sw [ScrolledWindow $lf.sw$wname]
	set sf [ScrollableFrame $lf.sf$wname -constrainedheight 1 -height 70]
	$sw setwidget $sf
	set sf [$sf getframe]
	pack $sw -fill both -expand 1
	set col 0
	for {set x 0} {$x < [llength $icos]} {incr x} {
	    # catch in case theres any icons with unsupported color
	    if {[catch {getIcon $file $x -type $type} img]} {
		set txt "ERROR: $img"
		set lbl [label $sf.lbl$wname-$x -anchor w -text $txt]
		grid $lbl -sticky s -row 0 -column [incr col]
	    } else {
		set txt [eval {format "$x: %sx%s %sbpp"} [lindex $icos $x]]
		set lbl [label $sf.lbl$wname-$x -anchor w -text $txt \
			     -compound top -image $img]
		grid $lbl -sticky s -row 0 -column [incr col]
	    }
	    update idletasks
	}
    }
    grid rowconfigure $parent 0 -weight 1
    grid columnconfigure $parent 0 -weight 1
}

package provide ico 0.3
