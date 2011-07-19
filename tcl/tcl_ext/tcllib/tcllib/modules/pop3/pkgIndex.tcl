if {![package vsatisfies [package provide Tcl] 8.2]} {return}
package ifneeded pop3 1.7 [list source [file join $dir pop3.tcl]]
