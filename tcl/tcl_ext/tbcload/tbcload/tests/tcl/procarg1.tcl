# procarg1.tcl --
#
#  Test file for compilation.
#  Contains a simple procedure whose argument list contains a malformed
#  element (too many fields).
#  The compilation should fail here.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: procarg1.tcl,v 1.2 2000/05/30 22:19:12 wart Exp $

proc a { {x one two} } {
    return "$x : $x"
}

a TEST
