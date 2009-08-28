
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

if {[tk windowingsystem] == "aqua"} {
    quicktimetcl::systemui allhidden
}
$m configure -width $newwidth -height $newheight
pack $m -side $side
raise $w
update
$m play
$m callback $timeArr(-movieduration) End
