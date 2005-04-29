#!/bin/sh
# the next line restarts using wish \
exec wish8.4 "$0" "$@"

package require -exact snack 2.2

snack::sound s

array set map {
    0 2
    1 3
    2 4
    3 5
    4 7
    5 9
    6 12
    7 15
    8 19
    9 23
    10 28
    11 34
    12 41
    13 49
    14 56
    15 63
}

pack [canvas .c -width 140 -height 100]

for {set i 0} {$i<16} {incr i} {
  .c create rect [expr 10*$i] 50 [expr 10*$i+10] 100 -fill green  -outline ""
  .c create rect [expr 10*$i] 20 [expr 10*$i+10] 50  -fill yellow -outline ""
  .c create rect [expr 10*$i] 0  [expr 10*$i+10] 20  -fill red   -outline ""
  .c create rect [expr 10*$i] 0  [expr 10*$i+10] 100 -fill black -tag c$i
}
for {set i 0} {$i<17} {incr i} {
  .c create line 0 [expr 6*$i] 140 [expr 6*$i] -width 3
  .c create line [expr 10*$i] 0 [expr 10*$i] 100 -width 5
}

pack [frame .f]
pack [button .f.a -text On -command On] -side left
pack [button .f.b -text Off -command {s stop}] -side left

proc On {} {
  s record
  after 100 Draw
}

proc Draw {} {
  if {[s length] > 129} {
    set spec [s dBPower -fftlen 128 -windowlength 128]
    s length 0
    for {set i 0} {$i < 16} {incr i} {
      set val [lindex $spec $::map($i)]
      .c coords c$i [expr 10*($i-2)] 0 [expr 10*($i-2)+9] \
	      [expr 100-1.4*($val+100)]
    }
  }
  if ![snack::audio active] return
  after 100 Draw
}
