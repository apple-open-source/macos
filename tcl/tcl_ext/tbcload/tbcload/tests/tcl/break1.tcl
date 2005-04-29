# break1.tcl --
#
#  Test file for compilation.
#  Contains nested for/foreach loops interrupted by break statements, used
#  to check generation of the exception ranges arrays.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: break1.tcl,v 1.2 2000/05/30 22:19:10 wart Exp $

set result {}
set top 8

set outer {}
set inner {}

for {set idx 0} {$idx < $top} {} {lappend outer [incr idx]}
lappend outer [incr idx]
lappend outer [incr idx]

foreach outerCounter $outer {
    if {$outerCounter > $top} {
	break
    }
    append result $outerCounter

    set inner {}
    for {set idx 0} {$idx < $outerCounter} {} {
	lappend inner [incr idx]
    }
    lappend inner [incr idx]
    lappend inner [incr idx]

    foreach innerCounter $inner {
	if {$innerCounter > $outerCounter} {
	    break;
	}
	append result .
    }
}

append result DONE
set result
