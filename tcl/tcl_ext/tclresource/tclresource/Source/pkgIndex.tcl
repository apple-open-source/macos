if {[catch {package require Tcl 8.2}]} return
package ifneeded resource 1.1 [list load [file join $dir resource1.1.dylib] resource]

