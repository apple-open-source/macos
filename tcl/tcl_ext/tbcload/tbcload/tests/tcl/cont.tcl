# cont.tcl --
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
# RCS: @(#) $Id: cont.tcl,v 1.2 2000/05/30 22:19:11 wart Exp $

set result {}
set top 16

for {set outerCounter 1} {$outerCounter <= $top} {incr outerCounter} {
    # use a continue statement to skip odd numbers

    if {[expr $outerCounter % 2] == 1} {
	continue
    }
    append result $outerCounter

    for {set innerCounter 1} \
	    {$innerCounter <= $outerCounter} {incr innerCounter} {
	# use a continue statement to skip even numbers

	if {[expr $innerCounter % 2] == 0} {
	    continue
	}
	append result .
    }
}

append result DONE
set result
