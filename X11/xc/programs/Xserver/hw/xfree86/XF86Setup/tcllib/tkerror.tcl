# $XConsortium: tkerror.tcl /main/1 1996/09/21 14:15:45 kaleb $
#
#
#
#
# tkerror.tcl --
#
# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/tcllib/tkerror.tcl,v 3.4 1996/12/27 06:55:10 dawes Exp $
#
# This file contains a modified version of the tkError procedure.  It
# posts a dialog box with the error message and gives the user a chance
# to see a more detailed stack trace. It also saves a copy of the
# stack trace to a file.
#
# Copyright 1996 by Joseph Moss,
# based on the standard implementation which is:
# Copyright (c) 1992-1994 The Regents of the University of California.
# Copyright (c) 1994-1995 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

if { $tk_version > 4.0 } {
	set errprocname bgerror
} else {
	set errprocname tkerror
}

proc $errprocname err {
    global errorInfo
    set info $errorInfo
    set fd [open /tmp/XS[pid].err w]
    puts $fd $errorInfo
    close $fd
    set button [tk_dialog .tkerrorDialog "Error in Tcl Script" \
	    "Error: $err\n\nA copy of the stack trace has been saved\
	    in the file /tmp/XS[pid].err\n\n\
	    If you think this error has not been reported before,\
	    please send a copy of that file, along with details of\
	    the problem you encountered, to joe@XFree86.org" \
	    error 0 OK "Skip Messages" "Stack Trace"]
    if {$button == 0} {
	return
    } elseif {$button == 1} {
	return -code break
    }

    set w .tkerrorTrace
    catch {destroy $w}
    toplevel $w -class ErrorTrace
    wm minsize $w 1 1
    wm title $w "Stack Trace for Error"
    wm iconname $w "Stack Trace"
    button $w.ok -text OK -command "destroy $w"
    text $w.text -relief sunken -bd 2 -yscrollcommand "$w.scroll set" \
	    -setgrid true -width 60 -height 20
    scrollbar $w.scroll -relief sunken -command "$w.text yview"
    pack $w.ok -side bottom -padx 3m -pady 2m
    pack $w.scroll -side right -fill y
    pack $w.text -side left -expand yes -fill both
    $w.text insert 0.0 $info
    $w.text mark set insert 0.0

    # Center the window on the screen.

    wm withdraw $w
    update idletasks
    set x [expr [winfo screenwidth $w]/2 - [winfo reqwidth $w]/2 \
	    - [winfo vrootx [winfo parent $w]]]
    set y [expr [winfo screenheight $w]/2 - [winfo reqheight $w]/2 \
	    - [winfo vrooty [winfo parent $w]]]
    wm geom $w +$x+$y
    wm deiconify $w

    # Be sure to release any grabs that might be present on the
    # screen, since they could make it impossible for the user
    # to interact with the stack trace.

    if {[grab current .] != ""} {
	grab release [grab current .]
    }
}

unset errprocname

