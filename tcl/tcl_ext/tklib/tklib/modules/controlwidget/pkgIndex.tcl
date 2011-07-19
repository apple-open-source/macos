# pkgIndex.tcl --
#     Index script for controlwidget package
#     Note:
#     We could split this into several parts. Now it is presented
#     as a single package.
#
if {![package vsatisfies [package provide Tcl] 8.5]} {
    # PRAGMA: returnok
    return
}
package ifneeded controlwidget 0.1 [list source [file join $dir controlwidget.tcl]]
