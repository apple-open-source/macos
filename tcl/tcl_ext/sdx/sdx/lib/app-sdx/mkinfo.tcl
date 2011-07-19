#! /usr/bin/env tclkit

# Show byte offset where MK data starts in a file and the view structure
# Nov 2002, jcw@equi4.com

proc fail {msg} { puts stderr " $msg"; exit 1 }

if {[llength $argv] < 1} {
  fail "usage: $argv0 file ..."
}

foreach filename $argv {
  puts $filename

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
  close $fd

  puts "  Metakit data starts at offset $start and is stored as $endian-endian"

  if {[catch {
    package require vlerq

    set db [vlerq open $filename]

    # MK files always have exactly one row with only top-level views
    if {[vlerq get $db #] == 1 && [regexp {^[ V]+$} [vlerq types $db]]} {
      foreach x [vlerq names $db] {
	set v [vlerq get $db 0 $x]
	puts "[format %7d [vlerq size $v]]x $x\[[vlerq structdesc $v]\]"
      }
    } else {
      puts "  Structure:"
      puts "    [vlerq structdesc $db]"
      puts "  (this file cannot be used with Metakit 2.4.x)"
    }
  }]} {
    mk::file open db $filename -readonly
    puts "  Views:"
    foreach x [mk::file views db] {
      puts "[format %7d [mk::view size db.$x]]x\
			[list $x [mk::view layout db.$x]]"
    }
    mk::file close db
  }
}
