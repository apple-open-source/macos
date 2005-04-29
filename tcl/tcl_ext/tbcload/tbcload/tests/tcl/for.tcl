# for.tcl --
#
#  Test file for compilation.
#  Contains nested for loops, used to check generation of the exception
#  ranges arrays.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: for.tcl,v 1.2 2000/05/30 22:19:11 wart Exp $

set result {}
set top 8

for {set outerCounter 1} {$outerCounter <= $top} {incr outerCounter} {
    append result $outerCounter

    for {set innerCounter 1} \
	    {$innerCounter <= $outerCounter} {incr innerCounter} {
	append result .
    }
}

append result DONE
set result
