# tclxslt.tcl --
#
#	Tcl library for TclXSLT package.
#
# Copyright (c) 2001-2002 Zveno Pty Ltd
# http://www.zveno.com/
#
# Zveno makes this software available free of charge for any purpose.
# Copies may be made of this software but all of this notice must be included
# on any copy.
#
# The software was developed for research purposes only and Zveno does not
# warrant that it is error free or fit for any purpose.  Zveno disclaims any
# liability for all claims, expenses, losses, damages and costs any user may
# incur as a result of using, copying or modifying this software.
#
# $Id: tclxslt.tcl,v 1.3 2002/09/24 21:15:14 balls Exp $

namespace eval xslt {
    namespace export getprocs
}

proc xslt::getprocs ns {
    set functions {}
    set elements {}
    foreach proc [info commands ${ns}::*] {
	if {[regexp {::([^:]+)$} $proc discard name]} {
	    if {[string equal [lindex [info args $proc] end] "args"]} {
		lappend functions $name
	    } else {
		lappend elements $name
	    }
	}
    }
    return [list $elements $functions]
}
