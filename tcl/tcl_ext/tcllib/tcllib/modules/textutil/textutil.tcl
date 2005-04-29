package require Tcl 8.2

namespace eval ::textutil {
    namespace export strRepeat
    
    variable HaveStrRepeat [ expr {![ catch { string repeat a 1 } ]} ]

    if {0} {
	# Problems with the deactivated code:
	# - Linear in 'num'.
	# - Tests for 'string repeat' in every call!
	#   (Ok, just the variable, still a test every call)
	# - Fails for 'num == 0' because of undefined 'str'.

	proc StrRepeat { char num } {
	    variable HaveStrRepeat
	    if { $HaveStrRepeat == 0 } then {
		for { set i 0 } { $i < $num } { incr i } {
		    append str $char
		}
	    } else {
		set str [ string repeat $char $num ]
	    }
	    return $str
	}
    }

}

if {$::textutil::HaveStrRepeat} {
    proc ::textutil::strRepeat {char num} {
	return [string repeat $char $num]
    }

    proc ::textutil::blank {n} {
	return [string repeat " " $n]
    }

} else {
    proc ::textutil::strRepeat {char num} {
	if {$num <= 0} {
	    # No replication required
	    return ""
	} elseif {$num == 1} {
	    # Quick exit for recursion
	    return $char
	} elseif {$num == 2} {
	    # Another quick exit for recursion
	    return $char$char
	} elseif {0 == ($num % 2)} {
	    # Halving the problem results in O (log n) complexity.
	    set result [strRepeat $char [expr {$num / 2}]]
	    return "$result$result"
	} else {
	    # Uneven length, reduce problem by one
	    return "$char[strRepeat $char [incr num -1]]"
	}
    }

    proc ::textutil::blank {n} {
	return [strRepeat " " $n]
    }
}


# @c Removes the last character from the given <a string>.
#
# @a string: The string to manipulate.
#
# @r The <a string> without its last character.
#
# @i chopping

proc ::textutil::chop {string} {
    return [string range $string 0 [expr {[string length $string]-2}]]
}



# @c Removes the first character from the given <a string>.
# @c Convenience procedure.
#
# @a string: string to manipulate.
#
# @r The <a string> without its first character.
#
# @i tail

proc ::textutil::tail {string} {
    return [string range $string 1 end]
}



# @c Capitalizes first character of the given <a string>.
# @c Complementary procedure to <p ::textutil::uncap>.
#
# @a string: string to manipulate.
#
# @r The <a string> with its first character capitalized.
#
# @i capitalize

proc ::textutil::cap {string} {
    return [string toupper [string index $string 0]][string range $string 1 end]
}

# @c unCapitalizes first character of the given <a string>.
# @c Complementary procedure to <p ::textutil::cap>.
#
# @a string: string to manipulate.
#
# @r The <a string> with its first character uncapitalized.
#
# @i uncapitalize

proc ::textutil::uncap {string} {
    return [string tolower [string index $string 0]][string range $string 1 end]
}


# Compute the longest string which is common to all strings given to
# the command, and at the beginning of said strings, i.e. a prefix. If
# only one argument is specified it is treated as a list of the
# strings to look at. If more than one argument is specified these
# arguments are the strings to be looked at. If only one string is
# given, in either form, the string is returned, as it is its own
# longest common prefix.

proc ::textutil::longestCommonPrefix {args} {
    return [longestCommonPrefixList $args]
}

proc ::textutil::longestCommonPrefixList {list} {
    if {[llength $list] == 0} {
	return ""
    } elseif {[llength $list] == 1} {
	return [lindex $list 0]
    }

    set list [lsort  $list]
    set min  [lindex $list 0]
    set max  [lindex $list end]

    # Min and max are the two strings which are most different. If
    # they have a common prefix, it will also be the common prefix for
    # all of them.

    # Fast bailouts for common cases.

    set n [string length $min]
    if {$n == 0}                         {return ""}
    if {0 == [string compare $min $max]} {return $min}

    set prefix ""
    for {set i 0} {$i < $n} {incr i} {
	if {0 == [string compare [set x [string range $min 0 $i]] [string range $max 0 $i]]} {
	    set prefix $x
	    continue
	}
	break
    }
    return $prefix
}



source [ file join [ file dirname [ info script ] ] adjust.tcl ]
source [ file join [ file dirname [ info script ] ] split.tcl ]
source [ file join [ file dirname [ info script ] ] tabify.tcl ]
source [ file join [ file dirname [ info script ] ] trim.tcl ]

# Do the [package provide] last, in case there is an error in the code above.
package provide textutil 0.6.1
