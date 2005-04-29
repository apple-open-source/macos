#!/bin/sh
# the next line restarts using wish \
exec wish8.4 "$0" "$@"

package require -exact snack 2.2

sound s
sound t

pack [frame .b] -side bottom
pack [button .b.r -bitmap snackRecord -command Start -fg red -width 40] \
    -side left  
pack [button .b.s -bitmap snackStop -command Stop -width 40] -side left
pack [button .b.p -bitmap snackPlay -command {Stop;s play} -width 40] \
    -side left
pack [label .b.l -text "Draw speed:"] -side left
tk_optionMenu .b.om pixpsec 25 50 100 200
pack .b.om -side left
pack [label .b.l2 -text "pixels per second"] -side left
pack [frame .f] -side top -expand true -fill both
pack [canvas .f.d -width 40 -bg white] -side left -fill y
pack [canvas .f.c -bg white] -side left -expand true -fill both
.f.c create text 150 100 -text "Pitch plot of microphone signal"
pack [canvas .c -bg white -width 200 -height 200]
.c create oval 100 100 100 100 -width 8 -outline black -tag point
.c create line 195 5 195 195 -fill red -arrow last
.c create line 5 5 195 5 -fill green -arrow first

set pixpsec 25
set samplePos 0
#.c create spectrogram 0 0 -sound s -height 200 -pixelspersec $pixpsec

proc Stop {} {
  s stop
  after cancel Draw
}

proc Start {} {
  Stop
  s record
  set ::samplePos 0
  set ::ox  0
  set ::oy  0
  .f.c delete all
  .f.c create line 0 $::ty 1280 $::ty -tags target
  after 50 Draw
}

proc Draw {} {
  set length [s length]
  while {$::samplePos < $length - 700-0*320} {
    t copy s -start $::samplePos -end [expr {$::samplePos+700+0*320}]
    set formants [lindex [t formant] end]
    set x [expr {$::ox + 0.01 * $::pixpsec}]
    set y [expr {[winfo height .f.c]*((4000-[lindex $formants 0])/4000.0)}]
    .f.c create oval $x $y $x $y -width 2 -outline red
    set y [expr {[winfo height .f.c]*((4000-[lindex $formants 1])/4000.0)}]
    .f.c create oval $x $y $x $y -width 2 -outline green
    set y [expr {[winfo height .f.c]*((4000-[lindex $formants 2])/4000.0)}]
    .f.c create oval $x $y $x $y -width 2 -outline blue
    set y [expr {[winfo height .f.c]*((4000-[lindex $formants 3])/4000.0)}]
    .f.c create oval $x $y $x $y -width 2 -outline yellow
    incr ::samplePos 160
    set ::ox $x
    if {$x > [winfo width .f.c]} Stop
    set y [expr {[winfo height .c]*(([lindex $formants 0]-0)/800.0)}]
    set x [expr {[winfo width .c]*((2300-[lindex $formants 1]+0)/2300.0)}]
    .c coords point $x $y $x $y
  }
  after 50 Draw
  if {[s length] > 320000} Stop
}

bind .f.c <Configure> Configure

proc Configure {} {
  .f.d delete all
  snack::frequencyAxis .f.d 0 0 40 [winfo height .f.c] -topfr 4000
}

set ty 150
.f.c create line 0 $::ty 1280 $::ty -tags target
bind .f.c <1> [list initDrag %x %y]
bind .f.c <B1-Motion> [list Drag %x %y]

proc initDrag {x y} {
  set ::ty [.f.c canvasy $y]
  .f.c coords target 0 $::ty 1280 $::ty
}

proc Drag {x y} {
  set y [.f.c canvasy $y]
  .f.c coords target 0 $::ty 1280 $::ty
  set ::ty $y
}
