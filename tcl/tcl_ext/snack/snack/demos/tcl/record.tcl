#!/bin/sh
# the next line restarts using wish \
exec wish8.4 "$0" "$@"

package require -exact snack 2.2

file delete _tmprec.wav
snack::sound t -debug 0
t write _tmprec.wav
snack::sound s -file _tmprec.wav -debug 0

set m [menu .menu]
$m add cascade -label File -menu $m.file -underline 0
menu $m.file -tearoff 0
$m.file add command -label "Open..." -command [list OpenSound]
$m.file add command -label "Save As..." -command [list SaveSound]
$m.file add command -label "Exit" -command exit
$m add cascade -label Audio -menu $m.audio -underline 0
menu $m.audio -tearoff 0
$m.audio add command -label "Settings..." -command Settings
$m.audio add command -label "Mixer..." -command snack::mixerDialog
. config -menu $m

snack::createIcons
pack [frame .f1] -pady 5
button .f1.bp -bitmap snackPlay -command Play
button .f1.bu -bitmap snackPause -command Pause
button .f1.bs -bitmap snackStop -command Stop
button .f1.br -bitmap snackRecord -command Record -fg red
pack .f1.bp .f1.bu .f1.bs .f1.br -side left
pack [frame .f2] -pady 5
label .f2.time -text "00:00.0" -width 10
snack::levelMeter .f2.lm
pack .f2.time .f2.lm -side left

wm protocol . WM_DELETE_WINDOW exit

proc OpenSound {} {
    set filename [snack::getOpenFile]
    s configure -file $filename
    SetTime [s length -unit sec]
}

proc SaveSound {} {
    set filename [snack::getSaveFile]
    s write $filename
}

proc Settings {} {
 set ::s(rate) [s cget -rate]
 set ::s(enc)  [s cget -encoding]
 set ::s(chan) [s cget -channels]

 set w .conv
 catch {destroy $w}
 toplevel $w
 wm title $w Settings

 frame $w.q
 pack $w.q -expand 1 -fill both -side top
 pack [frame $w.q.f1] -side left -anchor nw -padx 3m -pady 2m
 pack [frame $w.q.f2] -side left -anchor nw -padx 3m -pady 2m
 pack [frame $w.q.f3] -side left -anchor nw -padx 3m -pady 2m
 pack [frame $w.q.f4] -side left -anchor nw -padx 3m -pady 2m
 pack [label $w.q.f1.l -text "Sample Rate"]
 foreach e [snack::audio rates] {
  pack [radiobutton $w.q.f1.r$e -text $e -value $e -variable ::s(rate)] \
	  -anchor w
 }
 pack [entry $w.q.f1.e -textvariable ::s(rate) -width 6] -anchor w
 pack [label $w.q.f2.l -text "Sample Encoding"]
 foreach e [snack::audio encodings] {
  pack [radiobutton $w.q.f2.r$e -text $e -value $e -variable ::s(enc)] \
	  -anchor w
 }
 pack [label $w.q.f3.l -text Channels]
 pack [radiobutton $w.q.f3.1 -text Mono -value 1 -variable ::s(chan)] -anchor w
 pack [radiobutton $w.q.f3.2 -text Stereo -value 2 -variable ::s(chan)] \
	 -anchor w
 pack [entry $w.q.f3.e -textvariable ::s(chan) -width 3] -anchor w

 pack [ frame $w.f3]
 pack [ button $w.f3.b1 -text OK -width 6 \
	 -command "ApplySettings;destroy $w"] -side left
 pack [ button $w.f3.b2 -text Cancel -command "destroy $w"] -side left
}

proc ApplySettings {} {
 s configure -file ""
 s configure -rate $::s(rate) -channels $::s(chan) -encoding $::s(enc)
 t configure -rate $::s(rate) -channels $::s(chan) -encoding $::s(enc)
 t write _tmprec.wav
 s configure -file _tmprec.wav
}

proc SetTime {t} {
 set mmss [clock format [expr int($t)] -format "%M:%S"]
 .f2.time config -text $mmss.[format "%d" [expr int(10*($t-int($t)))]]
}

proc Update {} {
 if {$::op == "p"} {
  set t [audio elapsed]
  set end   [expr int([s cget -rate] * $t)]
  set start [expr $end - [s cget -rate] / 10]
  if {$start < 0} { set start 0}
  if {$end >= [s length]} { set end -1 }
  set l [s max -start $start -end $end]
 } else {
  set l [t max]
  t length 0
  set t [s length -unit sec]
 }
 SetTime $t
 .f2.lm configure -level $l
 
 after 100 Update
}

proc Record {} {
 s stop
 s configure -file _tmprec.wav
 s record
 t record
 set ::op r
 .f1.bp configure -relief raised
 .f1.br configure -relief groove
}

proc Play {} {
 t stop
 s stop
 s play -command Stop
 set ::op p
 .f1.bp configure -relief groove
 .f1.br configure -relief raised
 .f1.bu configure -relief raised
}

proc Stop {} {
 s stop
 t record
 set ::op s
 .f1.bp configure -relief raised
 .f1.br configure -relief raised
 .f1.bu configure -relief raised
}

proc Pause {} {
 s pause
 if {$::op != "s"} {
     if {[.f1.bu cget -relief] == "raised"} {
	 .f1.bu configure -relief groove
     } else {
	 .f1.bu configure -relief raised
     }
 }
}

t record
set op s
Update
