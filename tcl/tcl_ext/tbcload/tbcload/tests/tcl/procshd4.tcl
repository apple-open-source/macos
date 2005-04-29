# procshd4.tcl --
#
#  Test file for compilation.
#  This file contains a proc definintion, where the body is shared with other
#  statements. It checks that the compiler detects that the object is shared
#  and creates a copy that it will then compile.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: procshd4.tcl,v 1.2 2000/05/30 22:19:13 wart Exp $

proc a { x } {
    return "$x : $x"
}

set b {
    return "$x : $x"
}

list [a TEST] $b
