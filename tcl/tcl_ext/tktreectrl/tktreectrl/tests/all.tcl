# all.tcl --
#
# This file contains a top-level script to run all of the Tcl
# tests.  Execute it by invoking "source all.test" when running tcltest
# in this directory.
#
# Copyright (c) 1998-2000 by Scriptics Corporation.
# All rights reserved.
# 
# RCS: @(#) $Id: all.tcl,v 1.2 2002/12/30 21:43:48 krischan Exp $

if {[lsearch [namespace children] ::tcltest] == -1} {
    package require tcltest
    namespace import ::tcltest::*
}

set ::tcltest::testSingleFile false
set ::tcltest::testsDirectory [file dir [info script]]

# We need to ensure that the testsDirectory is absolute
::tcltest::normalizePath ::tcltest::testsDirectory

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

# Currently the following doesn't work if tktreectrl is not yet installed:
#   package require treectrl
# And we want to test the currently built version anyway.
# So we have to load and source it by hand, until TreectrlInit()
# evals the initScript with calls of tcl_findLibrary...

proc package_require {treectrl} { 
    set thisPlatform $::tcl_platform(platform)
    if {![catch {tk windowingsystem} windowingSystem] \
	    && [string equal aqua $windowingSystem]} {
	set thisPlatform macosx
    }
    switch -- $thisPlatform {
	macintosh {
	    load treectrl.shlb
	}
	macosx {
	    load build/treectrl.dylib
	}
	unix {
	    load [glob libtreectrl*[info sharedlibextension]]
	}
	default { # Windows
	    load Build/treectrl[info sharedlibextension]
	}
    }
    if {![namespace exists ::TreeCtrl]} {
	uplevel #0 source [file join library treectrl.tcl]
	uplevel #0 source [file join library filelist-bindings.tcl]
    }
}

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

