## -*- tcl -*-
# ### ### ### ######### ######### #########

## A discrete zoom-control widget based on two buttons and label.
## The API is similar to a scale.

# ### ### ### ######### ######### #########
## Requisites

package require Tcl 8.4        ; # No {*}-expansion :(
package require Tk
package require snit           ; # 
package require uevent::onidle ; # Some defered actions.

# ### ### ### ######### ######### #########
##

snit::widget ::canvas::zoom {
    # ### ### ### ######### ######### #########
    ## API

    option -orient   -default vertical -configuremethod O-orient \
	-type {snit::enum -values {vertical horizontal}}
    option -levels   -default {0 10}   -configuremethod O-levels \
	-type {snit::listtype -minlen 1 -maxlen 2 -type snit::integer}
    option -variable -default {}       -configuremethod O-variable
    option -command  -default {}       -configuremethod O-command

    constructor {args} {
	install reconfigure using uevent::onidle ${selfns}::reconfigure \
	    [mymethod Reconfigure]

        set options(-variable) [myvar myzoomlevel] ;# Default value
	$self configurelist $args

	# Force redraw if it could not be triggered by options.
        if {![llength $args]} {
            $reconfigure request
        }
	return
    }

    # ### ### ### ######### ######### #########
    ## Option processing. Any changes force a refresh of the grid
    ## information, and then a redraw.

    method O-orient {o v} {
	if {$options($o) eq $v} return
	set  options($o) $v
	$reconfigure request
	return
    }

    method O-levels {o v} {
	# When only a single value was specified, we use it as
	# our maximum, and default the minimum to zero.
        if {[llength $v] == 1} {
            set v [linsert $v 0 0]
        }
	if {$options($o) == $v} return
	set  options($o) $v
	$reconfigure request
	return
    }

    method O-variable {o v} {
	# The handling of an attached variable is very simple, without
	# any of the trace management one would expect to be
	# here. That is because we are using an unmapped aka hidden
	# scale widget to do this for us, at the C level.

        if {$options($o) == $v} return
        set options($o) $v
        $reconfigure request
	return
    }

    method O-command {o v} {
	if {$v eq $options(-command)} return
	set options(-command) $v
	return
    }

    # ### ### ### ######### ######### #########

    component reconfigure
    method Reconfigure {} {
	# (Re)generate the user interface.

	eval [linsert [winfo children $win] 0 destroy]

        set side $options(-orient)
        set var  $options(-variable)
        foreach {lo hi} $options(-levels) break

        set vwidth [expr {max([string length $lo], [string length $hi])}]
        set pre    [expr {[info commands ::ttk::button] ne "" ? "::ttk" : "::tk"}]

        ${pre}::frame  $win.z       -relief solid -borderwidth 1
        ${pre}::button $win.z.plus  -image ::canvas::zoom::plus  -command [mymethod ZoomIn]
        ${pre}::label  $win.z.val   -textvariable $var -justify c -anchor c -width $vwidth
        ${pre}::button $win.z.minus -image ::canvas::zoom::minus -command [mymethod ZoomOut]

        # Use an unmapped scale to keep var between lo and hi and
        # avoid doing our own trace management
        scale $win.z.sc -from $lo -to $hi -variable $var
        
        pack $win.z -fill both -expand 1
        if {$side eq "vertical"} {
            pack $win.z.plus $win.z.val $win.z.minus -side top  -fill x
        } else {
            pack $win.z.plus $win.z.val $win.z.minus -side left -fill y
        }
	return
    }

    # ### ### ### ######### ######### #########
    ## Events which act on the zoomlevel.

    method ZoomIn {} {
        upvar #0 $options(-variable) zoomlevel
        foreach {lo hi} $options(-levels) break
        if {$zoomlevel >= $hi} return
        incr zoomlevel
        $self Callback
	return
    }

    method ZoomOut {} {
        upvar #0 $options(-variable) zoomlevel
        foreach {lo hi} $options(-levels) break
        if {$zoomlevel <= $lo} return
        incr zoomlevel -1
        $self Callback
	return
    }

    method Callback {} {
	if {![llength $options(-command)]} return

        upvar   #0 $options(-variable) zoomlevel
	uplevel #0 [linsert $options(-command) end $win $zoomlevel]
	return
    }

    # ### ### ### ######### ######### #########
    ## State

    variable myzoomlevel 0 ; # The variable to use if the user
                             # did not supply one to -variable.

    # ### ### ### ######### ######### #########
}

# ### ### ### ######### ######### #########
## Images for the buttons

image create bitmap ::canvas::zoom::plus -data {
    #define plus_width 8
    #define plus_height 8
    static char bullet_bits = {
        0x18, 0x18, 0x18, 0xff, 0xff, 0x18, 0x18, 0x18
    }
}

image create bitmap ::canvas::zoom::minus -data {
    #define minus_width 8
    #define minus_height 8
    static char bullet_bits = {
        0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00
    }
}

# ### ### ### ######### ######### #########
## Ready

package provide canvas::zoom 0.2.1
return

# ### ### ### ######### ######### #########
## Scrap yard.
