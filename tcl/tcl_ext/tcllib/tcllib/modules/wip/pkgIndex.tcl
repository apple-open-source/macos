if {![package vsatisfies [package provide Tcl] 8.4]} {return}
package ifneeded wip 1.1.1 [list source [file join $dir wip.tcl]]

if {![package vsatisfies [package provide Tcl] 8.5]} {return}
package ifneeded wip 2.1.1 [list source [file join $dir wip2.tcl]]
