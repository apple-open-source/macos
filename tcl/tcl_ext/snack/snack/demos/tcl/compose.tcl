#!/bin/sh
# the next line restarts using wish \
exec wish8.3 "$0" "$@"

package require -exact snack 2.2

snack::sound s

set f0 [snack::filter map 0.5]
set f1 [snack::filter echo 0.6 0.6 30 0.4]
set f2 [snack::filter echo 0.6 0.6 50 0.3]
set f3 [snack::filter compose $f0 $f1 $f2]

pack [frame .f]
pack [frame .f.f1 -borderwidth 2 -relief raised]
pack [frame .f.f2 -borderwidth 2 -relief raised]
pack [frame .f.f3 -borderwidth 2 -relief raised]

set m1 1.0
pack [label .f.f1.l -text "Map Filter"]
pack [scale .f.f1.s0 -label Map -from 1.0 -to 0.0 -resolution .01 \
	-variable m1 -command "$f0 configure"]

set v(inGain1) 0.6
pack [label .f.f2.l -text "Echo Filter 1"]
pack [scale .f.f2.s1 -label InGain -from 1.0 -to 0.0 -resolution .01 \
	-variable v(inGain1) -command Config1] -side left

set v(outGain1) 0.6
pack [scale .f.f2.s2 -label OutGain -from 1.0 -to 0.0 -resolution .01 \
	-variable v(outGain1) -command Config1] -side left

set v(delay1) 30.0
pack [scale .f.f2.s3 -label Delay -from 250.0 -to 10.0 -variable v(delay1) \
	-command Config1] -side left 

set v(decay1) 0.4
pack [scale .f.f2.s4 -label Decay -from 1.0 -to 0.0 -resolution .01 \
	-variable v(decay1) -command Config1] -side left 

set v(inGain2) 0.7
pack [label .f.f3.l -text "Echo Filter 2"]
pack [scale .f.f3.s1 -label InGain -from 1.0 -to 0.0 -resolution .01 \
	-variable v(inGain2) -command Config2] -side left

set v(outGain2) 0.5
pack [scale .f.f3.s2 -label OutGain -from 1.0 -to 0.0 -resolution .01 \
	-variable v(outGain2) -command Config2] -side left

set v(delay2) 50.0
pack [scale .f.f3.s5 -label Delay -from 250.0 -to 10.0 -variable v(delay2) \
	-command Config2] -side left 

set v(decay2) 0.3
pack [scale .f.f3.s6 -label Decay -from 1.0 -to 0.0 -resolution .01 \
	-variable v(decay2) -command Config2] -side left 

snack::createIcons
pack [frame .fb]
pack [button .fb.a -image snackOpen -command Load] -side left
pack [button .fb.b -bitmap snackPlay -command Play] -side left
pack [button .fb.c -bitmap snackStop -command "s stop"] -side left

proc Config1 {args} {
    global f1 v
    $f1 configure $v(inGain1) $v(outGain1) $v(delay1) $v(decay1)
}

proc Config2 {args} {
    global f2 v
    $f2 configure $v(inGain2) $v(outGain2) $v(delay2) $v(decay2)
}

proc Play {} {
    global f3
    s play -filter $f3
}

proc Load {} {
 set file [snack::getOpenFile -initialdir [file dirname [s cget -file]]]
 if {$file == ""} return
 s config -file $file
}
