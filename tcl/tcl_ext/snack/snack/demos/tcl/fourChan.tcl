#!/bin/sh
# the next line restarts using wish \
exec wish8.3 "$0" "$@"

package require -exact snack 2.2

# Create a four channel sound with some computed waveform data

sound s -channels 4 -rate 8000
s length 16000
set p 16
set n 0
for {set i 0} {$i < [expr int([s length] / $p -1)]} {incr i} {
    s sample [incr n] 0          0  10000  10000
    s sample [incr n] 3827    7071  10000 -10000
    s sample [incr n] 7071   10000  10000  10000  
    s sample [incr n] 9239    7071  10000 -10000
    s sample [incr n] 10000      0  10000  10000
    s sample [incr n] 9239   -7071  10000 -10000
    s sample [incr n] 7071  -10000  10000  10000
    s sample [incr n] 3827   -7071  10000 -10000
    s sample [incr n] 0          0 -10000  10000
    s sample [incr n] -3827   7071 -10000 -10000
    s sample [incr n] -7071  10000 -10000  10000
    s sample [incr n] -9239   7071 -10000 -10000
    s sample [incr n] -10000     0 -10000  10000
    s sample [incr n] -9239  -7071 -10000 -10000
    s sample [incr n] -7071 -10000 -10000  10000
    s sample [incr n] -3827  -7071 -10000 -10000
}

# Filters for channel selection

set f1 [snack::filter map 1 0 0 0 1 0 0 0]
set f2 [snack::filter map 0 1 0 0 0 1 0 0]
set f3 [snack::filter map 0 0 1 0 0 0 1 0]
set f4 [snack::filter map 0 0 0 1 0 0 0 1]
set f5 [snack::filter map 0 0 1 0 0 1 0 0]
set f6 [snack::filter map 0 1 0 0 0 0 1 0]

pack [frame .a] -side left
pack [label .a.l -text "Sound channels 1-4"]
pack [canvas .a.c -width 256 -height 200]
.a.c create waveform 0   0 -sound s -channe 0 -end 128 -width 256 -height 50
.a.c create waveform 0  50 -sound s -channe 1 -end 128 -width 256 -height 50
.a.c create waveform 0 100 -sound s -channe 2 -end 128 -width 256 -height 50
.a.c create waveform 0 150 -sound s -channe 3 -end 128 -width 256 -height 50
pack [frame .f] -side left
pack [label .f.l -text "Play channels:"]
pack [button .f.a1 -text "Default 1,2"  -wi 12 -command "s play"]
pack [button .f.a2 -text "Channel 1"    -wi 12 -command "s play -filter $f1"]
pack [button .f.a3 -text "Channel 2"    -wi 12 -command "s play -filter $f2"]
pack [button .f.a4 -text "Channel 3"    -wi 12 -command "s play -filter $f3"]
pack [button .f.a5 -text "Channel 4"    -wi 12 -command "s play -filter $f4"]
pack [button .f.a6 -text "Channels 3,2" -wi 12 -command "s play -filter $f5"]
pack [button .f.a7 -text "Channels 2,3" -wi 12 -command "s play -filter $f6"]
