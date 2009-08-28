# procbod2.tcl --
#
#  Test file for compilation.
#  Contains two procs; the first is compiled, the second is not.
#  The body for the second is generated from a "info body" command on the
#  first; this is a construct that we don't support with the debugger, and
#  an error is generated when the second proc is executed.
#  Checks that the correct error is generated.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: procbod2.tcl,v 1.2 2000/05/30 22:19:12 wart Exp $

proc a { x } {
    return "$x : $x"
}

proc b { x } [info body a]

list [b TEST]
