
package require QuickTimeTcl 3.1

set myFile [tk_getOpenFile]
if {$myFile == ""} {
    return
}

# Get screen size.
set swidth  [winfo screenwidth .]
set sheight [winfo screenheight .]

set w .t
toplevel $w -background black
wm overrideredirect $w 1
wm geometry $w ${swidth}x${sheight}+0+0
bind $w <Command-q> exit
bind $w <Command-.> exit
bind $w <Escape> exit

set m $w.m
movie $m -file $myFile -controller 0
foreach {mwidth mheight} [$m size] break
array set timeArr [$m gettime]

# Buttons.
set dir [file dirname [info script]]
set imquit [image create photo -file [file join $dir quit.gif]]
set imstop [image create photo -file [file join $dir stop.gif]]
set implay [image create photo -file [file join $dir start.gif]]
set f [frame $w.f -bg black]
label $f.ss   -bg black -bd 0 -image $implay
label $f.quit -bg black -bd 0 -image $imquit
pack $f.ss $f.quit -side left -padx 12 -pady 6
pack $f -side bottom
bind $f.ss   <Button-1> {Command play}
bind $f.quit <Button-1> exit

# Keep proportions.
set rw [expr double($swidth)/$mwidth]
set rh [expr double($sheight)/$mheight]
if {$rw < $rh} {
    set newwidth $swidth
    set newheight [expr int($rw * $mheight)]
    set side left
} else {
    set newheight $sheight
    set newwidth [expr int($rh * $mwidth)]
    set side top
}
proc End {token w mtime} {
    exit
}
proc Command {what} {
    global w implay imstop
    
    $w.m $what
    switch -- $what {
	play {
	    bind $w.f.ss <Button-1> {Command stop}
	    $w.f.ss configure -image $imstop
	}
	stop {
	    bind $w.f.ss <Button-1> {Command play}
	    $w.f.ss configure -image $implay
	}
    }
}
if {[tk windowingsystem] eq "aqua"} {
    quicktimetcl::systemui allhidden
}
$m configure -width $newwidth -height $newheight
pack $m -side $side
raise $w
update
Command play
$m callback $timeArr(-movieduration) End


