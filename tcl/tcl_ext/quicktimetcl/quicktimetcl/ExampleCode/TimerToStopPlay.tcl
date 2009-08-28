# This is just atest of how accurate the timer works in conjunction
# with start/stop of movie playing.
# 
# This is outdated! Use the new callback command from 3.1 instead!

package require QuickTimeTcl
    
proc ControllerProc {w what {par {}}} {
    global  timeId start
    
    #puts "what=$what, par=$par"
    if {$what == "play" && $par > 0.99} {
	set start [.m time]
	puts "movie time $start"
	# 2 secs
	set timeId [after 2000 MovieTimer]
    }
}
proc MovieTimer { } {
    global start scale
    
    .m stop
    set time [.m time]
    puts "movie time $time"
    set diffms [expr 1000.0 * ($time - $start)/double($scale) - 2000 ]
    puts "difference in millisecs: $diffms"
}
set bgCol #dedede
. configure -bg $bgCol
wm resizable . 0 0

wm title . {Constrained Loop}
set myFile [tk_getOpenFile]
if {$myFile == ""} {
    exit
}
set theMovie [movie .m -file $myFile -mccommand ControllerProc]
pack .m
array set timeArr [.m gettime]
set scale $timeArr(-movietimescale)
puts "movie timescale: $scale"
