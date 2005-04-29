#----------------------------------------------------------------------
#
# list.tcl --
#
#	Definitions for extended processing of Tcl lists.
#
# Copyright (c) 2003 by Kevin B. Kenny.  All rights reserved.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# RCS: @(#) $Id: list.tcl,v 1.2 2004/01/15 06:36:14 andreas_kupries Exp $
#
#----------------------------------------------------------------------

package require Tcl 8.0
package require cmdline

namespace eval ::struct { namespace eval list {} }

namespace eval ::struct::list {
    namespace export list

    if {0} {
	# Possibly in the future.
	namespace export LlongestCommonSubsequence
	namespace export LlongestCommonSubsequence2
	namespace export LlcsInvert
	namespace export LlcsInvert2
	namespace export LlcsInvertMerge
	namespace export LlcsInvertMerge2
	namespace export Lreverse
	namespace export Lassign
	namespace export Lflatten
	namespace export Lmap
	namespace export Lfold
	namespace export Liota
	namespace export Lequal
	namespace export Lrepeat
	namespace export LdbJoin
	namespace export LdbJoinOuter
    }
}

##########################
# Public functions

# ::struct::list::list --
#
#	Command that access all list commands.
#
# Arguments:
#	cmd	Name of the subcommand to dispatch to.
#	args	Arguments for the subcommand.
#
# Results:
#	Whatever the result of the subcommand is.

proc ::struct::list::list {cmd args} {
    # Do minimal args checks here
    if { [llength [info level 0]] == 1 } {
	return -code error "wrong # args: should be \"$cmd ?arg arg ...?\""
    }
    set sub L$cmd
    if { [llength [info commands ::struct::list::$sub]] == 0 } {
	set optlist [info commands ::struct::list::L*]
	set xlist {}
	foreach p $optlist {
	    lappend xlist [string range $p 1 end]
	}
	return -code error \
		"bad option \"$cmd\": must be [linsert [join $xlist ", "] "end-1" "or"]"
    }
    return [uplevel 1 [linsert $args 0 ::struct::list::$sub]]
}

##########################
# Private functions follow
#
# Do a compatibility version of [lset] for pre-8.4 versions of Tcl.
# This version does not do multi-arg [lset]!

if { [package vcompare [package provide Tcl] 8.4] < 0 } {
    proc ::struct::list::K { x y } { set x }
    proc ::struct::list::lset { var index arg } {
	upvar 1 $var list
	set list [::lreplace [K $list [set list {}]] $index $index $arg]
    }
}

##########################
# Implementations of the functionality.
#

# ::struct::list::LlongestCommonSubsequence --
#
#       Computes the longest common subsequence of two lists.
#
# Parameters:
#       sequence1, sequence2 -- Two lists to compare.
#	maxOccurs -- If provided, causes the procedure to ignore
#		     lines that appear more than $maxOccurs times
#		     in the second sequence.  See below for a discussion.
# Results:
#       Returns a list of two lists of equal length.
#       The first sublist is of indices into sequence1, and the
#       second sublist is of indices into sequence2.  Each corresponding
#       pair of indices corresponds to equal elements in the sequences;
#       the sequence returned is the longest possible.
#
# Side effects:
#       None.
#
# Notes:
#
#	While this procedure is quite rapid for many tasks of file
# comparison, its performance degrades severely if the second list
# contains many equal elements (as, for instance, when using this
# procedure to compare two files, a quarter of whose lines are blank.
# This drawback is intrinsic to the algorithm used (see the References
# for details).  One approach to dealing with this problem that is
# sometimes effective in practice is arbitrarily to exclude elements
# that appear more than a certain number of times.  This number is
# provided as the 'maxOccurs' parameter.  If frequent lines are
# excluded in this manner, they will not appear in the common subsequence
# that is computed; the result will be the longest common subsequence
# of infrequent elements.
#
#	The procedure struct::list::LongestCommonSubsequence2
# functions as a wrapper around this procedure; it computes the longest
# common subsequence of infrequent elements, and then subdivides the
# subsequences that lie between the matches to approximate the true
# longest common subsequence.
#
# References:
#	J. W. Hunt and M. D. McIlroy, "An algorithm for differential
#	file comparison," Comp. Sci. Tech. Rep. #41, Bell Telephone
#	Laboratories (1976). Available on the Web at the second
#	author's personal site: http://www.cs.dartmouth.edu/~doug/

