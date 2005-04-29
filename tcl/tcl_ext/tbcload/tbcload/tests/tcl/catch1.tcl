# catch1.tcl --
#
#  Test script for the compiler.
#  Contains a catch command at global scope, with no variable. This command
#  is compiled inline as of TCL 8.0, so that exception ranges should
#  be generated.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: catch1.tcl,v 1.2 2000/05/30 22:19:11 wart Exp $

catch {
    set result PASS
    error ERROR
    set result FAIL
}

set result
