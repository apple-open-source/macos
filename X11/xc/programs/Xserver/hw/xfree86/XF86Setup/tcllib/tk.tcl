# $XConsortium: tk.tcl /main/1 1996/09/21 14:15:57 kaleb $
#
#
#
#
# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/tcllib/tk.tcl,v 3.3 1997/11/22 00:00:08 hohndel Exp $
#
# tk.tcl --
#
# Initialization script normally executed in the interpreter for each
# Tk-based application.  Arranges class bindings for widgets.
#
# @(#) tk.tcl 1.73 95/08/30 16:40:20
#
# Copyright (c) 1992-1994 The Regents of the University of California.
# Copyright (c) 1994-1995 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

# Insist on running with compatible versions of Tcl and Tk.

scan [info tclversion] "%d.%d" a b
if {$a < 7 || ($a == 7 && $b < 5)} {
    error "wrong version of Tcl loaded ([info tclversion]): need 7.5 or later"
}
scan $tk_version "%d.%d" a b
if {($a < 4) || ($a == 4 && $b < 1)} {
    error "wrong version of Tk loaded ($tk_version): need 4.1 or later"
}
unset a b

# Add Tk's directory to the end of the auto-load search path:

lappend auto_path $tk_library

# Turn off strict Motif look and feel as a default.

set tk_strictMotif 0

# tkScreenChanged --
# This procedure is invoked by the binding mechanism whenever the
# "current" screen is changing.  The procedure does two things.
# First, it uses "upvar" to make global variable "tkPriv" point at an
# array variable that holds state for the current display.  Second,
# it initializes the array if it didn't already exist.
#
# Arguments:
# screen -		The name of the new screen.

proc tkScreenChanged screen {
    set disp [file rootname $screen]
    uplevel #0 upvar #0 tkPriv.$disp tkPriv
    global tkPriv
    if [info exists tkPriv] {
	set tkPriv(screen) $screen
	return
    }
    set tkPriv(afterId) {}
    set tkPriv(buttons) 0
    set tkPriv(buttonWindow) {}
    set tkPriv(dragging) 0
    set tkPriv(focus) {}
    set tkPriv(grab) {}
    set tkPriv(initPos) {}
    set tkPriv(inMenubutton) {}
    set tkPriv(listboxPrev) {}
    set tkPriv(mouseMoved) 0
    set tkPriv(oldGrab) {}
    set tkPriv(popup) {}
    set tkPriv(postedMb) {}
    set tkPriv(pressX) 0
    set tkPriv(pressY) 0
    set tkPriv(screen) $screen
    set tkPriv(selectMode) char
    set tkPriv(window) {}
}

# Do initial setup for tkPriv, so that it is always bound to something
# (otherwise, if someone references it, it may get set to a non-upvar-ed
# value, which will cause trouble later).

tkScreenChanged [winfo screen .]

# ----------------------------------------------------------------------
# Read in files that define all of the class bindings.
# ----------------------------------------------------------------------

source $tk_library/button.tcl
source $tk_library/entry.tcl
source $tk_library/listbox.tcl
source $tk_library/menu.tcl
source $tk_library/scale.tcl
source $tk_library/scrollbar.tcl
source $tk_library/text.tcl

# ----------------------------------------------------------------------
# Default bindings for keyboard traversal.
# ----------------------------------------------------------------------

bind all <Tab> {focus [tk_focusNext %W]}
bind all <Key-ISO_Left_Tab> {focus [tk_focusPrev %W]}
bind all <Shift-Tab> {focus [tk_focusPrev %W]}

# tkCancelRepeat --
# This procedure is invoked to cancel an auto-repeat action described
# by tkPriv(afterId).  It's used by several widgets to auto-scroll
# the widget when the mouse is dragged out of the widget with a
# button pressed.
#
# Arguments:
# None.

proc tkCancelRepeat {} {
    global tkPriv
    after cancel $tkPriv(afterId)
    set tkPriv(afterId) {}
}
