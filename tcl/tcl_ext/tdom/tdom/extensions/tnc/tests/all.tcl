# all.tcl --
#
# This file contains a top-level script to run all of the Tcl
# tests.  Execute it by invoking "tclsh all.test".
#
# Copyright (c) 1998-1999 by Scriptics Corporation.
# All rights reserved.
#
# RCS: @(#) $Id: all.tcl,v 1.1 2003/02/22 21:19:59 rolf Exp $
# 

source [file join [file dir [info script]] loadtnc.tcl]

if {$tcl_version >= 8.1} {
    if {[lsearch [info proc ::tcltest::testConstraint] \
            ::tcltest::testConstraint] == -1} {
        set ::tcltest::testConfig(need_i18n) 1
        set ::tcltest::testConstraints(need_i18n) 1
   } else {
        ::tcltest::testConstraint need_i18n 1
    }
}

set timeCmd {clock format [clock seconds]}

set ::tcltest::testSingleFile false

puts stdout "Tcl $tcl_patchLevel tests running in interp:  [info nameofexecutable]"

if {$tcl_version < 8.2} {
    set TESTS_DIR [file join [pwd] [file dirname [info script]]]
    set currentDir [pwd]

    set globPattern [file join $TESTS_DIR *.test]
    foreach file [lsort [glob $globPattern]] {
        set tail [file tail $file]
        if {[string match l.*.test $tail]} {
            # This is an SCCS lockfile; ignore it
            continue
        }
        puts stdout $tail
        if {[catch {source $file} msg]} {
            puts stdout $msg
        }
    }
} else {
    set ::tcltest::testsDirectory [file dir [info script]]

    puts stdout "Tests running in working dir:  $::tcltest::testsDirectory"
    if {[llength $::tcltest::skip] > 0} {
        puts stdout "Skipping tests that match:  $::tcltest::skip"
    }
    if {[llength $::tcltest::match] > 0} {
        puts stdout "Only running tests that match:  $::tcltest::match"
    }

    if {[llength $::tcltest::skipFiles] > 0} {
        puts stdout "Skipping test files that match:  $::tcltest::skipFiles"
    }
    if {[llength $::tcltest::matchFiles] > 0} {
        puts stdout "Only sourcing test files that match:  $::tcltest::matchFiles"
    }

    puts stdout "Tests began at [eval $timeCmd]"

    # source each of the specified tests
    foreach file [lsort [::tcltest::getMatchingFiles]] {
        set tail [file tail $file]
        puts stdout $tail
        if {[catch {source $file} msg]} {
            puts stdout $msg
        }
    }
} 

# cleanup
puts stdout "\nTests ended at [eval $timeCmd]"
::tcltest::cleanupTests 1

# Just a dirty trick, to make life of mem leak debuggers a bit easier.
# See http://mini.net/tcl/3248
proc exit args {}

