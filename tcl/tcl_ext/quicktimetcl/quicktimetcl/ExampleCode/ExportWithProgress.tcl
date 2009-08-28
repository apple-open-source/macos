
package require QuickTimeTcl
wm title . {Export Progress}
set myFile [tk_getOpenFile]
if {$myFile == ""} return

proc Prog {widget message operation percent} {

    puts "message=$message, operation=$operation, percent=$percent"
    if {$percent >= 20} {
	puts "We cancelled from script!"
	return -code break "Kilroy was here!"
    }
}
movie .m -file $myFile -progressproc Prog
pack .m -side top
pack [button .exp -text "Export" -command DoExport] -side bottom
proc DoExport {} {
    if {[catch {.m export} msg]} {
	tk_messageBox -message \
	  "This is how we catch breaks from our progress proc: $msg"
    }
}


