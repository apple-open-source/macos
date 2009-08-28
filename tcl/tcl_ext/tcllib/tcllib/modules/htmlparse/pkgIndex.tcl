if {![package vsatisfies [package provide Tcl] 8.2]} {return}
package ifneeded htmlparse 1.1.3 [list source [file join $dir htmlparse.tcl]]
