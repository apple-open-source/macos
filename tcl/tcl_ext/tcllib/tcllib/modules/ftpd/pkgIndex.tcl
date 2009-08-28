if {![package vsatisfies [package provide Tcl] 8.3]} {return}
package ifneeded ftpd 1.2.4 [list source [file join $dir ftpd.tcl]]
