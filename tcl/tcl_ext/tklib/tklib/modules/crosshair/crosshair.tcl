# crosshair.tcl -
#
# Kevin's mouse-tracking crosshair in Tk's canvas widget.
#
# This package displays a mouse-tracking crosshair in the canvas widget.
#
# Copyright (c) 2003 by Kevin B. Kenny. All rights reserved.
# Redistribution permitted under the terms in
#  http://cvs.sourceforge.net/cgi-bin/viewcvs.cgi/tcl/tcl/license.terms?rev=1.3&content-type=text/plain
#
# Copyright (c) 2008 Andreas Kupries. Added ability to provide the tracking
#               information to external users.
#

# ### ### ### ######### ######### #########
## Requisites

package require Tcl 8.4
package require Tk  8.4

namespace eval ::crosshair {}

# ### ### ### ######### ######### #########
## API 

#----------------------------------------------------------------------
#
# ::crosshair::crosshair --
#
#       Displays a pair of cross-hairs in a canvas widget.  The
#       cross-hairs track the pointing device.
#
# Parameters:
#       w    - The path name of the canvas
#       args - Remaining args are treated as options as for
#              [$w create line].  Of particular interest are
#              -fill and -dash.
#
# Results:
#       None.
#
# Side effects:
#       Adds the 'crosshair' bind tag to the widget so that 
#       crosshairs will be displayed on pointing device motion.
#
#----------------------------------------------------------------------

proc ::crosshair::crosshair { w args } {
    variable config
    set opts(args) $args
    bindtags $w [linsert [bindtags $w] 1 Crosshair]
    set config($w) [array get opts]
    return
}

#----------------------------------------------------------------------
#
# ::crosshair::off -
#
#       Removes the crosshairs from a canvas widget
#
# Parameters:
#       w - The canvas from which the crosshairs should be removed
#
# Results:
#       None.
#
# Side effects:
#       If the widget has crosshairs, they are removed. The 'Crosshair'
#       bind tag is removed so that mouse motion will not restore them.
#
#----------------------------------------------------------------------

proc ::crosshair::off { w } {
    variable config
    if { ![info exists config($w)] } return
    array set opts $config($w)
    if { [winfo exists $w] } {
	Hide
	set bindtags [bindtags $w]
	set pos [lsearch -exact $bindtags Configure]
	if { $pos >= 0 } {
	    eval [list bindtags $w] [lreplace $bindtags $pos $pos]
	}
    }
    unset config($w)
    return
}

#----------------------------------------------------------------------
#
# ::crosshair::configure --
#
#       Changes the appearance of crosshairs in the canvas widget.
#
# Parameters:
#       w    - Path name of the widget
#       args - Additional args are flags to [$w create line]. Interesting
#              ones include -fill and -dash
#
# Results:
#       Returns the crosshairs' current configuration settings. 
#
#----------------------------------------------------------------------

proc ::crosshair::configure { w args } {
    variable config
    if { ![info exists config($w)] } {
	return -code error "no crosshairs in $w"
    }
    array set opts $config($w)
    if { [llength $args] > 0 } {
	array set flags $opts(args)
	array set flags $args
	set opts(args) [array get flags]
	if { [info exists opts(hhairl)] } {
	    eval [list $w itemconfig $opts(hhairl)] $args
	    eval [list $w itemconfig $opts(hhairr)] $args
	    eval [list $w itemconfig $opts(vhaird)] $args
	    eval [list $w itemconfig $opts(vhairu)] $args
	}
	set config($w) [array get opts]
    }
    return $opts(args)
}

#----------------------------------------------------------------------
#
# ::crosshair::track --
#
#       (De)activates reporting of the cross-hair coordinates through
#       a user-specified callback.
#
# Parameters:
#       which - What to do (legal values: 'on', 'off').
#       w     - The path name of the canvas
#       cmd   - Only for which == 'on', the command prefix to
#               use for execute.
#
#	The cmd is called with 7 arguments: The widget, and the x- and
#	y-coordinates of 3 points: Crosshair position, and the topleft
#	and bottomright corners of the canvas viewport. All position
#	data in pixels.
#
# Results:
#       None.
#
# Side effects:
#      See description.
#
#----------------------------------------------------------------------

proc ::crosshair::track { which w args } {
    variable config

    if { ![info exists config($w)] } {
	return -code error "no crosshairs in $w"
    }

    if { ![info exists config($w)] } return
    array set opts $config($w)

    switch -exact -- $which {
	on {
	    if {[llength $args] != 1} {
		return -code error "wrong\#args: Expected 'on w cmdprefix'"
	    }
	    set opts(track) [lindex $args 0]
	}
	off {
	    if {[llength $args] != 0} {
		return -code error "wrong\#args: Expected 'off w'"
	    }
	    catch { unset opts(track) }
	}
    }

    set config($w) [array get opts]
    return
}

