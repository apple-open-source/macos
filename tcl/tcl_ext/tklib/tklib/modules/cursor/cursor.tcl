# cursor.tcl --
#
#       Tk cursor handling routines
#
# Copyright (c) 2001-2009 by Jeffrey Hobbs
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: cursor.tcl,v 1.3 2009/04/24 22:03:48 hobbs Exp $

package require Tk 8.0
package provide cursor 0.3

namespace eval ::cursor {
    namespace export propagate restore display

    # Default to depthfirst (bottom up) restore to account for
    # megawidgets that will self-propagate cursor changes down.
    variable depthfirst 1

    variable cursors [list \
	    X_cursor arrow based_arrow_down based_arrow_up boat bogosity \
	    bottom_left_corner bottom_right_corner bottom_side bottom_tee \
	    box_spiral center_ptr circle clock coffee_mug cross cross_reverse \
	    crosshair diamond_cross dot dotbox double_arrow draft_large \
	    draft_small draped_box exchange fleur gobbler gumby hand1 hand2 \
	    heart icon iron_cross left_ptr left_side left_tee leftbutton \
	    ll_angle lr_angle man middlebutton mouse pencil pirate plus \
	    question_arrow right_ptr right_side right_tee rightbutton \
	    rtl_logo sailboat sb_down_arrow sb_h_double_arrow sb_left_arrow \
	    sb_right_arrow sb_up_arrow sb_v_double_arrow shuttle sizing \
	    spider spraycan star target tcross top_left_arrow top_left_corner \
	    top_right_corner top_side top_tee trek ul_angle umbrella \
	    ur_angle watch xterm \
	    ]

    switch -exact $::tcl_platform(os) {
	"windows" {
	    lappend cursors no starting size \
		    size_ne_sw size_ns size_nw_se size_we uparrow wait
	}
	"macintosh" {
	    lappend cursors text cross-hair
	}
	"unix" {
	    # no extra cursors
	}
    }
}

# ::cursor::propagate --
#
#	Propagates a cursor to a widget and all descendants.
#
# Arguments:
#	w	Parent widget to set cursor on (includes children)
#	cursor	The cursor to use
#
# Results:
#	Set the cursor of $w and all descendants to $cursor

proc ::cursor::propagate {w cursor} {
    variable CURSOR

    # Ignores {} cursors or widgets that don't have a -cursor option
    if {![catch {set CURSOR($w) [$w cget -cursor]}] && $CURSOR($w) != ""} {
	$w config -cursor $cursor
    } else {
	catch {unset CURSOR($w)}
    }
    foreach child [winfo children $w] { propagate $child $cursor }
}

# ::cursor::restore --
#
#	Restores original cursor of a widget and all descendants.
#
# Arguments:
#	w	Parent widget to restore cursor for (includes children)
#	cursor	The default cursor to use (if none was cached by propagate)
#
# Results:
#	Restore the cursor of $w and all descendants

proc ::cursor::restore {w {cursor {}}} {
    variable depthfirst
    variable CURSOR

    if {$depthfirst} {
	foreach child [winfo children $w] { restore $child $cursor }
    }
    if {[info exists CURSOR($w)]} {
	$w config -cursor $CURSOR($w)
    } else {
	# Not all widgets have -cursor
	catch {$w config -cursor $cursor}
    }
    if {!$depthfirst} {
	foreach child [winfo children $w] { restore $child $cursor }
    }
}


# ::cursor::display --
#
#	Show all known cursors for viewing
#
# Arguments:
#	w	Parent widget to use for dialog
#
# Results:
#	Pops up a dialog

proc ::cursor::display {{root .}} {
    variable cursors
    if {$root == "."} {
	set t .__cursorDisplay
    } else {
	set t $root.__cursorDisplay
    }
    destroy $t
    toplevel $t
    wm withdraw $t
    label $t.lbl -text "Select a cursor:" -anchor w
    listbox $t.lb -selectmode single -yscrollcommand [list $t.sy set]
    scrollbar $t.sy -orient v -command [list $t.lb yview]
    button $t.d -text Dismiss -command [list destroy $t]
    pack $t.d -side bottom
    pack $t.lbl -side top -fill x
    pack $t.sy -side right -fill y
    pack $t.lb -side right -fill both -expand 1
    eval [list $t.lb insert end] $cursors
    bind $t.lb <Button-1> { %W config -cursor [%W get [%W nearest %y]] }
    wm deiconify $t
}

