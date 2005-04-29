#!/bin/sh
# the next line restarts using wish \
exec wish8.4 "$0" "$@"

package require -exact snack 2.2

set f [snack::filter generator 440.0 30000 0.0 sine 8000]
snack::sound s
#snack::audio playLatency 100

wm resizable . 0 0

proc Beep {freq} {
  $::f configure $freq
  s stop
  s play -filter $::f
}

pack [button .b1 -text C4 -command [list Beep 261.6]] -side left
pack [button .b2 -text D4 -command [list Beep 293.7]] -side left
pack [button .b3 -text E4 -command [list Beep 329.7]] -side left
pack [button .b4 -text F4 -command [list Beep 349.3]] -side left
pack [button .b5 -text G4 -command [list Beep 392.1]] -side left
pack [button .b6 -text A4 -command [list Beep 440.0]] -side left
pack [button .b7 -text B4 -command [list Beep 493.9]] -side left
pack [button .b8 -text C5 -command [list Beep 523.3]] -side left
