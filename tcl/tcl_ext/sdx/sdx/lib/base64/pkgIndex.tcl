if {![package vsatisfies [package provide Tcl] 8.2]} {return}
package ifneeded base64   2.4.1 [list source [file join $dir base64.tcl]]
