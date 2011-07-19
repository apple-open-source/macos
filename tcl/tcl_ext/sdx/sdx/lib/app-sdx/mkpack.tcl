#! /usr/bin/env tclkit

# Re-pack a starkit/starpack or MK datafile (header is kept intact)
# Jan 2004, jcw@equi4.com

proc fail {msg} { puts stderr "${::inf}: $msg"; exit 1 }

if {[llength $argv] != 2} {
  set inf usage
  fail "$argv0 infile outfile"
}

set inf [lindex $argv 0]
set ouf [lindex $argv 1]

if {[file normalize $inf] eq [file normalize $ouf]} {
  fail "input and output may not be the same file"
}
if {![file exists $inf]} {
  fail "file does not exist"
}
if {![file isfile $inf]} {
  fail "this is not a regular file (perhaps mounted as VFS?)"
}
set end [file size $inf]
if {$end < 27} {
  fail "file too small, cannot be a datafile"
}

set fd [open $inf]
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
  default { fail "failed to locate data header" }
}
seek $fd 0

mk::file open db $inf -readonly

set ofd [open $ouf w]
fconfigure $ofd -translation binary
fcopy $fd $ofd -size $start
mk::file save db $ofd
close $ofd

puts "$inf: [file size $inf] bytes"
puts "$ouf: [file size $ouf] bytes"

close $fd
