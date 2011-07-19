#!/bin/sh
# -*- tcl -*- \
exec tclsh "$0" ${1+"$@"}
# ### ### ### ######### ######### #########

## DEMO. Show pseudo-city map using semi-random (*) street tiles.
##       (*) Random + restrictions about what tiles can be neighbours.
##           This part in citygrid.tcl

# ### ### ### ######### ######### #########
## For data files found relative to the example's location.

set selfdir  [file dirname [file normalize [info script]]]

source $selfdir/citygrid.tcl

# ### ### ### ######### ######### #########
## Other requirements for this example.

package require Tk
package require widget::scrolledwindow
package require canvas::sqmap
package require crosshair

package require struct::set      ; # citygrid.tcl
package require snit             ; # canvas::sqmap dependency
package require uevent::onidle   ; # ditto
package require cache::async 0.2 ; # ditto

# ### ### ### ######### ######### #########

set location {}

proc GUI {} {
    widget::scrolledwindow .sw
    canvas::sqmap          .map
    button                 .exit -command exit    -text Exit
    button                 .shfl -command Shuffle -text Shuffle
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

    set tile [city::tile]
    set city [expr {$tile * 64}]

    #.map configure -grid-show-borders 1 ;# This leaks items = memory
    if 0 {
	# This routes the requests and results through GOT/GET logging
	# commands.
	.map configure \
	    -grid-cell-command GET \
	    -grid-cell-width  $tile \
	    -grid-cell-height $tile \
	    -scrollregion [list 0 0 $city $city]
    } else {
	# This routes the requests directly to the grid provider, and
	# results back.
	.map configure \
	    -grid-cell-command ::city::grid \
	    -grid-cell-width  $tile \
	    -grid-cell-height $tile \
	    -scrollregion [list 0 0 $city $city]
    }

    pack .sw    -expand 1 -fill both -side bottom
    pack .exit  -expand 0 -fill both -side left
    pack .shfl  -expand 0 -fill both -side left
    pack .loc   -expand 0 -fill both -side left

    return
}

proc Shuffle {} {
    .map flush
    return
}

# ### ### ### ######### ######### #########
# Basic callback structure, log for logging, facade to transform the
# cache/tiles result into what xcanvas is expecting.

proc GET {__ at donecmd} {
    puts "GET ($at) ($donecmd)"
    ::city::grid get $at [list GOT $donecmd]
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
## Basic interface.
GUI
