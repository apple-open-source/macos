if {![package vsatisfies [package provide Tcl] 8.4]} {return}
package ifneeded crosshair 1.0.2 [list source [file join $dir crosshair.tcl]]
