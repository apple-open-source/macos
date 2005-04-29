# expr.tcl --
#
#  Test script for the compiler.
#  Under some conditions, the expr command is emitted as inline code wrapped
#  inside a catch staement, with the catch retrying the operation with a call
#  to (non-inlined) expr.
#  This file is expected to generate a CATCH_EXCEPTION_RANGE exception range.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: expr.tcl,v 1.2 2000/05/30 22:19:11 wart Exp $

# the "expr sqrt(16)" part generates the exception range, because that code
# can be inlined. The outer expr call does not, because of the call to the
# inner expr

set result [expr int(10 * [expr sqrt(16)])]
set result
