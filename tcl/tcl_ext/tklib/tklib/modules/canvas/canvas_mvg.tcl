# *- tcl -*-
# ### ### ### ######### ######### #########

# Copyright (c) 2010 Wolf-Dieter Busch
# Origin http://wiki.tcl.tk/26859 [23-08-2010]
# OLL licensed (http://wiki.tcl.tk/10892).

# ### ### ### ######### ######### #########
## Requisites

package require Tcl 8.5
package require Tk  8.5

namespace eval ::canvas {}

# ### ### ### ######### ######### #########
## Implementation.

proc ::canvas::mvg {canvas} {

    #raise [winfo toplevel $canvas] 
    #update

    # Initialize drawing state... This array is keyed by the MVG
    # commands for the attribute, not by the canvas options, and not
    # by something third.
    array set mode {
	fill            {}
	stroke          {}
	stroke-width    {}
	stroke-linejoin {}
	stroke-linecap  {}
	font            {}
	font-size       {}
    }

    # Get the bounding box of all item, and compute the translation
    # required to put the lower-left corner at the origin.
    set dx 0
    set dy 0
    set box [$canvas bbox {*}[$canvas find all]]
    lassign $box zx zy ex ey
    if {$zx < 0} { set dx [expr {- $zx}] ; set ex [expr {$ex + $dx}] }
    if {$zy < 0} { set dy [expr {- $zy}] ; set ey [expr {$ey + $dy}] }
    set box [list 0 0 $ex $ey]

    # Standard prelude...
    mvg::Emit [list viewbox {*}$box]
    mvg::EmitChanged stroke none
    mvg::EmitChanged fill   [mvg::Col2Hex $canvas]
    mvg::Emit [list rectangle {*}$box]

    # Introspect the canvas, i.e. convert each item to MVG
    foreach item [$canvas find all] {
	set type [$canvas type $item]

	# Info to help debugging...
	mvg::Emit "# $type ... [$canvas gettags $item]"

	# Dump the item's attributes, as they are supported by it.
	# Note how the code is not sliced by item type which then
	# handles each of its attributes, but by attribute name, which
	# then checks if the type of the current item supports it.

	# Further note that the current attribute state is stored in
	# the mode array and actually emitted if and only if it is
	# different from the previously drawn state. This optimizes
	# the number of commands needed to set the drawing state for a
	# particular item.

	# outline width
	if {$type in {polygon oval arc rectangle line}} then {
	    mvg::EmitValue $item -width stroke-width
	}

	# fill, stroke
	if {$type in {polygon oval arc rectangle}} {
	    mvg::EmitColor $item -fill    fill
	    mvg::EmitColor $item -outline stroke
	}

	# joinstyle
	if {$type in {polygon}} then {
	    mvg::EmitValue $item -joinstyle stroke-linejoin
	}

	# line color, capstyle
	if {$type in {line}} then {
	    mvg::EmitChanged fill none
	    mvg::EmitColor $item -fill     stroke
	    mvg::EmitCap   $item -capstyle stroke-linecap
	}

	# text color, font, size
	if {$type in {text}} then {
	    # Compute font-family, font-size
	    set font [$canvas itemcget $item -font]
	    if {$font in [font names]} {
		set fontsize   [font configure $font -size]
		set fontfamily [font configure $font -family]
	    } else {
		if {[llength $font] == 1} then {
		    set fontsize 12
		} else {
		    set fontsize [lindex $font 1]
		}
		set fontfamily [lindex $font 0]
	    }
	    if {$fontsize < 0} {
		set fontsize [expr {int(-$fontsize / [tk scaling])}]
	    }

	    mvg::EmitChanged stroke none
	    mvg::EmitColor $item -fill fill
	    mvg::EmitChanged font-size $fontsize
	    mvg::EmitChanged font $fontfamily

	    #
	    # Attention! In some cases ImageMagick assumes 72dpi where
	    # 90dpi is necessary. If that happens use the switch
	    # -density to force the correct dpi setting, like %
	    # convert -density 90 test.mvg test.png
	    #
	    # Attention! Make sure that ImageMagick has access to the
	    # used fonts. If it has not, an error msg will be shown,
	    # and then switches silently to the default font.
	    #
	}

	# After the attributes we can emit the command actually
	# drawing the item, in the its place.

	set line {}
	set coords [mvg::Translate [$canvas coords $item]]

	switch -exact -- $type {
	    line {
		# start of path
		lappend line path 'M

		# smooth can be any boolean value, plus the name of a
		# line smoothing method. Core supports only 'raw'.
		# This however is extensible through packages.

		switch -exact -- [mvg::Smooth $item] {
		    0 {
			lappend line {*}[lrange $coords 0 1] L {*}[lrange $coords 2 end]
		    }
		    1 {
			if {[$canvas itemcget $item -arrow] eq "none"} {
			    lappend line {*}[mvg::Spline2MVG $coords]
			} else {
			    lappend line {*}[mvg::Spline2MVG $coords false]
			}
		    }
		    2 {
			lappend line {*}[lrange $coords 0 1] C {*}[lrange $coords 2 end]
		    }
		}

		append line '
		mvg::Emit $line
	    }
	    polygon {
		# start of path.
		lappend line path 'M

		switch -exact -- [mvg::Smooth $item] {
		    0 {
			lassign $coords x0 y0
			lassign [lrange $coords end-1 end] x1 y1
			set x [expr {($x0+$x1)/2.0}]
			set y [expr {($y0+$y1)/2.0}]
			lappend line $x $y L {*}$coords $x $y Z
		    }
		    1 {
			lassign $coords x0 y0
			lassign [lrange $coords end-1 end] x1 y1
			if {($x0 != $x1) || ($y0 != $y1)} {
			    lappend coords {*}[lrange $coords 0 1]
			}
			lappend line {*}[mvg::Spline2MVG $coords]
		    }
		    2 {
			lappend line {*}[lrange $coords 0 1] C {*}[lrange $coords 2 end]
		    }
		}

		append line '
		mvg::Emit $line
	    }
	    oval {
		lassign $coords x0 y0 x1 y1
		set xc [expr {($x0+$x1)/2.0}]
		set yc [expr {($y0+$y1)/2.0}]

		mvg::Emit [list ellipse $xc $yc [expr {$x1-$xc}] [expr {$y1-$yc}] 0 360]
	    }
	    arc {
		lassign $coords x0 y0 x1 y1

		set rx [expr {($x1-$x0)/2.0}]
		set ry [expr {($y1-$y0)/2.0}]
		set x  [expr {($x0+$x1)/2.0}]
		set y  [expr {($y0+$y1)/2.0}]
		set f  [expr {acos(0)/90}]

		set start  [$canvas itemcget $item -start]
		set startx [expr {cos($start*$f)*$rx+$x}]
		set starty [expr {sin(-$start*$f)*$ry+$y}]
		set angle  [expr {$start+[$canvas itemcget $item -extent]}]
		set endx   [expr {cos($angle*$f)*$rx+$x}]
		set endy   [expr {sin(-$angle*$f)*$ry+$y}]

		# start path
		lappend line path 'M
		# start point
		lappend line $startx $starty
		lappend line A
		# radiusx, radiusy
		lappend line $rx $ry
		# angle -- always 0
		lappend line 0
		# "big" or "small"?
		lappend line [expr {($angle-$start) > 180}]
		# right side (always)
		lappend line 0
		# end point
		lappend line $endx $endy
		# close path
		lappend line L $x $y Z
		append line '

		mvg::Emit $line
	    }
	    rectangle {
		mvg::Emit [list rectangle {*}$coords]
	    }
	    text {
		lassign [mvg::Translate [$canvas bbox $item]] x0 y0 x1 y1
		mvg::Emit "text $x0 $y1 '[$canvas itemcget $item -text]'"
	    }
	    image - bitmap {
		set img  [$canvas itemcget $item -image]
		set file [$img cget -file]
		lassign  [mvg::Translate [$canvas bbox $item]] x0 y0
		mvg::Emit "image over $x0 $y0 0 0 '$file'"
	    }
	    default {
		set    line "# not yet done:"
		append line " "  [$canvas type $item]
		append line " "  [mvg::Translate [$canvas coords $item]]
		append line " (" [$canvas gettags $item] ")"
		mvg::Emit $line
	    }
	}
    }

    # At last, return the fully assembled snapshot
    return [join $result \n]
}

