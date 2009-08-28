
package require QuickTimeTcl
wm title . {Resize Button}
set myFile [tk_getOpenFile]
if {$myFile != ""} {
    movie .m -file $myFile -resizable 1
    pack .m
    button .bt -text {Just Padding}
    pack .bt
}
