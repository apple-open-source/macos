#!/bin/sh
# the next line restarts using wish \
exec wish8.4 "$0" "$@"

package require -exact snack 2.2

snack::sound s -channels 2

pack [frame .a]
pack [frame .b]
pack [snack::levelMeter .a.left  -width 20 -length 200 \
	-orient horizontal]
pack [snack::levelMeter .a.right -width 20 -length 200 \
	-orient horizontal]
pack [snack::levelMeter .b.left  -width 20 -length 200 \
	-orient vertical -oncolor green] -side left
pack [snack::levelMeter .b.right -width 20 -length 200 \
	-orient vertical -oncolor orange] -side left

s record
after 100 Update

proc Update {} {
  set l [s max -start 0 -end -1 -channel 0]
  set r [s max -start 0 -end -1 -channel 1]
  s length 0

  .a.left  configure -level $l
  .a.right configure -level $r
  .b.left  configure -level $l
  .b.right configure -level $r

  after 100 Update
}
