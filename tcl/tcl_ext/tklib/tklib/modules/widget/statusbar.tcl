# -*- tcl -*-
#
#  statusbar.tcl -
#	Create a status bar Tk widget
#
# RCS: @(#) $Id: statusbar.tcl,v 1.7 2007/06/21 01:59:40 hobbs Exp $
#

# Creation and Options - widget::scrolledwindow $path ...
#
#  -separator -default 1 ; show horizontal separator on top of statusbar
#  -resize    -default 1 ; show resize control on bottom right
#  -resizeseparator -default 1 ; show separator for resize control
#  ## Padding can be a list of {padx pady}
#  -ipad -default 1 ; provides padding around each status bar item
#  -pad  -default 0 ; provides general padding around the status bar
#
#  All other options to frame
#
# Methods
#  $path getframe           => $frame
#  $path add $widget ?args? => $widget
#  All other methods to frame
#
# Bindings
#  NONE
#
#  Provides a status bar to be placed at the bottom of a toplevel.
#  Currently does not support being placed in a toplevel that has
#  gridding applied (via widget -setgrid or wm grid).
#
#  Ensure that the widget is placed at the very bottom of the toplevel,
#  otherwise the resize behavior may behave oddly.
#

package require widget
package require tile

if {0} {
    proc sample {} {
    # sample usage
    eval destroy [winfo children .]
    pack [text .t -width 0 -height 0] -fill both -expand 1

    set sbar .s
    widget::statusbar $sbar
    pack $sbar -side bottom -fill x
    set f [$sbar getframe]

    # Specify -width 1 for the label widget so it truncates nicely
    # instead of requesting large sizes for long messages
    set w [label $f.status -width 1 -anchor w -textvariable ::STATUS]
    set ::STATUS "This is a status message"
    # give the entry weight, as we want it to be the one that expands
    $sbar add $w -weight 1

    # BWidget's progressbar
    set w [ProgressBar $f.bpbar -orient horizontal \
	       -variable ::PROGRESS -bd 1 -relief sunken]
    set ::PROGRESS 50
    $sbar add $w
    }
}

