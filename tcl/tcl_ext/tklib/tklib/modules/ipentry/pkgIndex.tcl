if { ![package vsatisfies [package provide Tcl] 8.4] } { return }
package ifneeded ipentry 0.3 [list source [file join $dir ipentry.tcl]]

