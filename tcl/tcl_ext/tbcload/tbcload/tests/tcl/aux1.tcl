# aux1.tcl --
#
#  Test file for compilation.
#  Defines a proc that contains a simple foreach loop.
#  Tests that support for aux data works.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: aux1.tcl,v 1.2 2000/05/30 22:19:10 wart Exp $

proc a { l } {
    set L ""
    foreach k $l {
	append L $k - $k { }
    }
    return [string trim $L]
}

a {0 1 2 3 4 5 6 7 8 9}
