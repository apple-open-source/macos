if {![package vsatisfies [package provide Tcl] 8.4]} {
    # PRAGMA: returnok
    return
}
package ifneeded Plotchart 1.9.2 [list source [file join $dir plotchart.tcl]]
if {![package vsatisfies [package provide Tcl] 8.5]} {
    # PRAGMA: returnok
    return
}
package ifneeded xyplot    1.0.1 [list source [file join $dir xyplot.tcl]]
