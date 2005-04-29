# proc.tcl --
#
#  Test file for compilation.
#  Contains a simple procedure.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: proc.tcl,v 1.2 2000/05/30 22:19:11 wart Exp $

proc a { x } {
    return "$x : $x"
}

proc b { x } {
    return "$x > $x"
}

list [a TEST] [b TEST]
