# procvar2.tcl --
#
#  Test file for compilation.
#  Simple procedure with local variables. Used to test that the local
#  variables count is set correctly in the proc struct created for the
#  procedure.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: procvar2.tcl,v 1.2 2000/05/30 22:19:13 wart Exp $

proc a { x y } {
    set l [string tolower $x ]
    set m [string toupper $y ]

    return [list $x $l $y $m]
}

a TEST test
