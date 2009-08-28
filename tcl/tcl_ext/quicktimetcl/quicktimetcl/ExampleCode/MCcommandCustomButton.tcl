# -custombutton and -mccommand

package require QuickTimeTcl
proc testProc {w what {par {}}} {    
    puts "testProc:: w=$w, what=$what, par=$par"
}
wm title . {Tcl Callback}
set myFile [tk_getOpenFile]
if {$myFile != ""} {
    movie .m -file $myFile -custombutton 1 -mccommand testProc
    pack .m
}
