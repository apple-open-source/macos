# -*- tcl -*-
#
# widget.tcl --
#
# megawidget package that uses snit as the object system (snidgets)
#
# Copyright (c) 2005 Jeffrey Hobbs
#
# RCS: @(#) $Id: widget.tcl,v 1.5 2006/09/29 16:25:07 hobbs Exp $
#

package require Tk 8.4
package require snit

#package provide Widget 3.0 ; # at end

namespace eval ::widget {
    if 0 {
	variable HaveMarlett \
	    [expr {[lsearch -exact [font families] "Marlett"] != -1}]
	snit::macro widget::HaveMarlett {} [list return $::widget::HaveMarlett]
    }
}


# widget::propagate -- (snit macro)
#
#   Propagates an option to multiple components
#
# Arguments:
#   option  option definition
#   args
# Results:
#   Create method Propagate$option
#
snit::macro widget::propagate {option args} {
    # propagate option $optDefn ?-default ...? to $components ?as $realopt?
    set idx [lsearch -exact $args "to"]
    set cmd [linsert [lrange $args 0 [expr {$idx - 1}]] 0 option $option]
    foreach {components as what} [lrange $args [expr {$idx + 1}] end] {
	break
    }
    # ensure we have just the option name
    set option [lindex $option 0]
    set realopt [expr {$what eq "" ? $option : $what}]
    lappend cmd -configuremethod Propagate$option
    eval $cmd

    set body "\n"
    foreach comp $components {
        append body "\$[list $comp] configure [list $realopt] \$value\n"
    }
    append body "set [list options($option)] \$value\n"

    method Propagate$option {option value} $body
}

if {0} {
    # Currently not feasible due to snit's compiler-as-slave-interp
    snit::macro widget::tkoption {option args} {
	# XXX should support this
	# tkoption {-opt opt Opt} ?-default ""? from /wclass/ ?as $wopt?
    }

    snit::macro widget::tkresource {wclass wopt} {
	# XXX should support this
	# tkresource $wclass $wopt
	set w ".#widget#$wclass"
	if {![winfo exists $w]} {
	    set w [$wclass $w]
	}
	set value [$w cget $wopt]
	after idle [list destroy $w]
	return $value
    }
}

# widget::tkresource --
#
#   Get the default option value from a widget class
#
# Arguments:
#   wclass  widget class
#   wopt    widget option
# Results:
#   Returns default value of $wclass $wopt value
#
proc widget::tkresource {wclass wopt} {
    # XXX should support this
    # tkresource $wclass $wopt
    set w ".#widget#$wclass"
    if {![winfo exists $w]} {
	set w [$wclass $w]
    }
    set value [$w cget $wopt]
    after idle [list destroy $w]
    return $value
}

# ::widget::validate --
#
#   Used by widgets for option validate - *private* spec may change
#
# Arguments:
#   as     type to compare as
#   range  range/data info specific to $as
#   option option name
#   value  value being validated
#
# Results:
#   Returns error or empty
#
proc ::widget::isa {as args} {
    foreach {range option value} $args { break }
    if {$as eq "list"} {
	if {[lsearch -exact $range $value] == -1} {
	    return -code error "invalid $option option \"$value\",\
		must be one of [join $range {, }]"
	}
    } elseif {$as eq "boolean" || $as eq "bool"} {
	foreach {option value} $args { break }
	if {![string is boolean -strict $value]} {
	    return -code error "$option requires a boolean value"
	}
    } elseif {$as eq "integer" || $as eq "int"} {
	foreach {min max} $range { break }
	if {![string is integer -strict $value]
	    || ($value < $min) || ($value > $max)} {
	    return -code error "$option requires an integer in the\
		range \[$min .. $max\]"
	}
    } elseif {$as eq "listofinteger" || $as eq "listofint"} {
	if {$range eq ""} { set range [expr {1<<16}] }
	set i 0
	foreach val $value {
	    if {![string is integer -strict $val] || ([incr i] > $range)} {
		return -code error "$option requires an list of integers"
	    }
	}
    } elseif {$as eq "double"} {
	foreach {min max} $range { break }
	if {![string is double -strict $value]
	    || ($value < $min) || ($value > $max)} {
	    return -code error "$option requires a double in the\
		range \[$min .. $max\]"
	}
    } elseif {$as eq "window"} {
	foreach {option value} $args { break }
	if {$value eq ""} { return }
	if {![winfo exists $value]} {
	    return -code error "invalid window \"$value\""
	}
    } else {
	return -code error "unknown validate type \"$as\""
    }
    return
}

package provide widget 3.0
