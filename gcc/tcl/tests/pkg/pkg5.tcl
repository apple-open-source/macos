# pkg5.tcl --
#
#  Test package for pkg_mkIndex. This package requires pkg2, and it calls
#  a pkg2 proc in the code that is executed by the file.
#  Pkg2 is a split package.
#
# Copyright (c) 1998 by Scriptics Corporation.
# All rights reserved.
# 
# RCS: @(#) $Id: pkg5.tcl,v 1.2 2001/09/14 01:44:06 zlaski Exp $

package require pkg2 1.0

package provide pkg5 1.0

namespace eval pkg5 {
    namespace export p5-1 p5-2
    variable m2 [pkg2::p2-1 10]
    variable m3 [pkg2::p2-2 10]
}

proc pkg5::p5-1 { num } {
    variable m2
    return [expr {$m2 * $num}]
}

proc pkg5::p5-2 { num } {
    variable m2
    return [expr {$m2 * $num}]
}
