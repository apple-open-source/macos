#
# testutils.tcl --
#
# 	Auxilliary utilities for use with the tcltest package.
# 	Author: Joe English <jenglish@flightlab.com>
# 	Version: 1.1
#
# This file is hereby placed in the public domain.
#

variable tracing 0		;# Set to '1' to enable the 'trace' command
variable tracingErrors 0	;# If set, 'expectError' prints error messages

# ok --
#	Returns an empty string.
#	May be used as the last statement in test scripts 
#	that are only evaluated for side-effects or in cases
#	where you just want to make sure that an operation succeeds
#
proc ok {} { return {} }

# result result --
#	Just returns $result
#
proc result {result} { return $result }

# tracemsg msg --
#	Prints tracing message if $::tracing is nonzero.
#
proc tracemsg {string} {
    if {$::tracing} {
	puts $::tcltest::outputChannel $string
    }
}

# assert expr ?msg? --
#	Evaluates 'expr' and signals an error
#	if the condition is not true.
#
proc assert {expr {message ""}} {
    if {![uplevel 1 [list expr $expr]]} {
	return -code error "Assertion {$expr} failed:\n$message"
    }
}

# expectError script  ? pattern ? --
#	Evaluate 'script', which is expected to fail
#	with an error message matching 'pattern'.
#
#	Returns: 1 if 'script' correctly fails, raises
#	an error otherwise.
#
proc expectError {script {pattern "*"}} {
    set rc [catch [list uplevel 1 $script] result]
    if {$::tracingErrors} {
	puts stderr "==> [string replace $result 70 end ...]"
    }
    set rmsg [string replace $result 40 end ...]
    if {$rc != 1} {
	return -code error \
	    "Expected error, got '$rmsg' (rc=$rc)"
    }
    if {![string match $pattern $result]} {
	return -code error \
	    "Error message '$rmsg' does not match '$pattern'" 
    }
    return $rc
}

# testPackage package ?version?
#	Loads specified package with 'package require $package $version',
#	then prints message describing how the package was loaded.
#
#	This is useful when you've got several versions of a
#	package to lying around and want to make sure you're 
#	testing the right one.
#

proc testPackage {package {version ""}} {
    if {![catch "package present $package $version"]} { return }
    set rc [catch "package require $package $version" result]
    if {$rc} { return -code $rc $result }
    set version $result
    set loadScript [package ifneeded $package $version]
    puts $::tcltest::outputChannel \
	"Loaded $package version $version via {$loadScript}"
    return;
}

#*EOF*
