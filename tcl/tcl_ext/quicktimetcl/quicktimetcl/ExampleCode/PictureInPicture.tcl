
package require QuickTimeTcl
wm title . {Pict In Pict}
set myFile [tk_getOpenFile]
if {$myFile != ""} {
    movie .m -file $myFile
    pack .m
    set theSize [.m size]
    movie .n -file $myFile -controller 0  \
      -width [expr [lindex $theSize 0]/4]  \
      -height [expr [lindex $theSize 1]/4]
    place .n -x 10 -y 10 -anchor nw
}
