# Tcl package index file, version 1.1

if {![package vsatisfies [package provide Tcl] 8.4]} {return}
package ifneeded json 1.0 [list source [file join $dir json.tcl]]
