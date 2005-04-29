# foreach.tcl --
#
#  Test file for compilation.
#  Contains nested foreach loops, used to check generation of the exception
#  ranges arrays.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: foreach.tcl,v 1.2 2000/05/30 22:19:11 wart Exp $

set result {}
set top 8

set outer {}
set inner {}

for {set idx 0} {$idx < $top} {} {lappend outer [incr idx]}

foreach outerCounter $outer {
    append result $outerCounter
    lappend inner $outerCounter

    foreach innerCounter $inner {
	append result .
    }
}

append result DONE
set result
