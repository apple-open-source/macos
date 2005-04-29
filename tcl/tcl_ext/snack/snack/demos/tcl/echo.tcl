#!/bin/sh
# the next line restarts using wish \
exec wish8.3 "$0" "$@"

package require -exact snack 2.2

set f [snack::filter echo 0.6 0.6 30 0.4 50 0.3]

snack::sound s

pack [frame .f]

set v(iGain) 0.6
pack [scale .f.s1 -label InGain -from 1.0 -to 0.0 -resolution .01 \
	-variable v(iGain) -command Config] -side left

set v(oGain) 0.6
pack [scale .f.s2 -label OutGain -from 1.0 -to 0.0 -resolution .01 \
	-variable v(oGain) -command Config] -side left

set v(delay1) 30.0
pack [scale .f.s3 -label Delay1 -from 250.0 -to 10.0 -variable v(delay1) \
	-command Config] -side left 

set v(decay1) 0.4
pack [scale .f.s4 -label Decay1 -from 1.0 -to 0.0 -resolution .01 \
	-variable v(decay1) -command Config] -side left 

set v(delay2) 50.0
pack [scale .f.s5 -label Delay2 -from 250.0 -to 10.0 -variable v(delay2) \
	-command Config] -side left 

set v(decay2) 0.3
pack [scale .f.s6 -label Decay2 -from 1.0 -to 0.0 -resolution .01 \
	-variable v(decay2) -command Config] -side left 

snack::createIcons
pack [frame .fb]
pack [button .fb.a -image snackOpen -command Load] -side left
pack [button .fb.b -bitmap snackPlay -command Play] -side left
pack [button .fb.c -bitmap snackStop -command "s stop"] -side left

proc Config {args} {
  global f v
  $f configure $v(iGain) $v(oGain) $v(delay1) $v(decay1) $v(delay2) $v(decay2)
}

proc Play {} {
  s stop
  s play -filter $::f
}

proc Load {} {
  set file [snack::getOpenFile -initialdir [file dirname [s cget -file]]]
  if {$file == ""} return
  s config -file $file
}
