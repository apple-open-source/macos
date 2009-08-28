# style.tcl -- Styles for Tk.

# $Id: style.tcl,v 1.4 2005/08/23 22:21:32 hobbs Exp $

# Copyright 2004 David N. Welton <davidw@dedasys.com>
# Copyright 2004 ActiveState Corporation

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
#	Until I see a better way of doing it, this is just a wrapper
#	for package require.  The problem is that 'use'ing different
#	styles won't undo the changes made by previous styles.

proc style::use {newstyle args} {
    package require style::${newstyle}
    eval [linsert $args 0 style::${newstyle}::init]
}

package provide style 0.3
