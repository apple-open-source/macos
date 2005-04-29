# procbod1.tcl --
#
#  Test file for compilation.
#  Contains two procs; the first is compiled, the second is not.
#  Returns "info body" on the procs.
#  Checks that "info body" is well-behaved with a compiled proc body.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: procbod1.tcl,v 1.2 2000/05/30 22:19:12 wart Exp $

proc a { x } {
    return "$x : $x"
}

set sh {
    return "$x : $x"
}

proc b { x } $sh

list [a TEST] [b TEST] [info body a]


