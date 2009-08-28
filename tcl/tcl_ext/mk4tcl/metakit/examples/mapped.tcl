# Some example and timing tests of the new hash/blocked/ordered views

if [catch {package require Mk4tcl}] {
  catch {load ./Mk4tcl.so mk4tcl}
  catch {load ./Mk4tcl_d.dll mk4tcl}
}

proc timedRun {tag count args} {
  set usec [lindex [time $args $count] 0]
  if {$usec >= 1000000} {
    set t [format {%.2f seconds} [expr {$usec/1000000.0}]]
  } elseif {$usec >= 1000} {
    set t [format {%.2f mSec} [expr {$usec/1000.0}]]
  } else {
    set t [format {%d uS} $usec]
  }
  puts [format {     %-10s %5dx -> %s} $tag $count $t]
  return $usec
}

proc setupPlain {} {
  global blocked_data

  file delete _large.mk
  mk::file open db _large.mk -nocommit

  if $blocked_data {
    mk::view layout db.words {{_B {k1 k2:I v:I}}}
    mk::view open db.words rawdata
    catch {rename words ""}
    rename [rawdata view blocked] words
  } else {
    mk::view layout db.words {k1 k2:I v:I}
    mk::view open db.words words
  }
}

proc teardownPlain {} {
  teardown
}

proc setupHash {} {
  global blocked_data blocked_map

  file delete _large.mk
  mk::file open db _large.mk -nocommit

  if $blocked_data {
    mk::view layout db.words {{_B {k1 k2:I v:I}}}
    mk::view open db.words rawdata
    catch {rename data ""}
    rename [rawdata view blocked] data
  } else {
    mk::view layout db.words {k1 k2:I v:I}
    mk::view open db.words data
  }

  if $blocked_map {
    mk::view layout db.words_map {{_B {_H:I _R:I}}}
    mk::view open db.words_map rawmap
    #catch {rename map ""}
    #rename [rawmap view blocked] map
    set map [rawmap view blocked]
  } else {
    mk::view layout db.words_map {_H:I _R:I}
    mk::view open db.words_map map
    set map map
  }

  #rename [data view hash map 2] words
  rename [data view hash $map 2] words
}

proc teardownHash {} {
  teardown
}

proc setupOrdered {} {
  global blocked_data

  file delete _large.mk
  mk::file open db _large.mk -nocommit

  if $blocked_data {
    mk::view layout db.words {{_B {k1 k2:I v:I}}}
    mk::view open db.words rawdata
    catch {rename data ""}
    rename [rawdata view blocked] data
  } else {
    mk::view layout db.words {k1 k2:I v:I}
    mk::view open db.words data
  }

  rename [data view ordered 2] words
}

proc teardownOrdered {} {
  teardown
}

proc setupBoth {} {
  global blocked_data blocked_map

  file delete _large.mk
  mk::file open db _large.mk -nocommit

  if $blocked_data {
    mk::view layout db.words {{_B {k1 k2:I v:I}}}
    mk::view open db.words rawdata
    catch {rename data ""}
    rename [rawdata view blocked] data
  } else {
    mk::view layout db.words {k1 k2:I v:I}
    mk::view open db.words data
  }

  if $blocked_map {
    mk::view layout db.words_map {{_B {_H:I _R:I}}}
    mk::view open db.words_map rawmap
    #catch {rename map ""}
    #rename [rawmap view blocked] map
    set map [rawmap view blocked]
  } else {
    mk::view layout db.words_map {_H:I _R:I}
    mk::view open db.words_map map
    set map map
  }

  #catch {rename hash ""}
  #rename [data view hash map 2] hash
  set hash [data view hash $map 2]

  #rename [hash view ordered 2] words
  rename [$hash view ordered 2] words
}

proc teardownBoth {} {
  teardown
}

proc teardown {} {
  rename words ""
  rename hash ""
  rename data ""
  rename map ""

  rename rawdata ""
  rename rawmap ""

  mk::file close db
}

proc filldb {n} {
  puts -nonewline stderr " filldb $n ... "
  set fd [open words]

  set n0 [words size]
  set t0 [clock clicks]
  while {[gets $fd w] >= 0} {
    words insert end k1 $w k2 $n v [expr {$n * [string length $w]}]
    #if {[words size] % 1000 == 0} {puts -nonewline stderr *}
  }
  set usec [expr {[clock clicks]-$t0}]

  close $fd
  set rps [expr {int(([words size]-$n0) / ($usec / 1000000.0))}]
  puts stderr "$rps adds/sec ($n0..[words size] rows)"
}

proc run {type bdata bmap runs finds} {
  global blocked_data blocked_map
  set blocked_data $bdata
  set blocked_map $bmap

  set s ""
  if $bdata {append s " - data is blocked"}
  if $bmap {append s " - map is blocked"}

  puts "\n*** $type$s ***\n"

  proc map {args} {return ?}
  proc rawmap {args} {return ?}
  proc data {args} {return ?}
  proc rawdata {args} {return ?}
  proc hash {args} {return ?}

  setup$type

  set runCounter 99

  while {[incr runs -1] >= 0} {
    timedRun Fill 1 filldb [incr runCounter]
    timedRun Commit 1 mk::file commit db
    timedRun Find $finds words find k1 iterator k2 $runCounter
  }

  puts " [words size] rows, [map size] hash slots,\
      file size = [file size _large.mk]"
  puts "     [rawdata size] data blocks, [rawmap size] map blocks"

  teardown$type
}

  # plain table, append at end, find with linear scan
run Plain 0 0 3 10
run Plain 1 0 3 10

  # hash table, with data and/or map optionally blocked
run Hash 0 0 3 1000
run Hash 0 1 3 1000
run Hash 1 0 3 1000
run Hash 1 1 3 1000

  # binary search, with data optionally blocked
run Ordered 0 0 3 1000
run Ordered 1 0 3 1000

  # combination of the above: hash access, rows kept in sort order
run Both 0 0 2 1000
run Both 0 1 2 1000
run Both 1 0 2 1000
run Both 1 1 2 1000

  # create larger datasets, this takes a long time
run Plain 1 0 10 10
run Hash  1 0 10 1000
run Ordered 1 0 10 1000
run Both  1 0 10 1000
