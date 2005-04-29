#----------------------------------------------------------------------
#
# sets.tcl --
#
#	Definitions for the processing of sets.
#
# Copyright (c) 2004 by Andreas Kupries.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# RCS: @(#) $Id: sets.tcl,v 1.2.2.1 2004/05/24 02:58:12 andreas_kupries Exp $
#
#----------------------------------------------------------------------

package require Tcl 8.0

namespace eval ::struct { namespace eval set {} }

namespace eval ::struct::set {
    namespace export set
}

##########################
# Public functions

# ::struct::set::set --
#
#	Command that access all set commands.
#
# Arguments:
#	cmd	Name of the subcommand to dispatch to.
#	args	Arguments for the subcommand.
#
# Results:
#	Whatever the result of the subcommand is.

proc ::struct::set::set {cmd args} {
    # Do minimal args checks here
    if { [llength [info level 0]] == 1 } {
	return -code error "wrong # args: should be \"$cmd ?arg arg ...?\""
    }
    ::set sub S$cmd
    if { [llength [info commands ::struct::set::$sub]] == 0 } {
	::set optlist [info commands ::struct::set::S*]
	::set xlist {}
	foreach p $optlist {
	    lappend xlist [string range $p 16 end]
	}
	return -code error \
		"bad option \"$cmd\": must be [linsert [join $xlist ", "] "end-1" "or"]"
    }
    return [uplevel 1 [linsert $args 0 ::struct::set::$sub]]
}

##########################
# Implementations of the functionality.
#

# ::struct::set::Sempty --
#
#       Determines emptiness of the set
#
# Parameters:
#       set	-- The set to check for emptiness.
#
# Results:
#       A boolean value. True indicates that the set is empty.
#
# Side effects:
#       None.
#
# Notes:

proc ::struct::set::Sempty {set} {
    return [expr {[llength $set] == 0}]
}

# ::struct::set::Ssize --
#
#	Computes the cardinality of the set.
#
# Parameters:
#	set	-- The set to inspect.
#
# Results:
#       An integer greater than or equal to zero.
#
# Side effects:
#       None.

proc ::struct::set::Ssize {set} {
    return [llength [Cleanup $set]]
}

# ::struct::set::Scontains --
#
#	Determines if the item is in the set.
#
# Parameters:
#	set	-- The set to inspect.
#	item	-- The element to look for.
#
# Results:
#	A boolean value. True indicates that the element is present.
#
# Side effects:
#       None.

proc ::struct::set::Scontains {set item} {
    return [expr {[lsearch -exact $set $item] >= 0}]
}

# ::struct::set::Sunion --
#
#	Computes the union of the arguments.
#
# Parameters:
#	args	-- List of sets to unify.
#
# Results:
#	The union of the arguments.
#
# Side effects:
#       None.

proc ::struct::set::Sunion {args} {
    switch -exact -- [llength $args] {
	0 {return {}}
	1 {return [lindex $args 0]}
    }
    foreach setX $args {
	foreach x $setX {::set ($x) {}}
    }
    return [array names {}]
}


# ::struct::set::Sintersect --
#
#	Computes the intersection of the arguments.
#
# Parameters:
#	args	-- List of sets to intersect.
#
# Results:
#	The intersection of the arguments
#
# Side effects:
#       None.

proc ::struct::set::Sintersect {args} {
    switch -exact -- [llength $args] {
	0 {return {}}
	1 {return [lindex $args 0]}
    }
    ::set res [lindex $args 0]
    foreach set [lrange $args 1 end] {
	if {[llength $res] && [llength $set]} {
	    ::set res [Intersect $res $set]
	} else {
	    # Squash 'res'. Otherwise we get the wrong result if res
	    # is not empty, but 'set' is.
	    ::set res {}
	    break
	}
    }
    return $res
}

proc ::struct::set::Intersect {A B} {
    if {[llength $A] == 0} {return {}}
    if {[llength $B] == 0} {return {}}

    # This is slower than local vars, but more robust
    if {[llength $B] > [llength $A]} {
	::set res $A
	::set A $B
	::set B $res
    }
    ::set res {}
    foreach x $A {::set ($x) {}}
    foreach x $B {
	if {[info exists ($x)]} {
	    lappend res $x
	}
    }
    return $res
}

# ::struct::set::Sdifference --
#
#	Compute difference of two sets.
#
# Parameters:
#	A, B	-- Sets to compute the difference for.
#
# Results:
#	A - B
#
# Side effects:
#       None.

if {[package vcompare [package provide Tcl] 8.4] < 0} {
    # Tcl 8.[23]. Use explicit array to perform the operation.

    proc ::struct::set::Sdifference {A B} {
	if {[llength $A] == 0} {return {}}
	if {[llength $B] == 0} {return $A}

	array set tmp {}
	foreach x $A {::set tmp($x) .}
	foreach x $B {catch {unset tmp($x)}}
	return [array names tmp]
    }

} else {
    # Tcl 8.4+, has 'unset -nocomplain'

    proc ::struct::set::Sdifference {A B} {
	if {[llength $A] == 0} {return {}}
	if {[llength $B] == 0} {return $A}

	# Get the variable B out of the way, avoid collisions
	# prepare for "pure list optimization"
	::set ::struct::set::tmp [lreplace $B -1 -1 unset -nocomplain]
	unset B

	# unset A early: no local variables left
	foreach [lindex [list $A [unset A]] 0] {.} {break}

	eval $::struct::set::tmp
	return [info locals]
    }
}

# ::struct::set::Ssymdiff --
#
#	Compute symmetric difference of two sets.
#
# Parameters:
#	A, B	-- The sets to compute the s.difference for.
#
# Results:
#	The symmetric difference of the two input sets.
#
# Side effects:
#       None.

proc ::struct::set::Ssymdiff {A B} {
    # symdiff == (A-B) + (B-A) == (A+B)-(A*B)
    if {[llength $A] == 0} {return $B}
    if {[llength $B] == 0} {return $A}
    return [Sunion [Sdifference $A $B] [Sdifference $B $A]]
}

# ::struct::set::Sintersect3 --
#
#	Return intersection and differences for two sets.
#
# Parameters:
#	A, B	-- The sets to inspect.
#
# Results:
#	List containing A*B, A-B, and B-A
#
# Side effects:
#       None.

proc ::struct::set::Sintersect3 {A B} {
    return [list [Sintersect $A $B] [Sdifference $A $B] [Sdifference $B $A]]
}

# ::struct::set::Sequal --
#
#	Compares two sets for equality.
#
# Parameters:
#	a	First set to compare.
#	b	Second set to compare.
#
# Results:
#	A boolean. True if the lists are equal.
#
# Side effects:
#       None.

proc ::struct::set::Sequal {A B} {
    ::set A [Cleanup $A]
    ::set B [Cleanup $B]

    # Equal if of same cardinality and difference is empty.

    if {[::llength $A] != [::llength $B]} {return 0}
    return [expr {[llength [Sdifference $A $B]] == 0}]
}


proc ::struct::set::Cleanup {A} {
    # unset A to avoid collisions
    if {[llength $A] < 2} {return $A}
    foreach [lindex [list $A [unset A]] 0] {.} {break}
    return [info locals]
}
