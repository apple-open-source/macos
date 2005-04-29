# Trying to find out how hash performance degrades as size increases

if [catch {package require Mk4tcl}] {
  catch {load ./Mk4tcl[info sharedlibext] mk4tcl}
  catch {load ./Mk4tcl_d.dll mk4tcl}
}

proc loadwords {step} {
  global warray
  set fd [open /usr/share/dict/words]
  #set fd [open words]
  for {set i 0} {$i < $step && [gets $fd line] >= 0} {incr i} {
    set warray($line) $i
  }
  close $fd
  return $line
}

proc timedRun {tag count args} {
  set usec [lindex [time $args $count] 0]
  lappend ::stats($tag) [expr {$count*$usec/1000.0}]
}

proc setup {keys type} {
  file delete _large.mk
  mk::file open db _large.mk -nocommit

  set layout "$keys v"
  if {$type == "b"} { set layout "{_B {$layout}}" }
  mk::view layout db.words $layout

  if {$type == "b"} {
    mk::view open db.words rawdata
    rename [rawdata view blocked] data
  } else {
    mk::view open db.words data
  }
  
  mk::view layout db.words_map {_H:I _R:I}
  mk::view open db.words_map map

  rename [data view hash map [regsub -all k $keys {} x]] words
}

proc teardown {} {
  catch { rename words "" }
  catch { rename data "" }
  catch { rename map "" }
  catch { rename rawdata "" }

  mk::file close db
}

proc fill1 {seq} {
  global warray
  foreach {k v} [array get warray] {
    set x $k$seq 
    words insert end k1 k1_$x v v_$x
  }
}

proc fill2 {seq} {
  global warray
  foreach {k v} [array get warray] {
    set x $k$seq 
    words insert end k1 k1_$x k2 k2_$x
  }
}

proc fill5 {seq} {
  global warray
  foreach {k v} [array get warray] {
    set x $k$seq 
    words insert end k1 k1_$x k2 k2_$x v1 v1_$x v2 v2_$x v3 v3_$x v v_$x
  }
}

proc find1 {w} {
  words find k1 k1_${w}5
}

proc find2 {w} {
  words find k1 k1_${w}5 k2 k2_${w}5
}

proc find5 {w} {
  words find k1 k1_${w}5 k2 k2_${w}5
}

set step 10000
#set step 100
set mult 50

set w [loadwords $step]
puts "w = $w"
puts [clock format [clock seconds]]

foreach type {f b} {
  foreach keys {{k1} {k1 k2} {k1 k2 v1 v2 v3}} {
    set nkeys [llength $keys]
    set mode "$type$nkeys"
    puts -nonewline stderr "$mode: "
    setup $keys $type
    for {set i 0} {$i < $mult} {incr i} {
      timedRun $mode-fill 1 fill$nkeys $i
      puts -nonewline stderr .
    }
    timedRun $mode-find 10000 find$nkeys $w
    timedRun $mode-commit 1 mk::file commit db
    puts stderr "  [words size] rows, [file size _large.mk] b"
    for {set i 0} {$i < 3} {incr i} {
      puts " $i: [words get $i]"
    }
    teardown
  }
}

puts [clock format [clock seconds]]

puts -nonewline "\n               "
foreach x [lsort [array names stats *-fill]] {
  puts -nonewline [format %8s [lindex [split $x -] 0]]
  set i 0
  foreach y $stats($x) {
    incr i $step
    append cols([format %6d $i]) [format {%8.2f} $y]
  }
  unset stats($x)
}
puts \n
parray cols
puts ""
parray stats
puts ""
puts [clock format [clock seconds]]
