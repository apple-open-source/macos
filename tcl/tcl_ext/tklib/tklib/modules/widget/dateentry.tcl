# -*- tcl -*-
#
# dateentry.tcl -
#
#       dateentry widget
#
# This widget provides an entry with a visual calendar for
# choosing a date. It is mostly a gathering compoments.
#
# The basics for the entry were taken from the "MenuEntry widget"
# of the widget package in the tklib.
# The visual calendar is taken from http://wiki.tcl.tk/1816.
#
# So many thanks to Richard Suchenwirth for visual calendar
# and to Jeff Hobbs for the widget package in tklib.
#
# See the example at the bottom.
#
# RCS: @(#) $Id: dateentry.tcl,v 1.4 2010/06/01 18:06:52 hobbs Exp $
#

# Creation and Options - widget::dateentry $path ...
#  -command        -default {}
#  -dateformat     -default "%m/%d/%Y"
#  -font           -default {Helvetica 9}
#  -background     -default white
#  -textvariable   -default {}  -configuremethod C-textvariable
#
# Following are passed to widget::calendar component:
#  -firstday
#  -highlightcolor
#
# Methods
#  $widget post   - display calendar dropdown
#  $widget unpost - remove calendar dropdown
#  All other methods to entry
#
# Bindings
#  NONE
#

###

package require widget
package require widget::calendar

namespace eval ::widget {
    # http://www.famfamfam.com/lab/icons/mini/
    # ?Mini? is a set of 144 GIF icons available for free use for any purpose.
    variable dateentry_gifdata {
	R0lGODlhEAAQAMQAANnq+K7T5HiUsMHb+v/vlOXs9IyzzHWs1/T5/1ZtjUlVa+z1/+3
	x9uTx/6a2ysng+FFhe0NLXIDG/fD4/ykxQz5FVf/41vr8/6TI3MvM0XHG/vbHQPn8//
	b8/4PL/f///yH5BAAAAAAALAAAAAAQABAAAAWV4Cdam2h+5AkExCYYsCC0iSAGTisAP
	JC7kNvicPBIjkeiIyHCMDzQaFRTYH4wBY6W0+kgvpNC8GNgXLhd8CQ8Lp8f3od8sSgo
	RIasHPGY0AcNdiIHBV0PfHQNgAURIgKFfBMPCw2KAIyOkH0LA509FY4TXn6UDT0MoB8
	JDwwFDK+wrxkUjgm2EBAKChERFRUUYyfCwyEAOw==
    }
    # http://www.famfamfam.com/lab/icons/silk/
    # ?Silk? is a smooth, free icon set,
    variable dateentry_gifdata {
	R0lGODlhEAAQAPZ8AP99O/9/PWmrYmytZW6uaHOxbP+EQv+LR/+QTf+UUv+VVP+WVP+
	YV/+ZWP+aWv+dXP+eXf+fX/+nVP+rWv+gYP+hYf+iYv+jZP+kZP+kZf+wYf+zaP+4bf
	+5cf+7df+9eUJ3u1KEw1SGxFWGxlaHx12KxVyKxl+MxlmKyFuKyV+NyF6Oy1+Py2OSz
	mSTzmiW0WqX0W6Z02+b1HKe1nSg13Wh13qj2nqk2X2l3H6o3ZHBjJvHlqXNoa/Sq4Cp
	3YOr3IKq34mu2Yyw24mw3pG03Za434Ss4Ieu4Yiv4oyx44+14Yyy5I+05ZC15pO355S
	355W445294Zq75p++5pa66Zi66Zq865u9652+656/7KG/55/A7aTB5KTB56vG5abD6a
	HB7qLB76rG6a7J6rLL6rfO6rrQ67zQ68PdwNfp1dji8Nvk8d7n8t7n8+Lq9Obt9urw9
	+vx9+3y+O7z+e/z+fD0+vH2+vL2+vT3+/n8+f7+/v7//v///wAAAAAAAAAAACH5BAEA
	AH0ALAAAAAAQABAAAAfMgH2Cg4SFg2FbWFZUTk1LSEY+ODaCYHiXmJmXNIJZeBkXFBA
	NCwgHBgF4MoJXeBgfHh0cGxoTEgB4MIJVnxcWFREPDgwKCXgugk94X3zNzs1ecSyCTH
	difD0FaT0DPXxcbCiCSXZjzQJpO3kFfFFqI4JHdWTnaTp8AnxFaiKCQHRl+KARwKMHA
	W9E1KgQlIOOGT569uyB2EyIGhOCbsw500XLFClQlAz5EUTNCUE15MB546bNGjUwY5YQ
	NCPGixYrUpAIwbMnCENACQUCADs=
    }
}

