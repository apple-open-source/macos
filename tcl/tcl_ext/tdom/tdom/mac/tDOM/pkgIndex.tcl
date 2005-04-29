if {[catch {package require Tcl 8.2}]} return

package ifneeded tdom 0.7.8 [list load [file join $dir tDOM.0.7.8.dylib] Tdom]


package ifneeded tnc 0.3.0 "package require tdom;
load [list [file join $dir tnc.0.3.0.dylib]] tnc"
