# expr1.tcl --
#
#  Test script for the compiler.
#  Under some conditions, the expr command is emitted as inline code wrapped
#  inside a catch staement, with the catch retrying the operation with a call
#  to (non-inlined) expr.
#  This file is not expected to generate a CATCH_EXCEPTION_RANGE exception
#  range.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: expr1.tcl,v 1.2 2000/05/30 22:19:11 wart Exp $

set l {1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16}

# the call to "llength $l" turns off the inlining

set result [expr int(10 * [expr sqrt([llength $l])])]
set result
