#!/usr/bin/env tclsh
#
# all.tcl --
#
# Run all Tcl tests in this directory.
#

package require tcltest 2.2

eval tcltest::configure $argv
tcltest::configure -testdir [file dir [info script]]
#tcltest::configure -verbose [concat [tcltest::configure -verbose] start]

tcltest::runAllTests
