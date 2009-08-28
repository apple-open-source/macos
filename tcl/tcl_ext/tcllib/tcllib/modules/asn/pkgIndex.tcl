# Tcl package index file, version 1.1

if {![package vsatisfies [package provide Tcl] 8.4]} {return}
package ifneeded asn 0.8.3 [list source [file join $dir asn.tcl]]
