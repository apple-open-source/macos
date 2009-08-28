
# seq grabber
package require QuickTimeTcl
option add *Background gray87
option add *highlightBackground gray87
wm title . {Mats TV}
. configure -bg gray87
pack [frame .f -bd 1 -relief sunken] -padx 4 -pady 4
set w .f.sg
seqgrabber $w -size quarter
pack $w -padx 4 -pady 4

pack [frame .fr] -expand true -fill both
button .fr.bt1 -text { Zoom In } -command [list $w configure -zoom 2.0]
button .fr.bt2 -text {Zoom Out} -command [list $w configure -zoom 1.0]
grid .fr.bt1 .fr.bt2 -sticky ew -padx 10 -pady 4