if {![package vsatisfies [package provide Tcl] 8.4]} {return}
package ifneeded docstrip       1.2 [list source [file join $dir docstrip.tcl]]
package ifneeded docstrip::util 1.2 [list source [file join $dir docstrip_util.tcl]]
