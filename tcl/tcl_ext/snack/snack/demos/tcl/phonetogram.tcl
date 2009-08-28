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
pack [checkbutton .b.l -text "Erase dots" -variable erase] -side left
pack [frame .f] -side top -expand true -fill both
pack [canvas .f.c -bg white] -side left -expand true -fill both
.f.c create text 130 100 -text "Phonetogram plot (pitch and intensity)"

set samplePos 0
set erase 0

proc Stop {} {
  s stop
  after cancel Draw
}

proc Start {} {
  Stop
  s record
  set ::samplePos 0
  .f.c delete all
  after 200 Draw
}

proc Draw {} {
  if {$::erase} { .f.c delete all }
  set length [s length]  
  while {$::samplePos < $length - 666-1*320} {
    t copy s -start $::samplePos -end [expr {$::samplePos+665+1*320}]
    t changed new
    set pitch [lindex [lindex [t pitch -method esps] 2] 0]
    set amplitude [t max]
    if {$amplitude < 1} { set amplitude 1 }
    set y [expr {[winfo height .f.c]*(2.0-log10($amplitude)/2.26)}]
    set x [expr {[winfo width .f.c]*($pitch/300.0)}]
    if {$pitch > 0.0} {
      .f.c create oval [expr {$x-1}] [expr {$y-1}] [expr {$x+1}] [expr {$y+1}]
    }
    incr ::samplePos 160
  }
  after 50 Draw
  if {[s length] > 320000} Stop
}
