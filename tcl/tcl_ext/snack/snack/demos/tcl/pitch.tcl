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
  after 200 Draw
}

proc Draw {} {
  set length [s length]  
  while {$::samplePos < $length - 666-1*320} {
    t copy s -start $::samplePos -end [expr {$::samplePos+665+1*320}]
   set pitch [lindex [lindex [t pitch -method esps] 2] 0]
    set x [expr {$::ox + 0.01 * $::pixpsec}]
    set y [expr {[winfo height .f.c]*((300-$pitch)/300.0)}]
    if {$::oy == 0} { set ::oy $y }
    if {$pitch > 0.0 && abs($::oy-$y) < 10} {
      .f.c create oval [expr {$x-1}] [expr {$y-1}] [expr {$x+1}] [expr {$y+1}]
    }
    incr ::samplePos 160
    set ::ox $x
    set ::oy $y
    if {$x > [winfo width .f.c]} Stop
  }
  after 50 Draw
  if {[s length] > 320000} Stop
}

bind .f.c <Configure> Configure

proc Configure {} {
  .f.d delete all
  snack::frequencyAxis .f.d 0 0 40 [winfo height .f.c] -topfr 300
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
