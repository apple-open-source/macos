# cont2.tcl --
#
#  Test file for compilation.
#  Contains nested loops that use continue statements, used to check
#  generation of the exception ranges arrays.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: cont2.tcl,v 1.2 2000/05/30 22:19:11 wart Exp $

set result {}
set top 16

set outerCounter 1
while {$outerCounter <= $top} {
    # use a continue statement to skip odd numbers

    if {[expr $outerCounter % 2] == 1} {
	incr outerCounter
	continue
    }
    append result $outerCounter

    set innerCounter 1
    while {$innerCounter <= $outerCounter} {
	# use a continue statement to skip even numbers

	if {[expr $innerCounter % 2] == 0} {
	    incr innerCounter
	    continue
	}
	append result .
	incr innerCounter
    }

    incr outerCounter
}

append result DONE
set result
