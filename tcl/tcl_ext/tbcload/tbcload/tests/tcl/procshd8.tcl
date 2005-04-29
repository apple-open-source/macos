# procshd6.tcl --
#
#  Test file for compilation.
#  This file contains a proc definintion, where the body is loaded from a
#  variable. Thus, the body is not compiled.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: procshd8.tcl,v 1.2 2000/05/30 22:19:13 wart Exp $

set a {
    return "$x : $x"
}

proc b { x } $a

proc c { x {y dummy} } {
    return "$x : $x"
}

list $a [b TEST] [c TEST]