proc ::struct::list::LlongestCommonSubsequence {
    sequence1
    sequence2
    {maxOccurs 0x7fffffff}
} {
    # Construct a set of equivalence classes of lines in file 2

    set index 0
    foreach string $sequence2 {
	lappend eqv($string) $index
	incr index
    }

    # K holds descriptions of the common subsequences.
    # Initially, there is one common subsequence of length 0,
    # with a fence saying that it includes line -1 of both files.
    # The maximum subsequence length is 0; position 0 of
    # K holds a fence carrying the line following the end
    # of both files.

    lappend K [::list -1 -1 {}]
    lappend K [::list [llength $sequence1] [llength $sequence2] {}]
    set k 0

    # Walk through the first file, letting i be the index of the line and
    # string be the line itself.

    set i 0
    foreach string $sequence1 {
	# Consider each possible corresponding index j in the second file.

	if { [info exists eqv($string)]
	     && [llength $eqv($string)] <= $maxOccurs } {

	    # c is the candidate match most recently found, and r is the
	    # length of the corresponding subsequence.

	    set r 0
	    set c [lindex $K 0]

	    foreach j $eqv($string) {
		# Perform a binary search to find a candidate common
		# subsequence to which may be appended this match.

		set max $k
		set min $r
		set s [expr { $k + 1 }]
		while { $max >= $min } {
		    set mid [expr { ( $max + $min ) / 2 }]
		    set bmid [lindex [lindex $K $mid] 1]
		    if { $j == $bmid } {
			break
		    } elseif { $j < $bmid } {
			set max [expr {$mid - 1}]
		    } else {
			set s $mid
			set min [expr { $mid + 1 }]
		    }
		}

		# Go to the next match point if there is no suitable
		# candidate.

		if { $j == [lindex [lindex $K $mid] 1] || $s > $k} {
		    continue
		}

		# s is the sequence length of the longest sequence
		# to which this match point may be appended. Make
		# a new candidate match and store the old one in K
		# Set r to the length of the new candidate match.

		set newc [::list $i $j [lindex $K $s]]
		if { $r >= 0 } {
		    lset K $r $c
		}
		set c $newc
		set r [expr { $s + 1 }]

		# If we've extended the length of the longest match,
		# we're done; move the fence.

		if { $s >= $k } {
		    lappend K [lindex $K end]
		    incr k
		    break
		}
	    }

	    # Put the last candidate into the array

	    lset K $r $c
	}

	incr i
    }

    # Package the common subsequence in a convenient form

    set seta {}
    set setb {}
    set q [lindex $K $k]

    for { set i 0 } { $i < $k } {incr i } {
	lappend seta {}
	lappend setb {}
    }
    while { [lindex $q 0] >= 0 } {
	incr k -1
	lset seta $k [lindex $q 0]
	lset setb $k [lindex $q 1]
	set q [lindex $q 2]
    }

    return [::list $seta $setb]
}

# ::struct::list::LlongestCommonSubsequence2 --
#
#	Derives an approximation to the longest common subsequence
#	of two lists.
#
# Parameters:
#	sequence1, sequence2 - Lists to be compared
#	maxOccurs - Parameter for imprecise matching - see below.
#
# Results:
#       Returns a list of two lists of equal length.
#       The first sublist is of indices into sequence1, and the
#       second sublist is of indices into sequence2.  Each corresponding
#       pair of indices corresponds to equal elements in the sequences;
#       the sequence returned is an approximation to the longest possible.
#
# Side effects:
#       None.
#
# Notes:
#	This procedure acts as a wrapper around the companion procedure
#	struct::list::LongestCommonSubsequence and accepts the same
#	parameters.  It first computes the longest common subsequence of
#	elements that occur no more than $maxOccurs times in the
#	second list.  Using that subsequence to align the two lists,
#	it then tries to augment the subsequence by computing the true
#	longest common subsequences of the sublists between matched pairs.

