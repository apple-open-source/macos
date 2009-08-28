# all.tcl --
#
# This file contains a top-level script to run all of the Tcl
# tests.  Execute it by invoking "source all.test" when running tcltest
# in this directory.
#
# Copyright (c) 1998-2000 by Scriptics Corporation.
# All rights reserved.
# 
# RCS: @(#) $Id: all.tcl,v 1.3 2008/12/03 02:20:45 hobbs Exp $

set tcltestVersion [package require tcltest]
namespace import -force tcltest::*

#tcltest::testsDirectory [file dir [info script]]
#tcltest::runAllTests

set ::tcltest::testSingleFile false
set ::tcltest::testsDirectory [file dir [info script]]

proc vfsCreateInterp {name} {
    # Use the same setup to access vfs as the master
    if {[catch {
	interp create $name
	$name eval [list set ::auto_path $::auto_path]
	$name eval {package require vfs}
    } err]} {
	puts "$err ; $::errorInfo"
    }
}

# We need to ensure that the testsDirectory is absolute
::tcltest::normalizePath ::tcltest::testsDirectory

package require vfs 1.4

puts stdout "Tests running in interp:  [info nameofexecutable]"
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

tcltest::testConstraint fsIsWritable [expr {1 - [catch {file mkdir isreadonly ; file delete isreadonly}]}]

set timeCmd {clock format [clock seconds]}
puts stdout "Tests began at [eval $timeCmd]"

# source each of the specified tests
foreach file [lsort [::tcltest::getMatchingFiles]] {
    set tail [file tail $file]
    puts stdout $tail
    if {[catch {source $file} msg]} {
	puts stdout $msg
    }
}

# cleanup
puts stdout "\nTests ended at [eval $timeCmd]"
::tcltest::cleanupTests 1
return

