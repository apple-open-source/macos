# all.tcl --
#
# This file contains a top-level script to run all of the Tcl
# tests.  Execute it by invoking "source all.test" when running tcltest
# in this directory.
#
# Copyright (c) 1998-2000 by Scriptics Corporation.
# All rights reserved.
# 
# RCS: @(#) $Id: all.tcl,v 1.2 2004/01/20 15:25:15 vincentdarley Exp $

set tcltestVersion [package require tcltest]
namespace import -force tcltest::*

#tcltest::testsDirectory [file dir [info script]]
#tcltest::runAllTests

set ::tcltest::testSingleFile false
set ::tcltest::testsDirectory [file dir [info script]]

proc vfsCreateInterp {name} {
    # Have to make sure we load the same dll else we'll have multiple
    # copies!
    if {[catch {
	interp create $name 
	$name eval [list package ifneeded vfs 1.3 [package ifneeded vfs 1.3]]
	$name eval [list set ::auto_path $::auto_path]
	$name eval {package require vfs}
    } err]} {
	puts "$err ; $::errorInfo"
    }
}

# Set up auto_path and package indices for loading.  Must make sure we 
# can load the same dll into the main interpreter and sub interps.
proc setupForVfs {lib} {
    namespace eval vfs {}
    global auto_path dir vfs::dll
    set dir [file norm $lib]
    set auto_path [linsert $auto_path 0 $dir]
    uplevel \#0 [list source [file join $dir pkgIndex.tcl]]
    set orig [package ifneeded vfs 1.3]
    set vfs::dll [lindex $orig 2]
    if {![file exists $vfs::dll]} {
	set vfs::dll [file join [pwd] [file tail $vfs::dll]]
	package ifneeded vfs 1.3 [list [lindex $orig 0] [lindex $orig 1] $vfs::dll]
    }
}

# We need to ensure that the testsDirectory is absolute
::tcltest::normalizePath ::tcltest::testsDirectory

if {[lindex [file system $::tcltest::testsDirectory] 0] == "native"} {
    setupForVfs [file join [file dir $::tcltest::testsDirectory] library]
}

package require vfs

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

