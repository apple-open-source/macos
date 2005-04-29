#!/bin/sh
# the next line restarts using wish \
exec wish8.4 "$0" "$@"

package require -exact snack 2.2

pack [canvas .c -width 300 -height 300]

snack::sound s -load ex1.wav

.c create section 0 0 -sound s -start 6000 -end 6100 -height 300 -width 300

pack [button .bClose -text Close -command exit]