snit::widget widget::statusbar {
    hulltype ttk::frame

    component resizer
    component separator
    component sepresize
    component frame

    # -background, -borderwidth and -relief apply to outer frame, but relief
    # should be left flat for proper look
    delegate option * to hull
    delegate method * to hull

    option -separator       -default 1 -configuremethod C-separator \
	-type [list snit::boolean]
    option -resize          -default 1 -configuremethod C-resize \
	-type [list snit::boolean]
    option -resizeseparator -default 1 -configuremethod C-resize \
	-type [list snit::boolean]
    # -pad provides general padding around the status bar
    # -ipad provides padding around each status bar item
    # Padding can be a list of {padx pady}
    option -ipad -default 2 -configuremethod C-ipad \
	-type [list snit::listtype -type {snit::integer} -minlen 1 -maxlen 4]
    delegate option -pad to frame as -padding

    variable ITEMS -array {}
    variable uid 0

    constructor args {
	$hull configure -height 18

	install frame using ttk::frame $win.frame

	install resizer using ttk::sizegrip $win.resizer

	install separator using ttk::separator $win.separator \
	    -orient horizontal

	install sepresize using ttk::separator $win.sepresize \
	    -orient vertical

	grid $separator -row 0 -column 0 -columnspan 3 -sticky ew
	grid $frame     -row 1 -column 0 -sticky news
	grid $sepresize -row 1 -column 1 -sticky ns;# -padx $ipadx -pady $ipady
	grid $resizer   -row 1 -column 2 -sticky se
	grid columnconfigure $win 0 -weight 1

	$self configurelist $args
    }

    method C-ipad {option value} {
	set options($option) $value
	# returns pad values - each will be a list of 2 ints
	foreach {px py} [$self _padval $value] { break }
	foreach w [grid slaves $frame] {
	    if {[string match _sep* $w]} {
		grid configure $w -padx $px -pady 0
	    } else {
		grid configure $w -padx $px -pady $py
	    }
	}
    }

    method C-separator {option value} {
	set options($option) $value
	if {$value} {
	    grid $separator
	} else {
	    grid remove $separator
	}
    }

    method C-resize {option value} {
	set options($option) $value
	if {$options(-resize)} {
	    if {$options(-resizeseparator)} {
		grid $sepresize
	    }
	    grid $resizer
	} else {
	    grid remove $sepresize $resizer
	}
    }

    # Use this or 'add' - but not both
    method getframe {} { return $frame }

    method add {what args} {
	if {[winfo exists $what]} {
	    set w $what
	    set symbol $w
	    set ours 0
	} else {
	    set w $frame._$what[incr uid]
	    set symbol [lindex $args 0]
	    set args [lrange $args 1 end]
	    if {![llength $args] || $symbol eq "%AUTO%"} {
		# Autogenerate symbol name
		set symbol _$what$uid
	    }
	    if {[info exists ITEMS($symbol)]} {
		return -code error "item '$symbol' already exists"
	    }
	    if {$what eq "label" || $what eq "button"
		|| $what eq "checkbutton" || $what eq "radiobutton"} {
		set w [ttk::$what $w -style Toolbutton -takefocus 0]
	    } elseif {$what eq "separator"} {
		set w [ttk::separator $w -orient vertical]
	    } elseif {$what eq "space"} {
		set w [ttk::frame $w]
	    } else {
		return -code error "unknown item type '$what'"
	    }
	    set ours 1
	}
	set opts(-weight)	[string equal $what "space"]
	set opts(-separator)	0
	set opts(-sticky)	news
	set opts(-pad)		$options(-ipad)
	if {$what eq "separator"} {
	    # separators should not have pady by default
	    lappend opts(-pad) 0
	}
	set cmdargs [list]
	set len [llength $args]
	for {set i 0} {$i < $len} {incr i} {
	    set key [lindex $args $i]
	    set val [lindex $args [incr i]]
	    if {$key eq "--"} {
		eval [list lappend cmdargs] [lrange $args $i end]
		break
	    }
	    if {[info exists opts($key)]} {
		set opts($key) $val
	    } else {
		# no error - pass to command
		lappend cmdargs $key $val
	    }
	}
	if {[catch {eval [linsert $cmdargs 0 $w configure]} err]} {
	    # we only want to destroy widgets we created
	    if {$ours} { destroy $w }
	    return -code error $err
	}
	set ITEMS($symbol) $w
	widget::isa listofint 4 -pad $opts(-pad)
	# returns pad values - each will be a list of 2 ints
	foreach {px py} [$self _padval $opts(-pad)] { break }

	# get cols,rows extent
	foreach {cols rows} [grid size $frame] break
	# Add separator if requested, and we aren't the first element
	if {$opts(-separator) && $cols != 0} {
	    set sep [ttk::separator $frame._sep[winfo name $w] \
			 -orient vertical]
	    # No pady for separators, and adjust padx for separator space
	    set sx $px
	    if {[lindex $sx 0] < 2} { lset sx 0 2 }
	    lset px 1 0
	    grid $sep -row 0 -column $cols -sticky ns -padx $sx -pady 0
	    incr cols
	}

	grid $w -in $frame -row 0 -column $cols -sticky $opts(-sticky) \
	    -padx $px -pady $py
	grid columnconfigure $frame $cols -weight $opts(-weight)

	return $symbol
    }

    method remove {args} {
	set destroy [string equal [lindex $args 0] "-destroy"]
	if {$destroy} {
	    set args [lrange $args 1 end]
	}
	foreach sym $args {
	    # Should we ignore unknown (possibly already removed) items?
	    #if {![info exists ITEMS($sym)]} { continue }
	    set w $ITEMS($sym)
	    # separator name is based off item name
	    set sep $frame._sep[winfo name $w]
	    # destroy separator for remove or destroy case
	    destroy $sep
	    if {$destroy} {
		destroy $w
	    } else {
		grid forget $w
	    }
	    unset ITEMS($sym)
	}
    }

    method delete {args} {
	eval [linsert $args 0 $self remove -destroy]
    }

    method items {{ptn *}} {
	# return from ordered list
	if {$ptn ne "*"} {
	    return [array names ITEMS $ptn]
	}
	return [array names ITEMS]
    }

    method _padval {val} {
	set len [llength $val]
	if {$len == 0} {
	    return [list 0 0 0 0]
	} elseif {$len == 1} {
	    return [list [list $val $val] [list $val $val]]
	} elseif {$len == 2} {
	    set x [lindex $val 0] ; set y [lindex $val 1]
	    return [list [list $x $x] [list $y $y]]
	} elseif {$len == 3} {
	    return [list [list [lindex $val 0] [lindex $val 2]] \
			[list [lindex $val 1] [lindex $val 1]]]
	} else {
	    return $val
	}
    }
}

package provide widget::statusbar 1.2
