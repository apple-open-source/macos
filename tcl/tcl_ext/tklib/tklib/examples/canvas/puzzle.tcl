#!/bin/sh
# -*- tcl -*- \
exec tclsh "$0" ${1+"$@"}
# ### ### ### ######### ######### #########

## DEMO. Slice image into tiles and show them, in order, or randomly
##       shuffled. Image can be provided as argument, or uses the
##       'morgens.jpg' from the example directory as default. Accepts
##       jpeg and png images.

# ### ### ### ######### ######### #########
## For data files found relative to the example's location.

set selfdir [file dirname [file normalize [info script]]]

## Ideas: It should be possible to get feedback on mouse clicks and
## use that to let the user swaps cells, until the shown image is
## restored to order.

# ### ### ### ######### ######### #########
## Other requirements for this example.

package require Tk
package require widget::scrolledwindow
package require canvas::sqmap
package require crosshair
package require img::jpeg
package require img::png

package require snit             ; # canvas::sqmap dependency
package require uevent::onidle   ; # ditto
package require cache::async 0.2 ; # ditto

# ### ### ### ######### ######### #########

proc Init {} {
    global argv tile scrollw scrollh basepicks maxw maxh

    set image [lindex $argv 0]
    if {$image eq ""} {
	set image [file join [file dirname [file normalize [info script]]] morgens.jpg]
    }
    set image [image create photo -file $image]

    set scrollw    [image width  $image]
    set scrollh    [image height $image]
    set tile 256

    set maxh 0
    for {set y 0} {$y < $scrollh} {incr y $tile} {
	set y1 $y ; incr y1 $tile
	if {$y1 > $scrollh} { set y1 $scrollh }
	set maxw 0
	for {set x 0} {$x < $scrollw} {incr x $tile} {
	    set x1 $x ; incr x1 $tile
	    if {$x1 > $scrollw} { set x1 $scrollh }

	    set parcel [image create photo -height $tile -width $tile]
	    $parcel copy $image -from $x $y $x1 $y1
	    lappend basepicks $parcel
	    incr maxw
	}
	incr maxh
    }

    image delete $image

    InitPicksUnordered
    return
}

# ### ### ### ######### ######### #########

set location {}

proc GUI {} {
    global tile scrollw scrollh

    widget::scrolledwindow .sw
    canvas::sqmap          .map
    button                 .exit -command exit    -text Exit
    button                 .shfl -command Shuffle -text Shuffle
    button                 .ord  -command Order   -text Original
    entry                  .loc  -textvariable location \
	-bd 2 -relief sunken -bg white -width 40

    .sw setwidget .map

    # Panning via mouse
    bind .map <ButtonPress-2> {%W scan mark   %x %y}
    bind .map <B2-Motion>     {%W scan dragto %x %y}

    # Cross hairs ...
    .map configure -cursor tcross
    crosshair::crosshair .map -width 0 -fill \#999999 -dash {.}
    crosshair::track on  .map TRACK

    #.map configure -grid-show-borders 1 ;# This leaks items = memory
    if 0 {
	# This routes the requests and results through GOT/GET logging
	# commands.
	.map configure \
	    -grid-cell-command GET \
	    -grid-cell-width  $tile \
	    -grid-cell-height $tile \
	    -scrollregion [list 0 0 $scrollw $scrollh]
    } else {
	# This routes the requests directly to the grid provider, and
	# results back.
	.map configure \
	    -grid-cell-command Pick \
	    -grid-cell-width  $tile \
	    -grid-cell-height $tile \
	    -scrollregion [list 0 0 $scrollw $scrollh]
    }

    pack .sw    -expand 1 -fill both -side bottom
    pack .exit  -expand 0 -fill both -side left
    pack .shfl  -expand 0 -fill both -side left
    pack .ord   -expand 0 -fill both -side left
    pack .loc   -expand 0 -fill both -side left

    return
}

proc Shuffle {} {
    InitPicksUnordered
    .map flush
    return
}

proc Order {} {
    InitPicksOrdered
    .map flush
    return
}

proc InitPicksUnordered {} {
    global picks basepicks order
    set picks [shuffle5a $basepicks]
    set order 0
    return
}

proc InitPicksOrdered {} {
    global picks basepicks order
    set picks $basepicks
    set order 1
    return
}

# ### ### ### ######### ######### #########
# Basic callback structure, log for logging, facade to transform the
# cache/tiles result into what xcanvas is expecting.

proc GET {__ at donecmd} {
    puts "GET ($at) ($donecmd)"
    Pick get $at [list GOT $donecmd]
    return
}

proc GOT {donecmd what at args} {
    puts "\tGOT $donecmd $what ($at) $args"
    if {[catch {
	uplevel #0 [eval [linsert $args 0 linsert $donecmd end $what $at]]
    }]} { puts $::errorInfo }
    return
}

# ### ### ### ######### ######### #########

proc TRACK {win x y args} {
    # args = viewport, pixels, see also xcanvas, SetPixelView.
    global location
    set location "@ $x, $y"
    return
}

# ### ### ### ######### ######### #########

proc Pick {__ at donecmd} {
    global picks image order maxw maxh
    if {[llength $picks]} {
	if {$order} {
	    foreach {r c} $at break
	    set i [expr {$c + ($r * $maxw)}]
	    set choice [lindex $picks $i]
	} else {
	    set choice [lindex $picks end]
	    set picks  [lreplace [K $picks [unset picks]] end end]
	}
	uplevel #0 [linsert $donecmd end set $at $choice]
    } else {
	uplevel #0 [linsert $donecmd end unset $at]
    }
    return
}

proc shuffle5a { list } {
    set n 1
    set slist {}
    foreach item $list {
	set index [expr {int(rand()*$n)}]
	set slist [linsert [K $slist [set slist {}]] $index $item]
	incr n
    }
    return $slist
}

proc K { x y } { set x }

# ### ### ### ######### ######### #########
## Basic interface.

Init
GUI