proc ::struct::list::LlongestCommonSubsequence2 {
    sequence1
    sequence2
    {maxOccurs 0x7fffffff}
} {
    # Derive the longest common subsequence of elements that occur at
    # most $maxOccurs times

    foreach { l1 l2 } \
	[LlongestCommonSubsequence $sequence1 $sequence2 $maxOccurs] {
	    break
	}

    # Walk through the match points in the sequence just derived.

    set result1 {}
    set result2 {}
    set n1 0
    set n2 0
    foreach i1 $l1 i2 $l2 {
	if { $i1 != $n1 && $i2 != $n2 } {
	    # The match points indicate that there are unmatched
	    # elements lying between them in both input sequences.
	    # Extract the unmatched elements and perform precise
	    # longest-common-subsequence analysis on them.

	    set subl1 [lrange $sequence1 $n1 [expr { $i1 - 1 }]]
	    set subl2 [lrange $sequence2 $n2 [expr { $i2 - 1 }]]
	    foreach { m1 m2 } [LlongestCommonSubsequence $subl1 $subl2] break
	    foreach j1 $m1 j2 $m2 {
		lappend result1 [expr { $j1 + $n1 }]
		lappend result2 [expr { $j2 + $n2 }]
	    }
	}

	# Add the current match point to the result

	lappend result1 $i1
	lappend result2 $i2
	set n1 [expr { $i1 + 1 }]
	set n2 [expr { $i2 + 1 }]
    }

    # If there are unmatched elements after the last match in both files,
    # perform precise longest-common-subsequence matching on them and
    # add the result to our return.

    if { $n1 < [llength $sequence1] && $n2 < [llength $sequence2] } {
	set subl1 [lrange $sequence1 $n1 end]
	set subl2 [lrange $sequence2 $n2 end]
	foreach { m1 m2 } [LlongestCommonSubsequence $subl1 $subl2] break
	foreach j1 $m1 j2 $m2 {
	    lappend result1 [expr { $j1 + $n1 }]
	    lappend result2 [expr { $j2 + $n2 }]
	}
    }

    return [::list $result1 $result2]
}

# ::struct::list::LlcsInvert --
#
#	Takes the data describing a longest common subsequence of two
#	lists and inverts the information in the sense that the result
#	of this command will describe the differences between the two
#	sequences instead of the identical parts.
#
# Parameters:
#	lcsData		longest common subsequence of two lists as
#			returned by longestCommonSubsequence(2).
# Results:
#	Returns a single list whose elements describe the differences
#	between the original two sequences. Each element describes
#	one difference through three pieces, the type of the change,
#	a pair of indices in the first sequence and a pair of indices
#	into the second sequence, in this order.
#
# Side effects:
#       None.

proc ::struct::list::LlcsInvert {lcsData len1 len2} {
    return [LlcsInvert2 [::lindex $lcsData 0] [::lindex $lcsData 1] $len1 $len2]
}

proc ::struct::list::LlcsInvert2 {idx1 idx2 len1 len2} {
    set result {}
    set last1 -1
    set last2 -1

    foreach a $idx1 b $idx2 {
	# Four possible cases.
	# a) last1 ... a and last2 ... b are not empty.
	#    This is a 'change'.
	# b) last1 ... a is empty, last2 ... b is not.
	#    This is an 'addition'.
	# c) last1 ... a is not empty, last2 ... b is empty.
	#    This is a deletion.
	# d) If both ranges are empty we can ignore the
	#    two current indices.

	set empty1 [expr {($a - $last1) <= 1}]
	set empty2 [expr {($b - $last2) <= 1}]

	if {$empty1 && $empty2} {
	    # Case (d), ignore the indices
	} elseif {$empty1} {
	    # Case (b), 'addition'.
	    incr last2 ; incr b -1
	    lappend result [::list added [::list $last1 $a] [::list $last2 $b]]
	    incr b
	} elseif {$empty2} {
	    # Case (c), 'deletion'
	    incr last1 ; incr a -1
	    lappend result [::list deleted [::list $last1 $a] [::list $last2 $b]]
	    incr a
	} else {
	    # Case (q), 'change'.
	    incr last1 ; incr a -1
	    incr last2 ; incr b -1
	    lappend result [::list changed [::list $last1 $a] [::list $last2 $b]]
	    incr a
	    incr b
	}

	set last1 $a
	set last2 $b
    }

    # Handle the last chunk, using the information about the length of
    # the original sequences.

    set empty1 [expr {($len1 - $last1) <= 1}]
    set empty2 [expr {($len2 - $last2) <= 1}]

    if {$empty1 && $empty2} {
	# Case (d), ignore the indices
    } elseif {$empty1} {
	# Case (b), 'addition'.
	incr last2 ; incr len2 -1
	lappend result [::list added [::list $last1 $len1] [::list $last2 $len2]]
    } elseif {$empty2} {
	# Case (c), 'deletion'
	incr last1 ; incr len1 -1
	lappend result [::list deleted [::list $last1 $len1] [::list $last2 $len2]]
    } else {
	# Case (q), 'change'.
	incr last1 ; incr len1 -1
	incr last2 ; incr len2 -1
	lappend result [::list changed [::list $last1 $len1] [::list $last2 $len2]]
    }

    return $result
}

proc ::struct::list::LlcsInvertMerge {lcsData len1 len2} {
    return [LlcsInvertMerge2 [::lindex $lcsData 0] [::lindex $lcsData 1] $len1 $len2]
}

