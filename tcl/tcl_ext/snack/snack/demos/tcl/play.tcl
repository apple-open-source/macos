#!/bin/sh
# the next line restarts using tclsh \
exec tclsh8.4 "$0" "$@"

package require -exact sound 2.2
# Try to load optional file format handlers
catch { package require snacksphere }
catch { package require snackogg }

if {[llength $argv] == 0} {
  puts {Usage: play.tcl file}
  exit
} else {
  set file [lindex $argv 0]
}

snack::sound s -file $file
s play -block 1
