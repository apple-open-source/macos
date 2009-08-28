# if.tcl --
#
#  Test script for the compiler.
#  The condition in the if statement is inline compiled to an expr, and
#  therefore generates a catch exception range. Note that it must have no
#  braces, or else the inline compilation does not generate an exception
#  range. This exception range is implicit, emitted by the compiler so that
#  the expression can be retried using the "expr" command if the inlined
#  version fails. 
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: expr2.tcl,v 1.2 2000/05/30 22:19:11 wart Exp $

if 1 {
    set result PASS
} else {
    set result FAIL
}

set result
