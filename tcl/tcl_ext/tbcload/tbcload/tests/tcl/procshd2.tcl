# procshd2.tcl --
#
#  Test file for compilation.
#  This file contains a proc definintion, where the body is shared with
#  another proc. The args are different for the two procs; in particualr,
#  proc b will fail at run time.
#  It checks that the compiler detects that the object is shared and creates a
#  copy that it will then compile.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: procshd2.tcl,v 1.2 2000/05/30 22:19:13 wart Exp $

proc a { x } {
    return "$x : $x"
}

proc b {} {
    return "$x : $x"
}

list [a TEST] [b]