proc ::struct::list::LlcsInvertMerge2 {idx1 idx2 len1 len2} {
    set result {}
    set last1 -1
    set last2 -1

    foreach a $idx1 b $idx2 {
	# Four possible cases.
	# a) last1 ... a and last2 ... b are not empty.
	#    This is a 'change'.
	# b) last1 ... a is empty, last2 ... b is not.
	#    This is an 'addition'.
	# c) last1 ... a is not empty, last2 ... b is empty.
	#    This is a deletion.
	# d) If both ranges are empty we can ignore the
	#    two current indices. For merging we simply
	#    take the information from the input.

	set empty1 [expr {($a - $last1) <= 1}]
	set empty2 [expr {($b - $last2) <= 1}]

	if {$empty1 && $empty2} {
	    # Case (d), add 'unchanged' chunk.
	    # [765321] 
	    # Note: We canot extend an 'unchanged' chunk at the
	    #       end if the result has no content yet.

	    if {[llength $result] > 0} {
		foreach {type left right} [lindex $result end] break
	    } else {
		set type -- ; # dummy value
	    }
	    if {[string equal $type unchanged]} {
		# We extend the 'unchanged' chunk found at the end.
		lset result end [::list unchanged [::list [lindex $left 0] $a] [::list [lindex $right 0] $b]]
	    } else {
		lappend result [::list unchanged [::list $last1 $a] [::list $last2 $b]]
	    }

	} elseif {$empty1} {
	    # Case (b), 'addition'.
	    incr last2 ; incr b -1
	    lappend result [::list added [::list $last1 $a] [::list $last2 $b]]
	    incr b
	} elseif {$empty2} {
	    # Case (c), 'deletion'
	    incr last1 ; incr a -1
	    lappend result [::list deleted [::list $last1 $a] [::list $last2 $b]]
	    incr a
	} else {
	    # Case (q), 'change'.
	    incr last1 ; incr a -1
	    incr last2 ; incr b -1
	    lappend result [::list changed [::list $last1 $a] [::list $last2 $b]]
	    incr a
	    incr b
	}

	set last1 $a
	set last2 $b
    }

    # Handle the last chunk, using the information about the length of
    # the original sequences.

    set empty1 [expr {($len1 - $last1) <= 1}]
    set empty2 [expr {($len2 - $last2) <= 1}]

    if {$empty1 && $empty2} {
	# Case (d), ignore the indices
    } elseif {$empty1} {
	# Case (b), 'addition'.
	incr last2 ; incr len2 -1
	lappend result [::list added [::list $last1 $len1] [::list $last2 $len2]]
    } elseif {$empty2} {
	# Case (c), 'deletion'
	incr last1 ; incr len1 -1
	lappend result [::list deleted [::list $last1 $len1] [::list $last2 $len2]]
    } else {
	# Case (q), 'change'.
	incr last1 ; incr len1 -1
	incr last2 ; incr len2 -1
	lappend result [::list changed [::list $last1 $len1] [::list $last2 $len2]]
    }

    return $result
}

# ::struct::list::Lreverse --
#
#	Reverses the contents of the list and returns the reversed
#	list as the result of the command.
#
# Parameters:
#	sequence	List to be reversed.
#
# Results:
#	The sequence in reverse.
#
# Side effects:
#       None.

proc ::struct::list::Lreverse {sequence} {
    set l [::llength $sequence]

    # Shortcut for lists where reversing yields the list itself
    if {$l < 2} {return $sequence}

    # Perform true reversal
    set res [::list]
    while {$l} {
	::lappend res [::lindex $sequence [incr l -1]]
    }
    return $res
}


# ::struct::list::Lassign --
#
#	Assign list elements to variables.
#
# Parameters:
#	sequence	List to assign
#	args		Names of the variables to assign to.
#
# Results:
#	The unassigned part of the sequence. Can be empty.
#
# Side effects:
#       None.

proc ::struct::list::Lassign {sequence args} {
    set l [::llength $sequence]
    set a [::llength $args]

    # Nothing to assign.
    if {$a == 0} {return $sequence}

    # Perform assignments
    set i 0
    foreach v $args {
	upvar 1 $v var
	set      var [::lindex $sequence $i]
	incr i
    }

    # Return remainder, if there is any.
    return [::lrange $sequence $a end]
}


# ::struct::list::Lflatten --
#
#	Remove nesting from the input
#
# Parameters:
#	sequence	List to flatten
#
# Results:
#	The input list with one or all levels of nesting removed.
#
# Side effects:
#       None.

