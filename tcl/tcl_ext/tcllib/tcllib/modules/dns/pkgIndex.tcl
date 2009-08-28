# pkgIndex.tcl -
#
# $Id: pkgIndex.tcl,v 1.19 2008/11/22 12:28:54 mic42 Exp $

if {![package vsatisfies [package provide Tcl] 8.2]} {return}
package ifneeded dns    1.3.3 [list source [file join $dir dns.tcl]]
package ifneeded resolv 1.0.3 [list source [file join $dir resolv.tcl]]
package ifneeded ip     1.1.2 [list source [file join $dir ip.tcl]]
package ifneeded spf    1.1.1 [list source [file join $dir spf.tcl]]
