# Some code that have provoked crashes.
package require QuickTimeTcl
wm title . {Testing}
set f [tk_getOpenFile]
if {$f == ""} {
    return
}
pack [text .t]
wm geometry . +100+100
proc NewTopMovie {wtop} {
    global  f
    toplevel $wtop
    set w $wtop.m
    pack [movie $w -file $f]
    update
}
proc Rotate {wtop} {
    foreach i {1 2 3} {
	lower $wtop
	update 
	after 1000
	raise $wtop
	update
	after 1000
    }
}
proc Cycle {wtop} {
    catch {destroy $wtop}
    NewTopMovie $wtop
    update
    after 1000
    destroy $wtop
}
NewTopMovie .mx
NewTopMovie .my
wm geometry .my +250+300
raise .my
Rotate .mx
destroy .my
after 1000
NewTopMovie .my
wm geometry .my +300+300
Cycle .my
Cycle .my
Cycle .my
Cycle .my
