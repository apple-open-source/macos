# catch.tcl --
#
#  Test script for the compiler.
#  Contains a catch command at global scope, with a variable. This command
#  is not compiled inline as of TCL 8.0, so that no exception ranges should
#  be generated.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: catch.tcl,v 1.2 2000/05/30 22:19:10 wart Exp $

set result FAIL
catch {
    error PASS
} result

set result
