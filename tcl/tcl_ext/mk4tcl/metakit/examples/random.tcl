#! /usr/bin/env tclkit

# Test script to do 100,000 adds/mods/dels of strings in a bytes prop field.
# The changes are also applied to a list and compared regularly to make sure
# that the stored data matches exactly what the mirror list contains.
#
# This was an attempt to track down a problem reported for 2.4.4, but no
# problem was found with this script.
#
# Output:
#
#     0:      0 rows,        0 b  C==========C==========C==========C==========
#  4000:    774 rows,   820166 b  C==========C==========C==========C==========
#  8000:   1596 rows,  1724131 b  C==========C==========C==========C==========
# 12000:   2381 rows,  1940124 b  C==========C==========C==========C==========
# 16000:   3225 rows,  2453683 b  C==========C==========C==========C==========
# 20000:   4063 rows,  2830557 b  C==========C==========C==========C==========
# 24000:   4810 rows,  3273064 b  C==========C==========C==========C==========
# 28000:   5599 rows,  3730358 b  C==========C==========C==========C==========
# 32000:   6381 rows,  4136253 b  C==========C==========C==========C==========
# 36000:   7200 rows,  4591425 b  C==========C==========C==========C==========
# 40000:   8017 rows,  5117372 b  C==========C==========C==========C==========
# 44000:   8844 rows,  5579832 b  C==========C==========C==========C==========
# 48000:   9640 rows,  6071057 b  C==========C==========C==========C==========
# 52000:   9997 rows,  7158289 b  C==========C==========C==========C==========
# 56000:   9999 rows,  7821411 b  C==========C==========C==========C==========
# 60000:   9999 rows,  8251942 b  C==========C==========C==========C==========
# 64000:   9999 rows,  8560813 b  C==========C==========C==========C==========
# 68000:  10000 rows,  8781565 b  C==========C==========C==========C==========
# 72000:  10003 rows,  8910941 b  C==========C==========C==========C==========
# 76000:  10000 rows,  8975682 b  C==========C==========C==========C==========
# 80000:   9995 rows,  8975682 b  C==========C==========C==========C==========
# 84000:  10002 rows,  8975682 b  C==========C==========C==========C==========
# 88000:   9997 rows,  8975682 b  C==========C==========C==========C==========
# 92000:  10000 rows,  8975682 b  C==========C==========C==========C==========
# 96000:  10002 rows,  8975682 b  C==========C==========C==========C==========
# Done.
#
# -jcw, 29-4-2002

if {[catch {load "" Mk4tcl}]} { load ./Mk4tcl.so Mk4tcl }

# returns a random integer less than the specified limit
proc rand {limit} {
  return [expr {int(rand() * $limit)}]
}

# returns true a certain percentage of the time
proc onavg {percent} {
  return [expr {rand() * 100 < $percent}]
}

# use the same random sequence each time around
expr {srand(1234567)}

# make sure the rand function works
if 0 {
  foreach x {a a a a a a a a a a a a a a a a a a a a} {
    foreach y {a a a a a a a a a a a a a a a a a a a a} {
      puts -nonewline [rand 10]
      puts -nonewline [rand 10]
      puts -nonewline [rand 10]
    }
    puts ""
  }
  puts ""
  foreach x {a a a a a a a a a a a a a a a a a a a a} {
    foreach y {a a a a a a a a a a a a a a a a a a a a} {
      puts -nonewline [onavg 10]
      puts -nonewline [onavg 10]
      puts -nonewline [onavg 10]
    }
    puts ""
  }
  exit
}

file delete data.mk
mk::file open db data.mk -nocommit
mk::view layout db.v d:B

set mirror {}

set desiredsize	10000	;# rows
set minlength	90	;# bytes
set maxlength	1100	;# bytes
set emptypct	3	;# percent
set commitfreq	1000	;# count
set checkfreq	100	;# count
set displayfreq	4000	;# count
set runcount	100000	;# count

fconfigure stdout -buffering none

set x 10000000

for {set i 0} {$i < $runcount} {incr i} {
  set n [llength $mirror]
  if {[expr {$i % $displayfreq == 0}]} {
    puts -nonewline [format "\n%7d: %6d rows, %8d b  " \
    			$i [mk::view size db.v] [file size data.mk]]
  }
  if {[expr {$i % $commitfreq == 0}]} {
    puts -nonewline C
    mk::file commit db
  }
  if {[expr {$i % $checkfreq == 0}]} {
    puts -nonewline =
    if {[mk::view size db.v] != $n} {
      puts "\n### $i: wrong size [mk::view size db.v] != $n"
      error mismatch
    }
    for {set j 0} {$j < $n} {incr j} {
      if {[mk::get db.v!$j d] != [lindex $mirror $j]} {
        puts "\n### $i: mismatch [mk::get db.v!$j d] != [lindex $mirror $j]"
	error mismatch
      }
    }
  }

  # under 100 rows just add items
  set a [expr {$n < 100 ? 0 : [rand 5]}]
  
  # boundary cases 1 and 3 may become adds or deletes to reach desired size
  switch $a {
    1 { set a [expr {$n < $desiredsize ? 0 : 2}] }
    3 { set a [expr {$n > $desiredsize ? 4 : 2}] }
  }

  # construct a test data value of the specified size
  set l [expr {int(rand() * ($maxlength - $minlength)) + $minlength - 10}]
  set t "[incr x]: [string repeat . $l]"
  if {[onavg $emptypct]} { set t "" }

  # randomly pick an existing row to modify
  set p [rand $n]

  # now make the change, to the mirror data list and to the view
  switch $a {
    0 { # add
      lappend mirror $t
      mk::row append db.v d $t
    }
    2 { # modify
      lset mirror $p $t
      mk::set db.v!$p d $t
    }
    4 { # delete
      set mirror [lreplace $mirror $p $p]
      mk::row delete db.v!$p
    }
  }
}

puts "\nDone."
