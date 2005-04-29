# Blocked view deletion tests
# jcw, 26-3-2002

if [catch {package require Mk4tcl}] {
  catch {load ./Mk4tcl.so mk4tcl}
  catch {load ./Mk4tcl_d.dll mk4tcl}
}

proc fill {n} {
  $::bv size 0
  set ::vv {}
  for {set i 0} {$i < $n} {incr i} {
    $::bv insert end a $i
    lappend ::vv $i
  }
}

proc remove {from {count 1}} {
  incr count -1
  $::bv delete $from [incr count $from]
  set ::vv [lreplace $::vv $from $count]
}

proc check {} {
  set pos 0
  foreach y $::vv {
    set x [$::bv get $pos a]
    if {$x != $y} { error "pos $pos is $x, should be $y" }
    incr pos
  }
}

mk::file open db
mk::view layout db.v {{_B {a:I}}}

set bv [[mk::view open db.v] view blocked]

for {set j 1} {$j < 6} {incr j} {
  fill 2000
  remove 996 $j
  check
}

for {set j 988} {$j < 1001} {incr j} {
  fill 2000
  remove $j 10
  check
}

for {set j 1100} {$j < 1110} {incr j} {
  fill 3000
  remove 985 $j
  check
}

set total 50000
fill $total

foreach x {1 2 3 4 5 6 7 8 9 10 11 12 13 14 15} {
  puts -nonewline stderr "$x "
  for {set i [expr {$total-1025}]} {$i > 0} {incr i -1000} {
    remove $i 45
    remove $i 45
    incr total -90
  }
  check
}

puts stderr OK
