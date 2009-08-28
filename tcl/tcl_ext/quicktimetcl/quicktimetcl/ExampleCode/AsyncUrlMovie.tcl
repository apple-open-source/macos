
# load remote movie asynchronously
package require QuickTimeTcl

proc MyLoadCheck {w msg {err {}}} {
    puts "w=$w, msg=$msg, err=$err"
    if {[llength $err]} {
	puts "Failed"
    } elseif {(($msg == "playable") || ($msg == "complete")) && ![winfo ismapped $w]} {
	pack $w
	update
    }
}
quicktimetcl::debuglevel 4
set f "http://hem.fyristorg.com/matben/qt/welcome.mov"
set f "http://www.visit.se/~matben/merry.mp3"
#set f http://coccinella.sourceforge.net/merry.mp3
#set f "http://192.168.0.2/dinosaur-c-240.mov"
movie .m -url $f -loadcommand MyLoadCheck
