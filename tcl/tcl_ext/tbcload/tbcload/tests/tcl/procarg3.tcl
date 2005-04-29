# procarg3.tcl --
#
#  Test file for compilation.
#  Contains a simple procedure whose argument list contains a malformed
#  element (argument name is array element).
#  The compilation should fail here.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: procarg3.tcl,v 1.2 2000/05/30 22:19:12 wart Exp $

proc a { x(x) } {
    return TEST
}

a TEST
