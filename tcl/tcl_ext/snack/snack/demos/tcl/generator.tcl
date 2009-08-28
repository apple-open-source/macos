#!/bin/sh
# the next line restarts using wish \
exec wish8.4 "$0" "$@"

package require -exact snack 2.2

set f [snack::filter generator 440.0]
snack::sound s
#snack::audio playLatency 200

tk_optionMenu .m v(type) sine rectangle triangle sawtooth noise
foreach i [list 0 1 2 3 4] {
  .m.menu entryconfigure $i -command Config
}
pack .m -side bottom

pack [frame .fb] -side bottom
pack [button .fb.a -bitmap snackPlay -command Play] -side left
pack [button .fb.b -bitmap snackStop -command "s stop"] -side left

set v(freq) 440.0
set v(ampl) 20000

pack [frame .f] -expand yes -fill both -side top
pack [scale .f.s1 -label Frequency -from 4000 -to 50 -length 200\
        -variable v(freq) -command Config] -side left -expand yes -fill both
pack [scale .f.s2 -label Amplitude -from 32767 -to 0 -length 200\
        -variable v(ampl) -command Config] -side left -expand yes -fill both

proc Config {args} {
  global f v
  set shape 0.0
  set type $v(type)
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
  $f configure $v(freq) $v(ampl) $shape $type -1
}

proc Play {} {
  global f
  s stop
  s play -filter $f
}