proc ::widget::createdateentryLayout {} {
    variable dateentry
    if {[info exists dateentry]} { return }
    set dateentry 1
    variable dateentry_pngdata
    variable dateentry_gifdata
    set img ::widget::img_dateentry
    image create photo $img -format GIF -data $dateentry_gifdata
    namespace eval ::ttk [list set dateimg $img] ; # namespace resolved
    namespace eval ::ttk {
	# Create -padding for space on left and right of icon
	set pad [expr {[image width $dateimg] + 6}]
	style theme settings "default" {
	    style layout dateentry {
		Entry.field -children {
		    dateentry.icon -side left
		    Entry.padding -children {
			Entry.textarea
		    }
		}
	    }
	    # center icon in padded cell
	    style element create dateentry.icon image $dateimg \
		-sticky "" -padding [list $pad 0 0 0]
	}
	if 0 {
	    # Some mappings would be required per-theme to adapt to theme
	    # changes
	    foreach theme [style theme names] {
		style theme settings $theme {
		    # Could have disabled, pressed, ... state images
		    #style map dateentry -image [list disabled $img]
		}
	    }
	}
    }
}

snit::widgetadaptor widget::dateentry {
    delegate option * to hull
    delegate method * to hull

    option -command -default {}
    option -dateformat -default "%m/%d/%Y" -configuremethod C-passtocalendar
    option -font -default {Helvetica 9} -configuremethod C-passtocalendar
    option -textvariable -default {}

    delegate option -highlightcolor to calendar
    delegate option -firstday to calendar

    component dropbox
    component calendar

    variable waitVar
    variable formattedDate
    variable rawDate
    variable startOnMonday 1

    constructor args {
	::widget::createdateentryLayout

	installhull using ttk::entry -style dateentry

	bindtags $win [linsert [bindtags $win] 1 TDateEntry]

	$self MakeCalendar

	$self configurelist $args

	set now [clock seconds]
	set x [clock format $now -format "%d/%m%/%Y"]
	set rawDate [clock scan "$x 00:00:00" -format "%d/%m%/%Y %H:%M:%S"]
	set formattedDate [clock format $rawDate -format $options(-dateformat)]

	$hull configure -state normal
	$hull delete 0 end
	$hull insert end $formattedDate
	$hull configure -state readonly
    }

    method C-passtocalendar {option value} {
	set options($option) $value
	$calendar configure $option $value
    }

    method MakeCalendar {args} {
	set dropbox $win.__drop
	destroy $dropbox
	toplevel $dropbox -takefocus 0
	wm withdraw $dropbox

	if {[tk windowingsystem] ne "aqua"} {
	    wm overrideredirect $dropbox 1
	} else {
	    tk::unsupported::MacWindowStyle style $dropbox \
		help {noActivates hideOnSuspend}
	}
	wm transient $dropbox [winfo toplevel $win]
	wm group     $dropbox [winfo parent $win]
	wm resizable $dropbox 0 0

	# Unpost on Escape or whenever user clicks outside the dropdown
	bind $dropbox <Escape> [list $win unpost]
	bind $dropbox <ButtonPress> [subst -nocommands {
	    if {[string first "$dropbox" [winfo containing %X %Y]] != 0} {
		$win unpost
	    }
	}]

	set calendar $dropbox.calendar
	widget::calendar $calendar -command [mymethod DateChosen] \
	    -textvariable [myvar formattedDate] \
	    -dateformat $options(-dateformat) \
	    -font $options(-font) \
	    -borderwidth 1 -relief solid

	pack $calendar -expand 1 -fill both

	return $dropbox
    }

    method post { args } {
	# XXX should we reset date on each display?
	if {![winfo exists $dropbox]} { $self MakeCalendar }
	set waitVar 0

	foreach {x y} [$self PostPosition] { break }
	wm geometry $dropbox "+$x+$y"
	wm deiconify $dropbox
	raise $dropbox

	if {[tk windowingsystem] ne "aqua"} {
	    tkwait visibility $dropbox
	}

	ttk::globalGrab $dropbox
	focus -force $calendar
	return

	tkwait variable [myvar waitVar]

	$self unpost
    }

    method unpost {args} {
	ttk::releaseGrab $dropbox
	wm withdraw $dropbox
    }

    method PostPosition {} {
	# PostPosition --
	#	Returns the x and y coordinates where the menu
	#	should be posted, based on the dateentry and menu size
	#	and -direction option.
	#
	# TODO: adjust menu width to be at least as wide as the button
	#	for -direction above, below.
	#
	set x [winfo rootx $win]
	set y [winfo rooty $win]
	set dir "below" ; #[$win cget -direction]

	set bw [winfo width $win]
	set bh [winfo height $win]
	set mw [winfo reqwidth $dropbox]
	set mh [winfo reqheight $dropbox]
	set sw [expr {[winfo screenwidth  $dropbox] - $bw - $mw}]
	set sh [expr {[winfo screenheight $dropbox] - $bh - $mh}]

	switch -- $dir {
	    above { if {$y >= $mh} { incr y -$mh } { incr y  $bh } }
	    below { if {$y <= $sh} { incr y  $bh } { incr y -$mh } }
	    left  { if {$x >= $mw} { incr x -$mw } { incr x  $bw } }
	    right { if {$x <= $sw} { incr x  $bw } { incr x -$mw } }
	}

	return [list $x $y]
    }

    method DateChosen { args } {
	upvar 0 $options(-textvariable) date

	set waitVar 1
	set date $formattedDate
	set rawDate [clock scan $formattedDate -format $options(-dateformat)]
	if { $options(-command) ne "" } {
	    uplevel \#0 $options(-command) $formattedDate $rawDate
	}
	$self unpost

	$hull configure -state normal
	$hull delete 0 end
	$hull insert end $formattedDate
	$hull configure -state readonly
    }
}

