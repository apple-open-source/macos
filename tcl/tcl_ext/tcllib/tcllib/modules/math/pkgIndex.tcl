if {![package vsatisfies [package provide Tcl] 8.2]} {return}
package ifneeded math             1.2.2 [list source [file join $dir math.tcl]]
package ifneeded math::geometry   1.0.1 [list source [file join $dir geometry.tcl]]
package ifneeded math::calculus   0.5.1 [list source [file join $dir calculus.tcl]]
package ifneeded math::fuzzy      0.2   [list source [file join $dir fuzzy.tcl]]
package ifneeded math::statistics 0.1.1 [list source [file join $dir statistics.tcl]]
package ifneeded math::optimize   0.1   [list source [file join $dir optimize.tcl]]