# ### ### ### ######### ######### #########
## Internal commands.

#----------------------------------------------------------------------
#
# ::crosshair::Hide --
#
#       Hides the crosshair temporarily
#
# Parameters:
#       w - Canvas widget containing crosshairs
#
# Results:
#       None.
#
# Side effects:
#       If the canvas contains crosshairs, they are hidden.
#
# This procedure is invoked in response to the <Leave> event to
# hide the crosshair when the pointer is not in the window.
#
#----------------------------------------------------------------------

proc ::crosshair::Hide { w } {
    variable config
    if { ![info exists config($w)] } return
    array set opts $config($w)
    if { ![info exists opts(hhairl)] } return
    $w delete $opts(hhairl)
    $w delete $opts(hhairr)
    $w delete $opts(vhaird)
    $w delete $opts(vhairu)
    unset opts(hhairl)
    unset opts(hhairr)
    unset opts(vhairu)
    unset opts(vhaird)
    set config($w) [array get opts]
    return
}

#----------------------------------------------------------------------
#
# ::crosshair::Unhide --
#
#       Places a hidden crosshair back on display
#
# Parameters:
#       w - Canvas widget containing crosshairs
#       x - x co-ordinate relative to the window where the vertical
#           crosshair should appear
#       y - y co-ordinate relative to the window where the horizontal
#           crosshair should appear.
#
# Results:
#       None.
#
# Side effects:
#       Crosshairs are put on display.
#
# This procedure is invoked in response to the <Enter> event to
# restore the crosshair to the display.
#
#----------------------------------------------------------------------

proc ::crosshair::Unhide { w x y } {
    variable config
    if { ![info exists config($w)] } return
    array set opts $config($w)
    if { ![info exists opts(hhairl)] } {
	set opts(hhairl) [eval [list $w create line 0 0 0 0] $opts(args)]
	set opts(hhairr) [eval [list $w create line 0 0 0 0] $opts(args)]
	set opts(vhaird) [eval [list $w create line 0 0 0 0] $opts(args)]
	set opts(vhairu) [eval [list $w create line 0 0 0 0] $opts(args)]
    }
    set config($w) [array get opts]
    Move $w $x $y
    return
}

#----------------------------------------------------------------------
#
# ::crosshair::Move --
#
#       Moves the crosshairs in a camvas
#
# Parameters:
#       w - Canvas widget containing crosshairs
#       x - x co-ordinate relative to the window where the vertical
#           crosshair should appear
#       y - y co-ordinate relative to the window where the horizontal
#           crosshair should appear.
#
# Results:
#       None.
#
# Side effects:
#       Crosshairs move.
#
# This procedure is called in response to a <Motion> event in a canvas
# with crosshairs.
#
#----------------------------------------------------------------------

proc ::crosshair::Move { w x y } {
    variable config
    array set opts $config($w)
    set opts(x) [$w canvasx $x]
    set opts(y) [$w canvasy $y]
    set opts(x0) [$w canvasx 0]
    set opts(x1) [$w canvasx [winfo width $w]]
    set opts(y0) [$w canvasy 0]
    set opts(y1) [$w canvasy [winfo height $w]]
    if { [info exists opts(hhairl)] } {
	# +/-4 is the minimal possible distance which still prevents
	# the canvas from choosing the crosshairs as 'current' object
	# under the cursor.
	set n 4
	$w coords $opts(hhairl) $opts(x0) $opts(y) [expr {$opts(x)-$n}] $opts(y)
	$w coords $opts(hhairr) [expr {$opts(x)+$n}] $opts(y) $opts(x1) $opts(y)
	$w coords $opts(vhairu) $opts(x) $opts(y0) $opts(x) [expr {$opts(y)-$n}]
	$w coords $opts(vhaird) $opts(x) [expr {$opts(y)+$n}] $opts(x) $opts(y1)
	$w raise $opts(hhairl)
	$w raise $opts(hhairr)
	$w raise $opts(vhaird)
	$w raise $opts(vhairu)
    }
    set config($w) [array get opts]
    if {[info exists opts(track)]} {
	uplevel \#0 [linsert $opts(track) end $w $opts(x) $opts(y) $opts(x0) $opts(y0) $opts(x1) $opts(y1)]
    }
    return
}

# ### ### ### ######### ######### #########
## State

namespace eval ::crosshair {
    
    # Array holding information describing crosshairs in canvases
    
    variable  config
    array set config {}
    
    # Controller that positions crosshairs according to user actions
    
    bind Crosshair <Destroy> "[namespace code off] %W"
    bind Crosshair <Enter>   "[namespace code Unhide] %W %x %y"
    bind Crosshair <Leave>   "[namespace code Hide] %W"
    bind Crosshair <Motion>  "[namespace code Move] %W %x %y"
}

# ### ### ### ######### ######### #########
## Ready

package provide crosshair 1.0.2
