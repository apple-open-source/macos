# common test setup script

if {[info exists testsInited]} return

package require Tcl 8.4

if {[lsearch [namespace children] ::tcltest] == -1} {
  package require tcltest 2.2
  namespace import tcltest::*
}

singleProcess true ;# run without forking

testsDirectory [file dirname [info script]]

# if run from the tests directory, move one level up
if {[pwd] eq [testsDirectory]} {
  cd ..
}

temporaryDirectory [pwd]
#workingDirectory [file dirname [testsDirectory]]

# TextMate support on Mac OS X: run make before running any test from editor
if {[info exists env(TM_FILENAME)]} {
  if {[catch { exec make } msg]} {
    puts stderr $msg
    exit 1
  }
}

proc readfile {filename} {                                                      
  set fd [open $filename]                                                       
  set data [read $fd]                                                           
  close $fd                                                                     
  return $data                                                                  
}

# extract version number from pkgIndex.tcl
regexp {ifneeded Mk4tcl\s(\S+)\s} [readfile [workingDirectory]/pkgIndex.tcl] - version
unset -

# make sure the pkgIndex.tcl is found
if {[lsearch $auto_path [workingDirectory]] < 0} {
  set auto_path [linsert $auto_path 0 [workingDirectory]]
}

testConstraint 64bit            [expr {$tcl_platform(wordSize) == 8}]
testConstraint bigendian        [expr {$tcl_platform(byteOrder) eq "bigEndian"}]
testConstraint tcl$tcl_version  1

proc assert {ok} {
  if {!$ok} {
    return -code error "assertion failed"
  }
}

proc equal {expected result} {
  if {$expected ne $result} {
    return -code error [list expected $expected got $result]
  }
}

proc match {pattern result} {
  if {![string match $pattern $result]} {
    return -code error [list pattern $pattern does not match $result]
  }
}

set testsInited 1