proc ::struct::list::Lflatten {args} {
    if {[::llength $args] < 1} {
	return -code error \
		"wrong#args: should be \"::struct::list::Lflatten ?-full? ?--? sequence\""
    }

    set full 0
    while {[string match -* [set opt [::lindex $args 0]]]} {
	switch -glob -- $opt {
	    -full   {set full 1}
	    --      {break}
	    default {return -code error ""}
	}
	set args [::lrange $args 1 end]
    }

    if {[::llength $args] != 1} {
	return -code error \
		"wrong#args: should be \"::struct::list::Lflatten ?-full? ?--? sequence\""
    }

    set sequence [::lindex $args 0]
    set cont 1
    while {$cont} {
	set cont 0
	set result [::list]
	foreach item $sequence {
	    eval [::list ::lappend result] $item
	}
	if {$full && [string compare $sequence $result]} {set cont 1}
	set sequence $result
    }
    return $result
}


# ::struct::list::Lmap --
#
#	Apply command to each element of a list and return concatenated results.
#
# Parameters:
#	sequence	List to operate on
#	cmdprefix	Operation to perform on the elements.
#
# Results:
#	List containing the result of applying cmdprefix to the elements of the
#	sequence.
#
# Side effects:
#       None of its own, but the command prefix can perform arbitry actions.

proc ::struct::list::Lmap {sequence cmdprefix} {
    # Shortcut when nothing is to be done.
    if {[::llength $sequence] == 0} {return $sequence}

    set res [::list]
    foreach item $sequence {
	lappend res [uplevel 1 [linsert $cmdprefix end $item]]
    }
    return $res
}

# ::struct::list::Lfold --
#
#	Fold list into one value.
#
# Parameters:
#	sequence	List to operate on
#	cmdprefix	Operation to perform on the elements.
#
# Results:
#	Result of applying cmdprefix to the elements of the
#	sequence.
#
# Side effects:
#       None of its own, but the command prefix can perform arbitry actions.

proc ::struct::list::Lfold {sequence initialvalue cmdprefix} {
    # Shortcut when nothing is to be done.
    if {[::llength $sequence] == 0} {return $initialvalue}

    set res $initialvalue
    foreach item $sequence {
	set res [uplevel 1 [linsert $cmdprefix end $res $item]]
    }
    return $res
}

# ::struct::list::Liota --
#
#	Return a list containing the integer numbers 0 ... n-1
#
# Parameters:
#	n	First number not in the generated list.
#
# Results:
#	A list containing integer numbers.
#
# Side effects:
#       None

proc ::struct::list::Liota {n} {
    set retval [::list]
    for {set i 0} {$i < $n} {incr i} {
	::lappend retval $i
    }
    return $retval
}

# ::struct::list::Lequal --
#
#	Compares two lists for equality
#	(Same length, Same elements in same order).
#
# Parameters:
#	a	First list to compare.
#	b	Second list to compare.
#
# Results:
#	A boolean. True if the lists are equal.
#
# Side effects:
#       None

proc ::struct::list::Lequal {a b} {
    # Author of this command is "Richard Suchenwirth"

    if {[::llength $a] != [::llength $b]} {return 0}
    if {[::lindex $a 0] == $a} {return [string equal $a $b]}
    foreach i $a j $b {if {![Lequal $i $j]} {return 0}}
    return 1
}

# ::struct::list::Lrepeat --
#
#	Create a list repeating the same value over again.
#
# Parameters:
#	value	value to use in the created list.
#	args	Dimension(s) of the (nested) list to create.
#
# Results:
#	A list
#
# Side effects:
#       None

proc ::struct::list::Lrepeat {value args} {
    if {[::llength $args] == 1} {set args [::lindex $args 0]}
    set buf {}
    foreach number $args {
	incr number 0 ;# force integer (1)
	set buf {}
	for {set i 0} {$i<$number} {incr i} {
	    ::lappend buf $value
	}
	set value $buf
    }
    return $buf
    # (1): See 'Stress testing' (wiki) for why this makes the code safer.
}

# ::struct::list::LdbJoin(Keyed) --
#
#	Relational table joins.
#
# Parameters:
#	args	key specs and tables to join
#
# Results:
#	A table/matrix as nested list. See
#	struct/matrix set/get rect for structure.
#
# Side effects:
#       None

