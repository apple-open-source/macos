package provide RHABOUT 1.0
set dirname [file dirname [info script]]
lappend auto_path [file join $dirname rhabout]
catch {load [file join $dirname rhabout rhabout.so]}
