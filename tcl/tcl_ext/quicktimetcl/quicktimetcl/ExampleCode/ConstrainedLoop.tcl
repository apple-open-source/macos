# Constrained looping

package require QuickTimeTcl

proc StartConstrainedLoop { } {
    
    set start [.m time]
    set end [expr $start + 1200]
    .m configure -mccommand [list ControllerProc $start $end]
    MovieTimer .m $start $end
}

proc StopConstrainedLoop { } {
    global  timeId
    
    after cancel $timeId
    .m configure -mccommand {}
}
    
proc ControllerProc {start end w what {par {}}} {
    
    #puts "ControllerProc:: w=$w, what=$what, par=$par"
    if {$w == ".m"} {
	if {$what == "goToTime"} {
	    set movieTime [lindex $par 1]
	    if {$movieTime < $start || $movieTime > $end} {
		# Break
		return -code 3
	    }
	} elseif {$what == "play"} {

	} 
    }
}

proc MovieTimer {w start end} {
    global  timeId timeArr
    
    set movieTime [.m time]
    if {$movieTime > $end || $movieTime >= $timeArr(-movieduration)} {
	.m time $start
    }
    set timeId [after 30 MovieTimer $w $start $end]
}

set bgCol #dedede
. configure -bg $bgCol
wm resizable . 0 0

wm title . {Constrained Loop}
set myFile [tk_getOpenFile]
if {$myFile == ""} {
    exit
}
set theMovie [movie .m -file $myFile]
pack .m
array set timeArr [.m gettime]
pack [frame .fr -bg $bgCol] -side top -fill x
pack [button .fr.start -text { Start Loop } -command StartConstrainedLoop] \
  -side left -fill x
pack [button .fr.stop -text { Stop Loop } -command StopConstrainedLoop] \
  -side left -fill x
