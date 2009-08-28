# Simple edit

package require QuickTimeTcl
proc ControllerProc { w what {par {}} } {
    global  selStart selDur movieTime timeId movieTimeHms selStartHms selDurHms
    
    #puts "ControllerProc:: w=$w, what=$what, par=$par"
    if {$w == ".fr.m"} {
	if {$what == "setSelectionBegin"} {
	    set selStart $par
	    set selStartHms [converttohmstime $w $selStart]
	} elseif {$what == "setSelectionDuration"} {
	    set selDur $par
	    set selDurHms [converttohmstime $w $selDur]
	    update
	} elseif {$what == "goToTime"} {
	    set movieTime [lindex $par 1]
	    set movieTimeHms [converttohmstime $w $movieTime]
	    update
	} elseif {$what == "play"} {
	    if {$par != 0.0} {
		#set movieTime ""
		set timeId [after 100 MovieTimer $w]
	    } else {
		catch {after cancel $timeId}
		set movieTime [$w time]
		set movieTimeHms [converttohmstime $w $movieTime]
	    }
	} 
    }
}

proc converttohmstime {movie timemovie} {

    array set timearr [$movie gettime]
    set hunsecs [format {%02i}    \
      [expr 100 * ($timemovie % $timearr(-movietimescale))/ \
      $timearr(-movietimescale)]]
    set totsecs [expr $timemovie/$timearr(-movietimescale)]
    set totmins [expr $totsecs/60]
    set tothours [expr $totmins/60]
    set secs [format {%02i} [expr $totsecs % 60]]
    set mins [format {%02i} [expr $totmins % 60]]
    
    return "${tothours}:${mins}:${secs}.${hunsecs}"
}

proc MovieTimer {w} {
    global  timeId movieTime movieTimeHms
    
    set movieTime [$w time]
    set movieTimeHms [converttohmstime $w $movieTime]
    set timeId [after 100 MovieTimer $w]
}

# We use a variable 'this(platform)' that is more convenient for MacOS X.
switch -- $tcl_platform(platform) {
    unix {
	set thisPlatform $tcl_platform(platform)
	if {[package vcompare [info tclversion] 8.3] == 1} {	
	    if {[string equal [tk windowingsystem] "aqua"]} {
		set thisPlatform "macosx"
	    }
	}
    }
    windows - macintosh {
	set thisPlatform $tcl_platform(platform)
    }
}

if {[string match "mac*" $thisPlatform]}  {
    set sysFont(s) {Geneva 9 normal}
    set sysFont(sb) {Geneva 9 bold}
    set osmod Command
} elseif {[string equal $thisPlatform "windows"]}  {
    set sysFont(s) {Arial 8 normal}
    set sysFont(sb) {Arial 8 bold}
    set osmod Control
}
set bgCol #dedede
. configure -bg $bgCol
wm resizable . 0 0

wm title . {Simple Edit}
set myFile [tk_getOpenFile]
if {$myFile == ""} {
    exit
}

frame .fr -relief sunken -bd 1 -bg $bgCol
set theMovie [movie .fr.m -file $myFile -mccommand ControllerProc -mcedit 1]
pack .fr -padx 8 -pady 8
pack .fr.m -padx 8 -pady 8
pack [frame .fr2 -bg $bgCol] -fill both -expand 1 -anchor w -padx 2 -side top
label .fr2.lab1 -bg $bgCol -anchor w \
  -text "Shift drag controller to select" -font $sysFont(s)
label .fr2.labtime -bg $bgCol -anchor w \
  -text "Movie time" -font $sysFont(s)
label .fr2.labstart -bg $bgCol -anchor w \
  -text "Select start" -font $sysFont(s)
label .fr2.labdur -bg $bgCol -anchor w \
  -text "Select duration" -font $sysFont(s)
entry .fr2.enttime -width 8 -bg $bgCol -textvariable movieTime \
  -highlightthickness 0 -bd 1 -relief sunken -font $sysFont(s)
entry .fr2.entstart -width 8 -bg $bgCol -textvariable selStart \
  -highlightthickness 0 -bd 1 -relief sunken -font $sysFont(s)
entry .fr2.entdur -width 8 -bg $bgCol -textvariable selDur \
  -highlightthickness 0 -bd 1 -relief sunken -font $sysFont(s)