proc ::struct::list::LdbJoin {args} {
    # --------------------------------
    # Process options ...

    set mode   inner
    set keyvar {}

    while {[llength $args]} {
        set err [::cmdline::getopt args {inner left right full keys.arg} opt arg]
	if {$err == 1} {
	    if {[string equal $opt keys]} {
		set keyvar $arg
	    } else {
		set mode $opt
	    }
	} elseif {$err < 0} {
	    return -code error "wrong#args: dbJoin ?-inner|-left|-right|-full? ?-keys varname? \{key table\}..."
	} else {
	    # Non-option argument found, stop processing.
	    break
	}
    }

    set inner       [string equal $mode inner]
    set innerorleft [expr {$inner || [string equal $mode left]}]

    # --------------------------------
    # Process tables ...

    if {([llength $args] % 2) != 0} {
	return -code error "wrong#args: dbJoin ?-inner|-left|-right|-full? \{key table\}..."
    }

    # One table only, join is identity
    if {[llength $args] == 2} {return [lindex $args 1]}

    # Use first table for setup.

    foreach {key table} $args break

    # Check for possible early abort
    if {$innerorleft && ([llength $table] == 0)} {return {}}

    set width 0
    array set state {}

    set keylist [InitMap state width $key $table]

    # Extend state with the remaining tables.

    foreach {key table} [lrange $args 2 end] {
	# Check for possible early abort
	if {$inner && ([llength $table] == 0)} {return {}}

	switch -exact -- $mode {
	    inner {set keylist [MapExtendInner      state       $key $table]}
	    left  {set keylist [MapExtendLeftOuter  state width $key $table]}
	    right {set keylist [MapExtendRightOuter state width $key $table]}
	    full  {set keylist [MapExtendFullOuter  state width $key $table]}
	}

	# Check for possible early abort
	if {$inner && ([llength $keylist] == 0)} {return {}}
    }

    if {[string length $keyvar]} {
	upvar 1 $keyvar keys
	set             keys $keylist
    }

    return [MapToTable state $keylist]
}

proc ::struct::list::LdbJoinKeyed {args} {
    # --------------------------------
    # Process options ...

    set mode   inner
    set keyvar {}

    while {[llength $args]} {
        set err [::cmdline::getopt args {inner left right full keys.arg} opt arg]
	if {$err == 1} {
	    if {[string equal $opt keys]} {
		set keyvar $arg
	    } else {
		set mode $opt
	    }
	} elseif {$err < 0} {
	    return -code error "wrong#args: dbJoin ?-inner|-left|-right|-full? table..."
	} else {
	    # Non-option argument found, stop processing.
	    break
	}
    }

    set inner       [string equal $mode inner]
    set innerorleft [expr {$inner || [string equal $mode left]}]

    # --------------------------------
    # Process tables ...

    # One table only, join is identity
    if {[llength $args] == 1} {
	return [Dekey [lindex $args 0]]
    }

    # Use first table for setup.

    set table [lindex $args 0]

    # Check for possible early abort
    if {$innerorleft && ([llength $table] == 0)} {return {}}

    set width 0
    array set state {}

    set keylist [InitKeyedMap state width $table]

    # Extend state with the remaining tables.

    foreach table [lrange $args 1 end] {
	# Check for possible early abort
	if {$inner && ([llength $table] == 0)} {return {}}

	switch -exact -- $mode {
	    inner {set keylist [MapKeyedExtendInner      state       $table]}
	    left  {set keylist [MapKeyedExtendLeftOuter  state width $table]}
	    right {set keylist [MapKeyedExtendRightOuter state width $table]}
	    full  {set keylist [MapKeyedExtendFullOuter  state width $table]}
	}

	# Check for possible early abort
	if {$inner && ([llength $keylist] == 0)} {return {}}
    }

    if {[string length $keyvar]} {
	upvar 1 $keyvar keys
	set             keys $keylist
    }

    return [MapToTable state $keylist]
}

## Helpers for the relational joins.
## Map is an array mapping from keys to a list
## of rows with that key

proc ::struct::list::Cartesian {leftmap rightmap key} {
    upvar $leftmap left $rightmap right
    set joined [::list]
    foreach lrow $left($key) {
	foreach row $right($key) {
	    lappend joined [concat $lrow $row]
	}
    }
    set left($key) $joined
    return
}

proc ::struct::list::SingleRightCartesian {mapvar key rightrow} {
    upvar $mapvar map
    set joined [::list]
    foreach lrow $map($key) {
	lappend joined [concat $lrow $rightrow]
    }
    set map($key) $joined
    return
}

proc ::struct::list::MapToTable {mapvar keys} {
    # Note: keys must not appear multiple times in the list.

    upvar $mapvar map
    set table [::list]
    foreach k $keys {
	foreach row $map($k) {lappend table $row}
    }
    return $table
}

## More helpers, core join operations: Init, Extend.

proc ::struct::list::InitMap {mapvar wvar key table} {
    upvar $mapvar map $wvar width
    set width [llength [lindex $table 0]]
    foreach row $table {
	set keyval [lindex $row $key]
	if {[info exists map($keyval)]} {
	    lappend map($keyval) $row
	} else {
	    set map($keyval) [::list $row]
	}
    }
    return [array names map]
}

