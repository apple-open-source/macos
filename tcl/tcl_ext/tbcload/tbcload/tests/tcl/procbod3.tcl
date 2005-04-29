# procbod3.tcl --
#
#  Test file for compilation.
#  Defines a proc which will be compiled, then does an info body on it and
#  uses it as a string, then calls the procedure again.
#  Checks that the internal representation of the compiled procedure is
#  not flushed.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: procbod3.tcl,v 1.2 2000/05/30 22:19:12 wart Exp $

proc a { x } {
    return "$x : $x"
}

set result [a TEST]
set L ""
lappend L [info body a]
lappend result [a TEST]

set result