# ### ### ### ######### ######### #########
## Helper commands. Internal.

namespace eval ::canvas::mvg {}

proc ::canvas::mvg::Translate {coords} {
    upvar 1 dx dx dy dy
    set tmp {}
    foreach {x y} $coords {
	lappend tmp [expr {$x + $dx}] [expr {$y + $dy}]
    }
    return $tmp
}


proc ::canvas::mvg::Smooth {item} {
    upvar 1 canvas canvas

    # Force smooth to canonical values we can then switch on.
    set smooth [$canvas itemcget $item -smooth]
    if {[string is boolean $smooth]} {
	if {$smooth} {
	    return 1
	} else {
	    return 0
	}
    } else {
	return 2
    }
}

proc ::canvas::mvg::EmitValue {item option cmd} {
    upvar 1 mode mode result result canvas canvas

    EmitChanged $cmd \
	[$canvas itemcget $item $option]
    return
}

proc ::canvas::mvg::EmitColor {item option cmd} {
    upvar 1 mode mode result result canvas canvas

    EmitChanged $cmd \
	[Col2Hex [$canvas itemcget $item $option]]
    return
}

proc ::canvas::mvg::EmitCap {item option cmd} {
    upvar 1 mode mode result result canvas canvas

    EmitChanged $cmd \
	[dict get {
	    butt       butt
	    projecting square
	    round      round
	} [$canvas itemcget $item $option]]
    return
}