# Bindings for menu portion.
#
# This is a variant of the ttk menubutton.tcl bindings.
# See menubutton.tcl for detailed behavior info.
#

bind TDateEntry <Enter>     { %W state active }
bind TDateEntry <Leave>     { %W state !active }
bind TDateEntry <<Invoke>>  { %W post }
bind TDateEntry <Control-space> { %W post }
bind TDateEntry <Escape>        { %W unpost }

bind TDateEntry <ButtonPress-1> { %W state pressed ; %W post }
bind TDateEntry <ButtonRelease-1> { %W state !pressed }

package provide widget::dateentry 0.92

##############
# TEST CODE ##
##############

if { [info script] eq $argv0 } {

    proc getDate { args } {
	puts [info level 0]
	puts "DATE $::DATE"

	update
    }

    proc dateTrace { args } {
	puts [info level 0]
    }

    # Samples
    # package require widget::dateentry
    set ::DATE ""
    set start [widget::dateentry .s -textvariable ::DATE \
		   -dateformat "%d.%m.%Y %H:%M" \
		   -command [list getDate .s]]
    set end [widget::dateentry .e \
		 -command [list getDate .e] \
		 -highlightcolor dimgrey \
		 -font {Courier 10} \
		 -firstday sunday]
    grid [label .sl -text "Start:"] $start  -padx 4 -pady 4
    grid [label .el -text "End:"  ] $end    -padx 4 -pady 4

    trace add variable ::DATE write dateTrace
    set ::DATE 1

    puts [$end get]
}
