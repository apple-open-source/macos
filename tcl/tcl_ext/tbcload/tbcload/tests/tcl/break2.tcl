# break2.tcl --
#
#  Test file for compilation.
#  Contains nested loops interrupted by break statements, used to check
#  generation of the exception ranges arrays.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: break2.tcl,v 1.2 2000/05/30 22:19:10 wart Exp $

set result {}
set top 8

set outerCounter 1
while {1} {
    if {$outerCounter > $top} {
	break
    }
    append result $outerCounter

    set innerCounter 1
    while {1} {
	if {$innerCounter > $outerCounter} {
	    break;
	}
	append result .
	incr innerCounter
    }

    incr outerCounter
}

append result DONE
set result
