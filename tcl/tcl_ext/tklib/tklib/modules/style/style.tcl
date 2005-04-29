# style.tcl -- Styles for Tk.

# $Id: style.tcl,v 1.2 2004/03/18 08:56:47 davidw Exp $

# Copyright 2004 David N. Welton <davidw@dedasys.com>

package provide style 0.1

namespace eval style {
    # Available styles
    variable available [list lobster as]
}

# style::names --
#
#	Return the names of all available styles.

proc style::names {} {
    variable available
    return $available
}

# style::use --
#
#	Untill I see a better way of doing it, this is just a wrapper
#	for package require.  The problem is that 'use'ing different
#	styles won't undo the changes made by previous styles.

proc style::use {newstyle} {
    package require style::${newstyle}
}