# movie.tcl
#
# This file defines the default bindings for the movie widget and provides
# procedures that help in implementing those bindings.
#
# Copyright (c) 2004 Mats Bengtsson
#
# $Id: movie.tcl,v 1.3 2007/03/06 07:46:57 matben Exp $

#-------------------------------------------------------------------------
# The code below creates the default class bindings for movies.
#-------------------------------------------------------------------------


bind Movie <Right> {
    if {[%W ispanoramic]} {
	%W pan [expr [%W pan] - 10]
    } else {
	%W step 1
    }
}
bind Movie <Left> {
    if {[%W ispanoramic]} {
	%W pan [expr [%W pan] + 10]
    } else {
	%W step -1
    }
}
bind Movie <Up> {
    if {[%W ispanoramic]} {
	%W tilt [expr [%W tilt] + 10]
    } else {
	set vol [expr 64 + [%W cget -volume]]
	if {$vol > 255} {set vol 255}
	%W configure -volume $vol
    }
}
bind Movie <Down> {
    if {[%W ispanoramic]} {
	%W tilt [expr [%W tilt] - 10]
    } else {
	set vol [expr -64 + [%W cget -volume]]
	if {$vol < 0} {set vol 0}
	%W configure -volume $vol
    }
}
bind Movie <Shift_L> {
    if {[%W ispanoramic]} {
	%W fieldofview [expr [%W fieldofview] - 5]
    }
}
bind Movie <Control_L> {
    if {[%W ispanoramic]} {
	%W fieldofview [expr [%W fieldofview] + 5]
    }
}
bind Movie <space> {
    if {![%W ispanoramic]} {
	if {[%W rate] == 0} {
	    %W play
	} else {
	    %W stop
	}
    }
}
bind Movie <Button-1> {
    focus %W
}

