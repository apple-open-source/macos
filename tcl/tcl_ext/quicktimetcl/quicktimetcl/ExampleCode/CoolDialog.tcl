
package require QuickTimeTcl
wm title . {Cool Dialog}
set myFile [tk_getOpenFile]
if {$myFile != ""} {
    movie .m -file $myFile -controller 0
    pack .m
    set theSize [.m size]
    set mw [lindex $theSize 0]
    set mh [lindex $theSize 1]
    update idletasks
    button .bt1 -text {Play Me} -command {.m play}
    button .bt2 -text Quit -command exit
    button .bt3 -text Stop -command {.m stop}
    place .bt1 -x 10 -y 10 -anchor nw
    place .bt2 -x [expr $mw - 10] -y [expr $mh - 10] -anchor se
    place .bt3 -x [expr $mw - 10] -y 10 -anchor ne
}
