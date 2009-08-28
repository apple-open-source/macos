# Let's store 100 million integers

catch { load ../builds/.libs/libmk4tcl[info sharedlibextension] Mk4tcl }
puts "[info script] - Mk4tcl [package require Mk4tcl] - $tcl_platform(os)"

file delete bigdata.mk
mk::file open db bigdata.mk
mk::view layout db.v {{_B {a:I}}}

set bv [[mk::view open db.v] view blocked]

set n 0

puts " filled    commit   #blocks   filesize   memuse        total"

for {set i 0} {$i < 20} {incr i} {

  set s [clock seconds]
  for {set j 0} {$j < 5000000} {incr j} {
    $bv insert end a [incr n]
  }
  set t [expr {[clock seconds] - $s}]
  set u [expr {[lindex [time {mk::file commit db}] 0]/1000.0}]
  set v [mk::view size db.v]
  set w [file size bigdata.mk]
  set x [lindex [exec ps l --noheader [pid]] 7]
  set y [$bv size]

  puts [format {%6s s %7.1f ms %6d %10d b %6d K %10d rows} $t $u $v $w $x $y]
}
