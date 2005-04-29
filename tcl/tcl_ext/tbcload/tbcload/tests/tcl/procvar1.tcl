# procvar1.tcl --
#
#  Test file for compilation.
#  This procedure compiles fine, but should fail at run time because of
#  the reference to an unknown variable.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: procvar1.tcl,v 1.2 2000/05/30 22:19:13 wart Exp $

proc a {} {
    return "$x : $x"
}

a
