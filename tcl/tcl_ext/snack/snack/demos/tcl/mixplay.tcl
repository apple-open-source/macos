#!/bin/sh
# the next line restarts using wish \
exec wish8.4 "$0" "$@"

package require -exact snack 2.2

# This example shows how to control the volume of two audio clips.

snack::sound s1
snack::sound s2

set map1 [snack::filter map 1.0]
set map2 [snack::filter map 1.0]

proc play {} {
 s1 play -filter $::map1
 s2 play -filter $::map2
}

proc stop {} {
 s1 stop
 s2 stop
}

proc configure { args } {
 $::map1 configure [$::ft.s1 get]
 $::map2 configure [$::ft.s2 get]
}

proc load1 {} {
 s1 configure -file [tk_getOpenFile]
}

proc load2 {} {
 s2 configure -file [tk_getOpenFile]
}

set t .
set ft [frame $t.ft]
set fb [frame $t.fb]
pack $fb -side bottom
pack $ft

scale $ft.s1 -label "sound 1" -from 1.0 -to 0.0 -resolution 0.01 -command configure
scale $ft.s2 -label "sound 2" -from 1.0 -to 0.0 -resolution 0.01 -command configure
pack $ft.s1 $ft.s2 -side left

pack [button $fb.bl1 -text "load 1" -command load1] -side left
pack [button $fb.bl2 -text "load 2" -command load2] -side left
pack [button $fb.bp -bitmap snackPlay -command play] -side left
pack [button $fb.bs -bitmap snackStop -command stop] -side left
