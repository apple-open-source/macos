# Some code that...
package require QuickTimeTcl
wm title . {Testing}
set f [tk_getOpenFile]
if {$f == ""} {
    return
}
pack [movie .m -file $f -volume 20]
set num 4
for {set no 1} {$no <= $num} {incr no} {
    toplevel .m$no
    set w .m$no.m
    movie $w -file $f
    pack $w
}
update
set width [winfo width .m1.m]
set height [winfo height .m1.m]
array set timeArr [.m1.m gettime]
for {set no 1} {$no <= $num} {incr no} {
    .m$no.m time $timeArr(-movieduration)
}
for {set no 1} {$no <= [expr $num/2]} {incr no} {
    set no2 [expr $num/2 + $no]
    set x [expr 20 + ($no-1) * ($width + 10)]
    wm geometry .m$no +$x+20
    wm geometry .m$no2 +$x+[expr 40 + $height]
}
after 1000
raise .
proc MoveTop {} {
    global  height width num
    for {set x 0} {$x<[expr $width*$num/2+100]} {incr x 50} {
	wm geometry . +$x+[expr $height/2]
	update
	after 500
    }
}
update
.m play
MoveTop
destroy .m1
MoveTop
destroy .m3
MoveTop
