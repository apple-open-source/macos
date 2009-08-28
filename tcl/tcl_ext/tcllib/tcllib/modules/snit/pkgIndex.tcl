if {[package vsatisfies [package provide Tcl] 8.5]} {
    package ifneeded snit 2.2.1 \
        [list source [file join $dir snit2.tcl]]
}

package ifneeded snit 1.3.1 [list source [file join $dir snit.tcl]]
