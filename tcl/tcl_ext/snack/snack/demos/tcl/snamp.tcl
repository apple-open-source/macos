#!/bin/sh
# the next line restarts using wish \
exec wish8.4 "$0" "$@"

package require -exact snack 2.2
# Try to load optional file format handlers
catch { package require snacksphere }
catch { package require snackogg }

# If they are present add new filetypes to file dialogs
set extTypes  {}
set loadTypes {}
set loadKeys  {}
set saveTypes {}
set saveKeys  {}
if {[info exists snack::snacksphere]} {
    lappend extTypes {SPHERE .sph} {SPHERE .wav}
    lappend loadTypes {{SPHERE Files} {.sph}} {{SPHERE Files} {.wav}}
    lappend loadKeys SPHERE SPHERE
}
if {[info exists snack::snackogg]} {
  lappend extTypes  {OGG .ogg}
  lappend loadTypes {{Ogg Vorbis Files} {.ogg}}
  lappend loadKeys  OGG
  lappend saveTypes {{Ogg Vorbis Files} {.ogg}}
  lappend saveKeys  OGG
}
snack::addExtTypes $extTypes
snack::addLoadTypes $loadTypes $loadKeys
snack::addSaveTypes $saveTypes $saveKeys


snack::debug 0
snack::sound s -debug 0
snack::sound s2
snack::sound sa

set timestr ""
option add *font {Helvetica 10 bold}
wm title . "Snack Audio MPEG Player"

if 0 {
set draw 1
pack [frame .f]
pack [canvas .f.c -width 140 -height 40] -side left
pack [checkbutton .f.a -text Analyzer -variable draw] -side left

for {set i 0} {$i<16} {incr i} {
  .f.c create rect [expr 10*$i] 20 [expr 10*$i+10] 40 -fill green  -outline ""
  .f.c create rect [expr 10*$i] 10 [expr 10*$i+10] 20 -fill yellow -outline ""
  .f.c create rect [expr 10*$i] 0  [expr 10*$i+10] 10 -fill red   -outline ""
  .f.c create rect [expr 10*$i] 0  [expr 10*$i+10] 40 -fill black -tag c$i
}
for {set i 0} {$i<17} {incr i} {
  .f.c create line [expr 10*$i] 0 [expr 10*$i] 40 -width 5
}
for {set i 0} {$i<7} {incr i} {
  .f.c create line 0 [expr 6*$i] 140 [expr 6*$i] -width 3
}
}

pack [frame .frame] -side top -expand yes -fill both
scrollbar .frame.scroll -command ".frame.list yview"
listbox .frame.list -yscroll ".frame.scroll set" -setgrid 1 -selectmode single -exportselection false -height 16
pack .frame.scroll -side right -fill y
pack .frame.list -side left -expand 1 -fill both
bind .frame.list <Double-ButtonPress-1> Play
bind .frame.list <B1-Motion> {Drag %y}
bind .frame.list <ButtonPress-1> {Select %y}
bind . <BackSpace> Cut

snack::createIcons
pack [frame .panel] -side bottom -before .frame
pack [button .panel.bp -bitmap snackPlay -command Play] -side left
pack [button .panel.bs -bitmap snackStop -command Stop] -side left
pack [button .panel.bo -image snackOpen -command Open] -side left
set p 0
pack [scale .panel.ss -show no -orient horiz -len 130 -var p] -side left
set gain [snack::audio play_gain]
pack [scale .panel.sv -show no -orient horiz -command {snack::audio play_gain}\
	-len 70 -var gain] -side left
set setdrag 1
bind .panel.ss <ButtonPress-1> {set setdrag 0}
bind .panel.ss <ButtonRelease-1> {set setdrag 1 ; Play2}
pack [label .panel.l -textvar timestr]

proc Open {} {
    global files
    set file [snack::getOpenFile -format MP3]
    if {$file != ""} {
	set name [file tail $file]
	set files($name) $file
	.frame.list insert end $name
    }
}

proc Play args {
    global files t0 filelen
    if {[.frame.list curselection] == ""} {
	set i 0
    } else {
	set i [lindex [.frame.list curselection] 0]
    }
    .frame.list selection set $i
    Stop
    s config -file $files([.frame.list get $i])
    sa config -file $files([.frame.list get $i])
    if {$args == ""} {
	s play -command Next
	set t0 [clock scan now]
    } else {
	s play -start $args -command Next
	set t0 [expr [clock scan now] - $args / [s cget -rate]]
    }
    set filelen [s length]
    Timer
}

proc Play2 {} {
    global filelen p
    Play [expr int($p/100.0*[s length])]
}

proc Stop {} {
    s stop
    after cancel Timer
}

proc Timer {} {
    global t0 timestr setdrag
    set time [expr [clock scan now] - $t0]
    set timestr [clock format $time -format "%M:%S"]
    if $setdrag {
	.panel.ss set [expr int(100 * $time / [s length -unit sec])]
    }
#    Draw
    after 100 Timer
}

proc Next {} {
    set i [lindex [.frame.list curselection] 0]
    if {$i == ""} return
    .frame.list selection clear $i
    incr i
    .frame.list selection set $i
    .frame.list see $i
    after 10 Play
}

set cut ""
proc Cut {} {
    global cut
    if {[.frame.list curselection] != ""} {
	set cut [.frame.list get [.frame.list curselection]]
	.frame.list delete [.frame.list curselection]
    }
}

proc Select y {
    global old timestr files
    set old [.frame.list nearest $y]
    s2 config -file $files([.frame.list get $old])
    set timestr [clock format [expr int([s2 length -unit sec])] -format "%M:%S"]
}

proc Drag y {
    global old
    set new [.frame.list nearest $y]
    if {$new == -1} return
    set tmp [.frame.list get $old]
    .frame.list delete $old
    .frame.list insert $new $tmp
    .frame.list selection set $new
    set old $new
}

array set map {
    0 2
    1 3
    2 4
    3 5
    4 7
    5 9
    6 12
    7 15
    8 19
    9 23
    10 28
    11 34
    12 41
    13 49
    14 56
    15 63
}

proc Draw {} {
    global draw
    if ![snack::audio active] return
    if {$draw == 1} {
puts [time {
	set pos [expr int([s cget -rate] * [snack::audio elapsed])]
        if {$pos > 1000} {
         set junk [sa sample [expr $pos - 1000]]
         set junk [sa sample [expr $pos - 100]]
         set junk [sa sample [expr $pos]]
puts $junk
	}
	set spec [sa dBPower -start $pos -fftlen 128 -windowlength 128]
	for {set i 0} {$i < 16} {incr i} {
	    set val [lindex $spec $::map($i)]
	    .f.c coords c$i [expr 10*($i-2)] 0 [expr 10*($i-2)+9] \
		    [expr 100-1.4*($val+100)]
	}
    }]
    }
}

if [info exists argv] {
 if [file isdirectory $argv] {
  catch {cd $argv}
 }
}

wm protocol . WM_DELETE_WINDOW exit

if {$::tcl_platform(platform) == "windows"} {
 set filelist [glob -nocomplain *.mp3 *.wav]
} else {
 set filelist [glob -nocomplain *.mp3 *.wav *.MP3 *.WAV]
}
if {[info exists snack::snackogg]} {
  set filelist [concat $filelist [glob -nocomplain *.ogg *.OGG]]
}
foreach file [lsort -dictionary $filelist] {
    set name [file tail $file]
    set files($name) $file
    .frame.list insert end $file
}
