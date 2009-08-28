# all.tcl -- -*- tcl -*-
#
# This file contains a top-level script to run all of the Tcl
# tests.  Execute it by invoking "source all.test" when running tcltest
# in this directory.
#
# Copyright (c) 1998-2000 by Scriptics Corporation.
# Copyright (c) 2002         Andreas Kupries <andreas_kupries@users.sourceforge.net>
# All rights reserved.
#
# RCS: @(#) $Id: all.tcl 156 2008-10-23 15:52:49Z nijtmans $

if {[lsearch [namespace children] ::tcltest] == -1} {
    package require tcltest
    namespace import ::tcltest::*
}

proc run_tests {} {
    set chan $::tcltest::outputChannel

    puts $chan "Tests running in interp:       [info nameofexecutable]"
    puts $chan "Tests running with pwd:        [pwd]"
    puts $chan "Tests running in working dir:  $::tcltest::testsDirectory"

    if {[llength $::tcltest::skip] > 0} {
	puts $chan "Skipping tests that match:            $::tcltest::skip"
    }
    if {[llength $::tcltest::match] > 0} {
	puts $chan "Only running tests that match:        $::tcltest::match"
    }
    if {[llength $::tcltest::skipFiles] > 0} {
	puts $chan "Skipping test files that match:       $::tcltest::skipFiles"
    }
    if {[llength $::tcltest::matchFiles] > 0} {
	puts $chan "Only sourcing test files that match:  $::tcltest::matchFiles"
    }

    set timeCmd {clock format [clock seconds]}
    puts $chan "Tests began at [eval $timeCmd]"

    # source each of the specified tests
    foreach file [lsort [::tcltest::getMatchingFiles]] {
	set tail [file tail $file]
	puts $chan $tail
	if {[catch {source $file} msg]} {
	    puts $chan $msg
	}
    }

    # cleanup
    puts $chan "\nTests ended at [eval $timeCmd]"
    ::tcltest::cleanupTests 1
    return
}