proc ::canvas::mvg::EmitChanged {cmd value} {
    upvar 1 mode mode result result

    if {$mode($cmd) eq $value} return
    set mode($cmd) $value
    Emit [list $cmd $value]
    return
}

proc ::canvas::mvg::Emit {command} {
    upvar 1 result result
    lappend result $command
    return
}

proc ::canvas::mvg::Col2Hex {color} {
    # This command or similar functionality we might have somewhere
    # in tklib already ...

    # Special handling of canvas widgets, use their background color.
    if {[winfo exists $color] && [winfo class $color] eq "Canvas"} {
	set color [$color cget -bg]
    }
    if {$color eq ""} {
	return none
    }
    set result #
    foreach x [winfo rgb . $color] {
	append result [format %02x [expr {int($x / 256)}]]
    }
    return $result
}

proc ::canvas::mvg::Spline2MVG {coords {canBeClosed yes}} {
    set closed [expr {$canBeClosed &&
		      [lindex $coords 0] == [lindex $coords end-1] &&
		      [lindex $coords 1] == [lindex $coords end]}]

    if {$closed} {
	lassign [lrange $coords end-3 end] x0 y0 x1 y1

	set x [expr {($x0+$x1)/2.0}]
	set y [expr {($y0+$y1)/2.0}]

	lset coords end-1 $x
	lset coords end $y

	set coords [linsert $coords 0 $x $y]
    }

    if {[llength $coords] != 6} {
	lappend tmp {*}[lrange $coords 0 1]

	set co1 [lrange $coords 2 end-4]
	set co2 [lrange $coords 4 end-2]

	foreach {x1 y1} $co1 {x2 y2} $co2 {
	    lappend tmp $x1 $y1 [expr {($x1+$x2)/2.0}] [expr {($y1+$y2)/2.0}]
	}
	lappend tmp {*}[lrange $coords end-3 end]
	set coords $tmp
    }

    return [lreplace $coords 2 1 Q]
}

# ### ### ### ######### ######### #########
## Ready

package provide canvas::mvg 1
return
