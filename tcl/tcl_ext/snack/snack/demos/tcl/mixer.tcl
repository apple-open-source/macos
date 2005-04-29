#!/bin/sh
# the next line restarts using wish \
exec wish8.4 "$0" "$@"

# A cross-platform mixer application that adapts to the capabilities
# of Snack on the machine it is run on.
# Lots of functionality on Linux - play volume only on Windows, currently.

package require -exact snack 2.2

proc Update {} {
  snack::mixer update
  if $::doMonitor { after 100 Update }
}
bind . <Configure> Update
wm protocol . WM_DELETE_WINDOW exit

pack [frame .f] -expand yes -fill both
pack [checkbutton .r -text Monitor -command Update -variable doMonitor]

foreach line [snack::mixer lines] {
  pack [frame .f.g$line -bd 1 -relief solid] -side left -expand yes -fill both
  pack [label .f.g$line.l -text $line]
  if {[snack::mixer channels $line] == "Mono"} {
    snack::mixer volume $line v(r$line)
  } else {
    snack::mixer volume $line v(l$line) v(r$line)
    pack [scale .f.g$line.e -from 100 -to 0 -show no -var v(l$line)] -side \
	    left -expand yes -fill both
  }
  pack [scale .f.g$line.s -from 100 -to 0 -show no -var v(r$line)] -expand yes\
	  -fill both
}

pack [frame .f.f2] -side left

if {[llength [snack::mixer inputs]] > 0} {
  pack [label .f.f2.li -text "Input jacks:"]
  foreach jack [snack::mixer inputs] {
    snack::mixer input $jack v(i$jack)
    pack [checkbutton .f.f2.b$jack -text $jack -variable v(i$jack)] -anc w
  }
}
if {[llength [snack::mixer outputs]] > 0} {
  pack [label .f.f2.lo -text "Output jacks:"]
  foreach jack [snack::mixer outputs] {
    snack::mixer output $jack v(o$jack)
    pack [checkbutton .f.f2.b$jack -text $jack -variable v(o$jack)] -anc w
  }
}
