# simple.tcl --
#
#  Test package for pkg_mkIndex. This is a simple package, just to check
#  basic functionality.
#
# Copyright (c) 1998 by Scriptics Corporation.
# All rights reserved.
# 
# RCS: @(#) $Id: simple.tcl,v 1.2 2001/09/14 01:44:06 zlaski Exp $

package provide simple 1.0

namespace eval simple {
    namespace export lower upper
}

proc simple::lower { stg } {
    return [string tolower $stg]
}

proc simple::upper { stg } {
    return [string toupper $stg]
}
