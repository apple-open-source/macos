# pkga.tcl --
#
#  Test package for pkg_mkIndex. This package provides Pkga,
#  which is also provided by a DLL.
#
# Copyright (c) 1998 by Scriptics Corporation.
# All rights reserved.
# 
# RCS: @(#) $Id: pkga.tcl,v 1.1.1.2 2000/04/12 02:02:32 wsanchez Exp $

package provide Pkga 1.0

proc pkga_neq { x } {
    return [expr {! [pkgq_eq $x]}]
}