proc ::struct::list::MapExtendInner {mapvar key table} {
    upvar $mapvar map
    array set used {}

    # Phase I - Find all keys in the second table matching keys in the
    # first. Remember all their rows.
    foreach row $table {
	set keyval [lindex $row $key]
	if {[info exists map($keyval)]} {
	    if {[info exists used($keyval)]} {
		lappend used($keyval) $row
	    } else {
		set used($keyval) [::list $row]
	    }
	} ; # else: Nothing to do for missing keys.
    }

    # Phase II - Merge the collected rows of the second (right) table
    # into the map, and eliminate all entries which have no keys in
    # the second table.
    foreach k [array names map] {
	if {[info exists  used($k)]} {
	    Cartesian map used $k
	} else {
	    unset map($k)
	}
    }
    return [array names map]
}

proc ::struct::list::MapExtendRightOuter {mapvar wvar key table} {
    upvar $mapvar map $wvar width
    array set used {}

    # Phase I - We keep all keys of the right table, even if they are
    # missing in the left one <=> Definition of right outer join.

    set w [llength [lindex $table 0]]
    foreach row $table {
	set keyval [lindex $row $key]
	if {[info exists used($keyval)]} {
	    lappend used($keyval) $row
	} else {
	    set used($keyval) [::list $row]
	}
    }

    # Phase II - Merge the collected rows of the second (right) table
    # into the map, and eliminate all entries which have no keys in
    # the second table. If there is nothing in the left table we
    # create an appropriate empty row for the cartesian => definition
    # of right outer join.

    # We go through used, because map can be empty for outer

    foreach k [array names map] {
	if {![info exists used($k)]} {
	    unset map($k)
	}
    }
    foreach k [array names used] {
	if {![info exists map($k)]} {
	    set map($k) [::list [Lrepeat {} $width]]
	}
	Cartesian map used $k
    }

    incr width $w
    return [array names map]
}

proc ::struct::list::MapExtendLeftOuter {mapvar wvar key table} {
    upvar $mapvar map $wvar width
    array set used {}

    ## Keys: All in inner join + additional left keys 
    ##       == All left keys = array names map after
    ##          all is said and done with it.

    # Phase I - Find all keys in the second table matching keys in the
    # first. Remember all their rows.
    set w [llength [lindex $table 0]]
    foreach row $table {
	set keyval [lindex $row $key]
	if {[info exists map($keyval)]} {
	    if {[info exists used($keyval)]} {
		lappend used($keyval) $row
	    } else {
		set used($keyval) [::list $row]
	    }
	} ; # else: Nothing to do for missing keys.
    }

    # Phase II - Merge the collected rows of the second (right) table
    # into the map. We keep entries which have no keys in the second
    # table, we actually extend them <=> Left outer join.

    foreach k [array names map] {
	if {[info exists  used($k)]} {
	    Cartesian map used $k
	} else {
	    SingleRightCartesian map $k [Lrepeat {} $w]
	}
    }
    incr width $w
    return [array names map]
}

proc ::struct::list::MapExtendFullOuter {mapvar wvar key table} {
    upvar $mapvar map $wvar width
    array set used {}

    # Phase I - We keep all keys of the right table, even if they are
    # missing in the left one <=> Definition of right outer join.

    set w [llength [lindex $table 0]]
    foreach row $table {
	set keyval [lindex $row $key]
	if {[info exists used($keyval)]} {
	    lappend used($keyval) $row
	} else {
	    lappend keylist $keyval
	    set used($keyval) [::list $row]
	}
    }

    # Phase II - Merge the collected rows of the second (right) table
    # into the map. We keep entries which have no keys in the second
    # table, we actually extend them <=> Left outer join.
    # If there is nothing in the left table we create an appropriate
    # empty row for the cartesian => definition of right outer join.

    # We go through used, because map can be empty for outer

    foreach k [array names map] {
	if {![info exists used($k)]} {
	    SingleRightCartesian map $k [Lrepeat {} $w]
	}
    }
    foreach k [array names used] {
	if {![info exists map($k)]} {
	    set map($k) [::list [Lrepeat {} $width]]
	}
	Cartesian map used $k
    }

    incr width $w
    return [array names map]
}

## Keyed helpers

proc ::struct::list::InitKeyedMap {mapvar wvar table} {
    upvar $mapvar map $wvar width
    set width [llength [lindex [lindex $table 0] 1]]
    foreach row $table {
	foreach {keyval rowdata} $row break
	if {[info exists map($keyval)]} {
	    lappend map($keyval) $rowdata
	} else {
	    set map($keyval) [::list $rowdata]
	}
    }
    return [array names map]
}

