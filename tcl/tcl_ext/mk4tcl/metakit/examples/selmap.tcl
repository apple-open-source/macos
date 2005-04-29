# How to map a Tcl selection result back to a view
#
# Note: it'd be nice to have a "$view loop var { script ... }" in Mk4tcl

if {[catch {package require Mk4tcl}] &&
    [catch {load ./Mk4tcl.so mk4tcl}] &&
    [catch {load ../builds/Mk4tcl.so mk4tcl}] &&
    [catch {load ./Mk4tcl_d.dll mk4tcl}] &&
    [catch {load ../builds/Mk4tcl_d.dll mk4tcl}]} {
  error "cannot load Mk4tcl"
}

mk::file open db
mk::view layout db.squares {x:I y:I}

set count 50000
mk::view size db.squares $count

set t [clock seconds]

mk::loop c db.squares {
  set i [mk::cursor position c]
  mk::set $c x $i y [expr {$i*$i}]
}

puts "init took [expr {[clock seconds] - $t}] seconds"

puts "select timing: [time {set v [mk::select db.squares -regexp y 11111]}]"

puts "results from select:"
foreach i $v {
  foreach {x y} [mk::get db.squares!$i x y] break
  puts [format {%7i: %10d,%d} $i $x $y]
}

set v1 [mk::view new]
foreach i $v {
  $v1 insert end n:I $i
}

puts "contents of v1:"
for {set i 0} {$i < [$v1 size]} {incr i} {
  puts "  [$v1 get $i n]"
}

set v2 [mk::view open db.squares]
puts "the squares view is called '$v2' and contains [$v2 size] rows"

set v3 [$v2 view map $v1]

puts "mapped view:"
for {set i 0} {$i < [$v3 size]} {incr i} {
  foreach {x y} [$v3 get $i x y] break
  puts [format {%7i: %10d,%d} $i $x $y]
}

$v3 close
$v2 close
$v1 close

# The output on this script should be:
# 
# init took 1 seconds
# select timing: 443096 microseconds per iteration
# results from select:
#   10541:      10541,111112681
#   33334:      33334,1111155556
#   48310:      48310,-1961111196
# contents of v1:
#   10541
#   33334
#   48310
# the squares view is called 'view1' and contains 100000 rows
# mapped view:
#       0:      10541,111112681
#       1:      33334,1111155556
#       2:      48310,-1961111196
