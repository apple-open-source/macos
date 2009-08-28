
package require QuickTimeTcl
wm title . {Simple Playback}
set myFile [tk_getOpenFile]
if {$myFile != ""} {
    movie .m -file $myFile
    pack .m
}
