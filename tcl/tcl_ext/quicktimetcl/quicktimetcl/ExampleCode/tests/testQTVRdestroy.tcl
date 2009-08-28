# Have provoked crashes for qtvr movies.
package require QuickTimeTcl
proc testProc { w what {par {}} } {
    global  myFile2
    
    puts "testProc:: w=$w, what=$what, par=$par"
    if {$what == "triggerhotspot"} {
	destroy .m
    }
}
wm title . {Tcl Crash}
tk_messageBox -message {Pick a QTVR (pano) movie}
set myFile [tk_getOpenFile]
if {$myFile != ""} {
    movie .m -file $myFile -custombutton 1 -mccommand testProc
    pack .m
}
