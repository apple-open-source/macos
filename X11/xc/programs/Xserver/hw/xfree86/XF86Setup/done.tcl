# $XConsortium: done.tcl /main/4 1996/10/25 10:21:11 kaleb $
#
#
#
#
# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/done.tcl,v 3.9 1998/04/05 16:15:49 robin Exp $
#
# Copyright 1996 by Joseph V. Moss <joe@XFree86.Org>
#
# See the file "LICENSE" for information regarding redistribution terms,
# and for a DISCLAIMER OF ALL WARRANTIES.
#

#
# Routines run to end the main configuration phase (phase 2)
#

proc Done_create_widgets { win } {
	global pc98_EGC messages
	set w [winpathprefix $win]
        if !$pc98_EGC {
	    frame $w.done -width 640 -height 420 \
		    -relief ridge -borderwidth 5
	} else {
	    frame $w.done -width 640 -height 400 \
		    -relief ridge -borderwidth 5
	}
	frame $w.done.pad -relief raised -bd 3
	pack  $w.done.pad -padx 20 -pady 15 -expand yes
	label $w.done.pad.text
	$w.done.pad.text configure -text $messages(done.1)
	pack  $w.done.pad.text -fill both -expand yes
	button $w.done.pad.okay -text $messages(done.2) \
		-command [list Done_nextphase $w]
	pack  $w.done.pad.okay -side bottom -pady 10m
	focus $w.done.pad.okay
}

proc Done_activate { win } {
	set w [winpathprefix $win]
	pack $w.done -side top -fill both -expand yes
	focus $w.done.pad.okay
}

proc Done_deactivate { win } {
	set w [winpathprefix $win]
	pack forget $w.done
}

proc Done_execute { win } {
	global CfgSelection

	set w [winpathprefix $win]
	set CfgSelection Done
	config_select $w
}

proc Done_nextphase { win } {
	global StartServer XF86Setup_library env messages

	set w [winpathprefix $win]
	if $StartServer {
		mesg $messages(done.3) info
		catch {destroy .}
		catch {server_running -close $env(DISPLAY)}
		save_state
	} else {
		destroy $w.menu $w.done $w.buttons
		uplevel #0 source $XF86Setup_library/phase4.tcl
	}
}