proc ::struct::list::MapKeyedExtendInner {mapvar table} {
    upvar $mapvar map
    array set used {}

    # Phase I - Find all keys in the second table matching keys in the
    # first. Remember all their rows.
    foreach row $table {
	foreach {keyval rowdata} $row break
	if {[info exists map($keyval)]} {
	    if {[info exists used($keyval)]} {
		lappend used($keyval) $rowdata
	    } else {
		set used($keyval) [::list $rowdata]
	    }
	} ; # else: Nothing to do for missing keys.
    }

    # Phase II - Merge the collected rows of the second (right) table
    # into the map, and eliminate all entries which have no keys in
    # the second table.
    foreach k [array names map] {
	if {[info exists  used($k)]} {
	    Cartesian map used $k
	} else {
	    unset map($k)
	}
    }

    return [array names map]
}

proc ::struct::list::MapKeyedExtendRightOuter {mapvar wvar table} {
    upvar $mapvar map $wvar width
    array set used {}

    # Phase I - We keep all keys of the right table, even if they are
    # missing in the left one <=> Definition of right outer join.

    set w [llength [lindex $table 0]]
    foreach row $table {
	foreach {keyval rowdata} $row break
	if {[info exists used($keyval)]} {
	    lappend used($keyval) $rowdata
	} else {
	    set used($keyval) [::list $rowdata]
	}
    }

    # Phase II - Merge the collected rows of the second (right) table
    # into the map, and eliminate all entries which have no keys in
    # the second table. If there is nothing in the left table we
    # create an appropriate empty row for the cartesian => definition
    # of right outer join.

    # We go through used, because map can be empty for outer

    foreach k [array names map] {
	if {![info exists used($k)]} {
	    unset map($k)
	}
    }
    foreach k [array names used] {
	if {![info exists map($k)]} {
	    set map($k) [::list [Lrepeat {} $width]]
	}
	Cartesian map used $k
    }

    incr width $w
    return [array names map]
}

proc ::struct::list::MapKeyedExtendLeftOuter {mapvar wvar table} {
    upvar $mapvar map $wvar width
    array set used {}

    ## Keys: All in inner join + additional left keys 
    ##       == All left keys = array names map after
    ##          all is said and done with it.

    # Phase I - Find all keys in the second table matching keys in the
    # first. Remember all their rows.
    set w [llength [lindex $table 0]]
    foreach row $table {
	foreach {keyval rowdata} $row break
	if {[info exists map($keyval)]} {
	    if {[info exists used($keyval)]} {
		lappend used($keyval) $rowdata
	    } else {
		set used($keyval) [::list $rowdata]
	    }
	} ; # else: Nothing to do for missing keys.
    }

    # Phase II - Merge the collected rows of the second (right) table
    # into the map. We keep entries which have no keys in the second
    # table, we actually extend them <=> Left outer join.

    foreach k [array names map] {
	if {[info exists  used($k)]} {
	    Cartesian map used $k
	} else {
	    SingleRightCartesian map $k [Lrepeat {} $w]
	}
    }
    incr width $w
    return [array names map]
}

proc ::struct::list::MapKeyedExtendFullOuter {mapvar wvar table} {
    upvar $mapvar map $wvar width
    array set used {}

    # Phase I - We keep all keys of the right table, even if they are
    # missing in the left one <=> Definition of right outer join.

    set w [llength [lindex $table 0]]
    foreach row $table {
	foreach {keyval rowdata} $row break
	if {[info exists used($keyval)]} {
	    lappend used($keyval) $rowdata
	} else {
	    lappend keylist $keyval
	    set used($keyval) [::list $rowdata]
	}
    }

    # Phase II - Merge the collected rows of the second (right) table
    # into the map. We keep entries which have no keys in the second
    # table, we actually extend them <=> Left outer join.
    # If there is nothing in the left table we create an appropriate
    # empty row for the cartesian => definition of right outer join.

    # We go through used, because map can be empty for outer

    foreach k [array names map] {
	if {![info exists used($k)]} {
	    SingleRightCartesian map $k [Lrepeat {} $w]
	}
    }
    foreach k [array names used] {
	if {![info exists map($k)]} {
	    set map($k) [::list [Lrepeat {} $width]]
	}
	Cartesian map used $k
    }

    incr width $w
    return [array names map]
}

proc ::struct::list::Dekey {keyedtable} {
    set table [::list]
    foreach row $keyedtable {lappend table [lindex $row 1]}
    return $table
}
