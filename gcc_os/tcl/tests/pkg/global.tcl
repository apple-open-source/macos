# global.tcl --
#
#  Test package for pkg_mkIndex.
#  Contains global symbols, used to check that they don't have a leading ::
#
# Copyright (c) 1998 by Scriptics Corporation.
# All rights reserved.
# 
# RCS: @(#) $Id: global.tcl,v 1.2 2001/09/14 01:44:05 zlaski Exp $

package provide global 1.0

proc global_lower { stg } {
    return [string tolower $stg]
}

proc global_upper { stg } {
    return [string toupper $stg]
}
