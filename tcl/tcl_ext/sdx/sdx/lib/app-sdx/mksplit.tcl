#! /usr/bin/env tclkit

# Split starkit/starpack file in two, create name.head and name.tail
# derived from mkinfo.tcl code
# Jan 2004, jcw@equi4.com

proc fail {msg} { puts stderr "${::filename}: $msg"; exit 1 }

if {[llength $argv] < 1} {
  set filename usage
  fail "$argv0 file ..."
}

foreach filename $argv {
  if {![file exists $filename]} {
    fail "file does not exist"
  }
  if {![file isfile $filename]} {
    fail "this is not a regular file (perhaps mounted as VFS?)"
  }
  set end [file size $filename]
  if {$end < 27} {
    fail "file too small, cannot be a datafile"
  }

  set fd [open $filename]
  fconfigure $fd -translation binary
  seek $fd -16 end
  binary scan [read $fd 16] IIII a b c d

 #puts [format %x-%d-%x-%d $a $b $c $d]

  if {($c >> 24) != -128} {
    fail "this is not a Metakit datafile"
  }

  # avoid negative sign / overflow issues
  if {[format %x [expr {$a & 0xffffffff}]] eq "80000000"} {
    set start [expr {$end - 16 - $b}]
  } else {
    # if the file is in commit-progress state, we need to do more
    fail "this code needs to be finished..."
  }

  seek $fd $start
  switch -- [read $fd 2] {
    JL { set endian little }
    LJ { set endian big }
    default { fail "failed to locate file header" }
  }

  seek $fd 0 end
  set remainder [expr {[tell $fd] - $start}]
  seek $fd 0
  set fn [file tail [file root $filename]]

  set ofd [open $fn.head w]
  fconfigure $ofd -translation binary
  fcopy $fd $ofd -size $start
  close $ofd
  puts "$fn.head: $start bytes"

  set ofd [open $fn.tail w]
  fconfigure $ofd -translation binary
  fcopy $fd $ofd -size $remainder
  close $ofd
  puts "$fn.tail: $remainder bytes"

  close $fd
}
