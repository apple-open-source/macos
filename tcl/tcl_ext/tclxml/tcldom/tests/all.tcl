# This file contains a top-level script to run all of the Tcl
# tests.  Execute it by invoking "source all" when running tclTest
# in this directory.
#
# Copyright (c) 2000 Ajuba Solutions
#
# SCCS: @(#) all 1.8 97/08/01 11:07:14

if {[lsearch [namespace children] ::tcltest] == -1} {
	package require tcltest
	namespace import ::tcltest::*
}

set ::tcltest::testSingleFile false
set ::tcltest::testsDirectory [file dir [info script]]

# source each of the specified tests
foreach file [lsort [::tcltest::getMatchingFiles]] {
	set tail [file tail $file]
	puts stdout $tail
	if {[catch {source $file} msg]} {
		puts stdout $msg
	}
}

# cleanup
::tcltest::cleanupTests 1
return
