if {[catch {package require Tcl 8.2}]} return
package ifneeded Tclapplescript 1.0 [list load [file join $dir Tclapplescript1.0.dylib] Tclapplescript]