entry .fr2.enthms -width 8 -bg $bgCol -textvariable movieTimeHms \
  -highlightthickness 0 -bd 1 -relief sunken -font $sysFont(s)
entry .fr2.entstarthms -width 8 -bg $bgCol -textvariable selStartHms \
  -highlightthickness 0 -bd 1 -relief sunken -font $sysFont(s)
entry .fr2.entdurhms -width 8 -bg $bgCol -textvariable selDurHms \
  -highlightthickness 0 -bd 1 -relief sunken -font $sysFont(s)

grid .fr2.lab1 -column 0 -row 0 -columnspan 2 -sticky w -pady 0 -padx 2
grid .fr2.labtime .fr2.labstart .fr2.labdur -sticky news -pady 1 -padx 2
grid .fr2.enttime .fr2.entstart .fr2.entdur -sticky news -pady 3 -padx 2
grid .fr2.enthms .fr2.entstarthms .fr2.entdurhms -sticky news -pady 4 -padx 2

# Find first video track if any.
set videoTrack -1
if {[$theMovie isvisual]} {
    set desc [$theMovie tracks full]
    foreach trackDescList $desc {
	array set arrDesc $trackDescList
	if {$arrDesc(-mediatype) == "vide"} {
	    set videoTrack $arrDesc(-trackid)
	    break
	}
    }
}
set undoNo -1

menu .menu -tearoff 0
set m [menu .menu.file -tearoff 0]
.menu add cascade -label {File } -menu $m
$m add command -label {Save} -accelerator $osmod+S -command {
    set fileName [$theMovie save]
}
$m add command -label {Save As...} -command {
    tk_messageBox -icon info -type ok -message  \
      {The movie may not be saved self contained with this command.\
      If you want to save a self contained movie, use the Flatten command.}
    set f [tk_getSaveFile]
    if {$f != ""} {
	set fileName [$theMovie saveas $f]
    }
}
$m add command -label {Flatten...} -command {
    set f [tk_getSaveFile]
    if {$f != ""} {
	$theMovie flatten $f
    }
}
$m add command -label {Compress...} -command {
    set f [tk_getSaveFile]
    if {$f != ""} {
	$theMovie compress $f 1
    }
}
$m add command -label {Export...} -accelerator $osmod+E -command {
    set fileName [$theMovie export]
}
$m add command -label "Quit" -accelerator $osmod+Q -command exit

set m [menu .menu.edit -tearoff 0]
.menu add cascade -label {Edit } -menu $m
$m add command -label {Undo} -accelerator $osmod+Z -command {
    if {$undoNo >= 0} {
	$theMovie undo $undoNo
	incr undoNo -1
    }
}
$m add command -label {Cut} -accelerator $osmod+X -command {
    set undoList [$theMovie cut]
    set undoNo [lindex [lindex $undoList 0] 1]
}
$m add command -label {Copy} -accelerator $osmod+C -command {
    set undoList [$theMovie copy]
    set undoNo [lindex [lindex $undoList 0] 1]
}
$m add command -label {Paste} -accelerator $osmod+V -command {
    set undoList [$theMovie paste]
    set undoNo [lindex [lindex $undoList 0] 1]
}
if {$tcl_platform(platform) == "macintosh"} {
    $m add command -label {Paste...} -command {
	set undoList [$theMovie paste dialog]
	set undoNo [lindex [lindex $undoList 0] 1]
    }
}
$m add command -label {Add} -command {
    set undoList [$theMovie add]
    set undoNo [lindex [lindex $undoList 0] 1]
}
$m add sep
$m add command -label {Zero Source Effect...} -state disabled -command {
    set selection [$theMovie select]
    set from [lindex $selection 0]
    set duration [lindex $selection 1]
    $theMovie effect $from $duration
}
$m add command -label {One Source Effect...} -state disabled -command {
    set selection [$theMovie select]
    set from [lindex $selection 0]
    set duration [lindex $selection 1]
    $theMovie effect $from $duration $videoTrack
}
if {$videoTrack >= 0} {
    .menu.edit entryconfigure "*Zero*" -state normal
    .menu.edit entryconfigure "*One*" -state normal
}
. configure -menu .menu

