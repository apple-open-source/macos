
package require QuickTimeTcl
wm title . {Memory Leaks?}
wm resizable . 0 0
if {[string compare $tcl_platform(platform) "macintosh"] == 0}  {
    tk_messageBox -icon info -message {Due to the 'info script' bug
    on Mac you need to manually select the drop.mov file in the
    ExampleCode folder in the following dialog} -type ok
    set f [tk_getOpenFile -defaultextension .mov]
    set fontSize 18
} elseif {[string compare $tcl_platform(platform) "windows"] == 0}  {
    set dir [file dirname [info script]]
    set f [file join $dir drop.mov]
    set fontSize 14
    puts "pwd=[pwd], dir=$dir"
}
movie .m -file $f -controller 0 -loopstate 1
pack .m
set theSize [.m size]
set mw [lindex $theSize 0]
set mh [lindex $theSize 1]
update idletasks
button .bt1 -text {Plug Leak}
place .bt1 -x 10 -y [expr $mh - 10] -anchor sw
button .bt2 -text {Help}
place .bt2 -x 10 -y [expr $mh - 40] -anchor sw
button .bt3 -text {Quit} -command exit
place .bt3 -x [expr $mw - 10] -y [expr $mh - 10] -anchor se
label .lb -text {If you have memory leaks, fix them!} -bg white  \
  -fg red -font "Helvetica $fontSize"
place .lb -x 10 -y 10 -anchor nw
update
.m play