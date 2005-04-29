# procshd5.tcl --
#
#  Test file for compilation.
#  This file contains a proc definintion, where the body is shared among
#  several procs. The args are different for same, the same for others
#  It checks that the compiler detects that the object is shared and creates a
#  copy that it will then compile.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: procshd5.tcl,v 1.2 2000/05/30 22:19:13 wart Exp $

proc a { x } {
    return "$x : $x"
}

proc b { x {y dummy} } {
    return "$x : $x"
}

proc c { x y } {
    return "$x : $x"
}

proc d { x {y dummy} } {
    return "$x : $x"
}

proc e { x } {
    return "$x > $x"
}

proc f { x } {
    return "$x < $x"
}

list [a TEST] [b TEST] [c TEST dummy] [d TEST] [e TEST] [f TEST]
