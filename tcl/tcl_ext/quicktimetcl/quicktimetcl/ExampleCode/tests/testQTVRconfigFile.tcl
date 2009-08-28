# Have provoked crashes for qtvr movies.
package require QuickTimeTcl
proc testProc { w what {par {}} } {
    global  myFile2
    
    puts "testProc:: w=$w, what=$what, par=$par"
    if {$what == "triggerhotspot"} {
	#after idle [list .m configure -file $myFile2]
	.m configure -file $myFile2
	tk_messageBox -type ok -message {We made .m configure -file $myFile2}
    }
}
wm title . {Tcl Crash}
tk_messageBox -message {Pick two QTVR (pano) movies}
set myFile [tk_getOpenFile]
set myFile2 [tk_getOpenFile]
if {$myFile != ""} {
    movie .m -file $myFile -custombutton 1 -mccommand testProc
    pack .m
}
