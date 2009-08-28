if {![package vsatisfies [package provide Tcl] 8.4]} {
    # PRAGMA: returnok
    return
}
package ifneeded Plotchart 1.5.1 [list source [file join $dir plotchart.tcl]]
