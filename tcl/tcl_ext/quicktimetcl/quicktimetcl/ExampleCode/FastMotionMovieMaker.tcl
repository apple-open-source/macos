# seq grabber
package require QuickTimeTcl
wm title . {Fast Motion}

set bgCol #dedede
. configure -bg $bgCol
wm resizable . 0 0
option add *Frame.Background $bgCol
option add *Label.Background $bgCol

set wgrabber .sfr.sg
frame .sfr -relief sunken -bd 1 -bg $bgCol
seqgrabber $wgrabber
pack .sfr -padx 8 -pady 8 -side top
pack $wgrabber -padx 8 -pady 8

frame .fr
set wbt [frame .fr.fbt]
set wfr [frame .fr.f]
set wfr2 [frame .fr.f2]
set wbtstart $wbt.start
pack .fr
pack $wbt -pady 4
pack $wfr -pady 4
pack $wfr2 -pady 4

set wmovie .mfr.m
frame .mfr -relief sunken -bd 1 -bg $bgCol
pack .mfr -padx 8 -pady 8 -side top

pack [button $wbtstart -text Start -width 8 -command Start] -side left -padx 10 -pady 4
pack [button $wbt.btmm -text "Make Movie" -command MakeMovie] -side left -padx 10 -pady 4
label $wfr.sla -text "Shot interval: " -font System
spinbox $wfr.min -from 0 -to 60 -increment 1 -width 2 -textvariable shotinterval(min) -font System
label $wfr.slamin -text "Minutes" -font System
spinbox $wfr.secs -from 0 -to 59 -increment 1 -width 2 -textvariable shotinterval(secs) -font System
label $wfr.slasec -text "Seconds" -font System
grid $wfr.sla $wfr.min $wfr.slamin $wfr.secs $wfr.slasec -padx 2 -pady 2 -anchor w

label $wfr2.lpho -text "Number of Photos:"
label $wfr2.npho -textvariable uid
grid $wfr2.lpho $wfr2.npho -padx 2 -pady 2 -anchor w

set shotinterval(min) 0
set shotinterval(secs) 5
set uid 0

set cachePath [file join [file dirname [info script]] cache]
if {![file isdirectory $cachePath]} {
    file mkdir $cachePath
}

proc Start { } {
    global wbtstart wgrabber uid shotinterval afterid width height cachePath
    
    foreach f [glob -nocomplain -directory $cachePath *.png] {
	file delete $f
    }
    set width [winfo width $wgrabber]
    set height [winfo height $wgrabber]
    set nphotos 0
    
    $wbtstart configure -text Stop -command Stop
    set uid 0
    Picture
}

proc Stop { } {
    global wbtstart afterid
    
    after cancel $afterid
    $wbtstart configure -text Start -command Start
}

proc Picture { } {
    global wgrabber cachePath uid afterid shotinterval
    
    set imName [image create photo]
    $wgrabber picture $imName
    set filename [file join $cachePath "image[incr uid].png"]
    $imName write $filename -format quicktimepng
    image delete $imName
    set ms [expr 1000 * ($shotinterval(min) * 60 + $shotinterval(secs))]
    set afterid [after $ms Picture]
}

proc MakeMovie { } {
    global cachePath width height wmovie
    
    catch {destroy $wmovie}
    movie $wmovie
    $wmovie new [file join $cachePath themovie.mov]
    set res [$wmovie tracks new video $width $height]
    set trackID [lindex $res 1]
    
    set allFiles [lsort -dictionary [glob -nocomplain -directory $cachePath *.png]]
    
    # Batch to be economical.
    set len [llength $allFiles]
    set nbatch 12
    set i 0
    set fileBatchList {}
    while {$i < $len} {
	lappend fileBatchList [lrange $allFiles $i [expr $i + $nbatch - 1]]
	incr i $nbatch
    }
    set fps 10
    set duration [expr 600/$fps]
    set starttime 0
    
    foreach fileBatch $fileBatchList {
	set imageList {}
	foreach f $fileBatch {
	    lappend imageList [image create photo -file $f]
	}
	$wmovie tracks add picture $trackID $starttime $duration $imageList
	  #-compressor mp4v -spatialquality high -temporalquality high
	eval {image delete} $imageList
	incr starttime [expr $nbatch * $duration]
    }
    pack $wmovie -padx 8 -pady 8
    $wmovie save
}
    
    
