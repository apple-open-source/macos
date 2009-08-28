# pkgIndex.tcl - 
#
# RIPEMD package index file
#
# This package has been tested with tcl 8.2.3 and above.
#
# $Id: pkgIndex.tcl,v 1.5 2005/02/23 12:41:37 patthoyts Exp $

if {![package vsatisfies [package provide Tcl] 8.2]} {return}
package ifneeded ripemd128 1.0.3 [list source [file join $dir ripemd128.tcl]]
package ifneeded ripemd160 1.0.3 [list source [file join $dir ripemd160.tcl]]
