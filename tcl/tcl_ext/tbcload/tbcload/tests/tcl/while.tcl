# while.tcl --
#
#  Test file for compilation.
#  Contains nested while loops, used to check generation of the exception
#  ranges arrays.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: while.tcl,v 1.2 2000/05/30 22:19:13 wart Exp $

set result {}
set top 8

set outerCounter 1
while {$outerCounter <= $top} {
    append result $outerCounter

    set innerCounter 1
    while {$innerCounter <= $outerCounter} {
	append result .
	incr innerCounter
    }

    incr outerCounter
}

append result DONE
set result
