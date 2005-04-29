#!/bin/sh
# the next line restarts using wish \
exec wish8.4 "$0" "$@"

package require -exact snack 2.2

snack::sound s1 -channels 2
set leftMap [snack::filter map 1 0 0 0]
set left(generator) [snack::filter generator 440 20000 0.0 sine -1]
set leftFilter [snack::filter compose $left(generator) $leftMap]

snack::sound s2 -channels 2
set rightMap [snack::filter map 0 0 0 1]
set right(generator) [snack::filter generator 440 20000 0.0 sine -1]
set rightFilter [snack::filter compose $right(generator) $rightMap]

pack [frame .fb] -side bottom
pack [button .fb.a -bitmap snackPlay -command Play] -side left
pack [button .fb.b -bitmap snackStop -command "snack::audio stop"] -side left

set left(freq) 1000.0
set left(ampl) 20000
set right(freq) 2200.0
set right(ampl) 20000

pack [frame .left] -expand yes -fill both -side top
pack [label .left.l -text "Left channel "] -side left
pack [scale .left.s1 -label Frequency -from 4000 -to 50 -length 200\
        -variable left(freq) -command [list Config left]] -side left -expand yes -fill both
pack [scale .left.s2 -label Amplitude -from 32767 -to 0 -length 200\
        -variable left(ampl) -command [list Config left]] -side left -expand yes -fill both
tk_optionMenu .left.m1 left(type) sine rectangle triangle sawtooth noise
foreach i [list 0 1 2 3 4] {
 .left.m1.menu entryconfigure $i -command [list Config left]
}
pack .left.m1 -side left

pack [frame .right] -expand yes -fill both -side top
pack [label .right.l -text "Right channel"] -side left
pack [scale .right.s1 -label Frequency -from 4000 -to 50 -length 200\
        -variable right(freq) -command [list Config right]] -side left -expand yes -fill both
pack [scale .right.s2 -label Amplitude -from 32767 -to 0 -length 200\
        -variable right(ampl) -command [list Config right]] -side left -expand yes -fill both
tk_optionMenu .right.m2 right(type) sine rectangle triangle sawtooth noise
foreach i [list 0 1 2 3 4] {
 .right.m2.menu entryconfigure $i -command [list Config right]
}
pack .right.m2 -side left

proc Config {f args} {
  set shape 0.0
  upvar $f lf
  set type $lf(type)
  switch $type {
    sine {
      set shape 0.0
    }
    rectangle {
      set shape 0.5
    }
    triangle {
      set shape 0.5
    }
    sawtooth {
      set shape 0.0
      set type triangle
    }
  }
  $lf(generator) configure $lf(freq) $lf(ampl) $shape $type -1
}

proc Play {} {
  snack::audio stop
  s1 play -filter $::leftFilter
  s2 play -filter $::rightFilter
}
