# Timing of the view sort operation

package require Mk4tcl

proc timedRun {tag count args} {
  set usec [lindex [time $args $count] 0]
  append ::stats($tag) [format {%9.2f} [expr {$count*$usec/1000.0}]]
}

proc fill {seq} {
  global warray
  foreach {k v} [array get warray] {
    mk::row append db.words text $k$seq
  }
}

set step 40000
set mult 7

#set fd [open /usr/share/dict/words]
set fd [open words]
for {set i 0} {$i < $step && [gets $fd line] >= 0} {incr i} {
  set warray($line) $i
}
close $fd

puts [clock format [clock seconds]]

file delete _large.mk
mk::file open db _large.mk -nocommit
mk::view layout db.words text

for {set i 0} {$i < $mult} {incr i} {
  append stats(count) [format {%9d} [expr {($i+1)*$step}]]
  timedRun fill 1 fill $i
  timedRun sort 1 mk::select db.words -sort text
  puts -nonewline stderr .
}

timedRun commit 1 mk::file commit db
puts stderr "  [mk::view size db.words] rows, [file size _large.mk] b"

for {set i 0} {$i < 3} {incr i} {
  puts " $i: [mk::get db.words!$i]"
}

mk::file close db

puts [clock format [clock seconds]]

puts ""
parray stats
puts ""
puts [clock format [clock seconds]]
