## -*- tcl -*-
## (C) 2010 Andreas Kupries <andreas_kupries@users.sourceforge.net>
## BSD Licensed
# # ## ### ##### ######## ############# ######################

#
# Database of named directions, for use in the diagram controller.
#
# Directions are identified by name and each has a set of attributes,
# each identified by name, with associated value. The attributes are
# not typed.
#
# Standard attributes are 'angle' and 'oppposite', the first providing
# the angle of the direction, in degrees (0-360, 0 == right/east, 90
# == up/north), and the second naming the complentary direction going
# into the opposite direction (+/- 180 degrees).
#
# The eight directions (octants) of the compass rose are predefined,
# standard.
#
# Beyond the directions the system also manages 'aliases',
# i.e. alternate/secondary names for the primary directions.
#
# All names are handled case-insensitive!
#

##
# # ## ### ##### ######## ############# ######################
## Requisites

package require Tcl 8.5 ; # Want the nice things it brings (dicts, {*}, etc.)
package require snit    ; # Object framework.

# # ## ### ##### ######## ############# ######################
## Implementation

snit::type ::diagram::direction {

    # # ## ### ##### ######## ############# ######################
    ## Public API :: Extending the database

    method {new direction} {name args} {
	set thename [string tolower $name]
	# Argument validation.
	if {[info exists myinfo($thename)] ||
	    [info exists myalias($thename)]} {
	    return -code error "direction already known"
	} elseif {[llength $args] % 2 == 1} {
	    return -code error "Expected a dictionary, got \"$args\""
	} elseif {![dict exists $args angle]} {
	    return -code error "Standard attribute 'angle' is missing"
	} elseif {![dict exists $args opposite]} {
	    return -code error "Standard attribute 'opposite' is missing"
	}
	# Note: Can't check the value of opposite, a direction, for
	# existence, because then we are unable to define the pairs.

	# Should either check the angle, or auto-reduce to the proper
	# interval.

	set myinfo($thename) $args
	return
    }

    method {new alias} {name primary} {
	set thename    [string tolower $name]
	set theprimary [string tolower $primary]
	# Argument validation.
	if {[info exists myalias($thename)]} {
	    return -code error "alias already known"
	} elseif {![info exists myalias($theprimary)] &&
		  ![info exists myinfo($theprimary)]} {
	    return -code error "existing direction expected, not known"
	}
	# (*a) Resolve alias to alias in favor of the underlying
	# primary => Short lookup, no iteration required.
	if {[info exists myalias($theprimary)]} {
	    set theprimary $myalias($theprimary)
	}
	# And remember the mapping.
	set mydb($thename) $theprimary
	return
    }

    # # ## ### ##### ######## ############# ######################
    ## Public API :: Validate directions, either as explict angle, or named.
    ##               and return it normalized (angle reduced to
    ##               interval, primary name of any alias).

    method validate {direction} {
	if {[Norm $direction angle]} { return $angle }
	set d $direction
	# Only one alias lookup necessary, see (*a) in 'new alias'.
	if {[info exists myalias($d)]} { set d $myalias($d) }
	if {[info exists myinfo($d)]}  { return $d }
	return -code error "Expected direction, got \"$direction\""
    }

    method is {d} {
	if {[Norm $d angle]} { return 1 }
	# Only one alias lookup necessary, see (*a) in 'new alias'.
	if {[info exists myalias($d)]} { set d $myalias($d) }
	return [info exists myinfo($d)]
    }

    method isStrict {d} {
	# Only one alias lookup necessary, see (*a) in 'new alias'.
	if {[info exists myalias($d)]} { set d $myalias($d) }
	return [info exists myinfo($d)]
    }

    method map {corners c} {
	if {[dict exists $corners $c]} {
	    return $c
	} elseif {[$self is $c]} {
	    set new [$self validate $c]
	    if {$new ne $c} {
		return $new
	    }
	}

	# Find nearest corner by angle.
	set angle [$self get $c angle]
	set delta Inf
	set min {}
	foreach d [dict keys $corners] {
	    if {![$self isStrict $d]} continue
	    if {[catch {
		set da [$self get $d angle]
	    }]} continue
	    set dda [expr {abs($da - $angle)}]
	    if {$dda >= $delta} continue
	    set delta $dda
	    set min   $d
	}
	if {$min ne $c} {
	    return $min
	}
	return $c
    }

    # # ## ### ##### ######## ############# ######################
    ## Public API :: Retrieve directional attributes (all, or
    ##               specific). Accepts angles as well, and uses
    ##               nearest named direction.

    method get {direction {detail {}}} {
	if {[Norm $direction angle]} {
	    set d [$self FindByAngle $angle]
	} elseif {[info exists myalias($direction)]} {
	    set d $myalias($direction)
	} else {
	    set d $direction
	}
	if {[info exists myinfo($d)]}  {
	    if {[llength [info level 0]] == 7} {
		return [dict get $myinfo($d) $detail]
	    } else {
		return $myinfo($d)
	    }
	}
	return -code error "Expected direction, got \"$direction\""
    }

    # # ## ### ##### ######## ############# ######################

    proc Norm {angle varname} {
	if {![string is double -strict $angle]} { return 0 }
	while {$angle < 0}   { set angle [expr {$angle + 360}] }
	while {$angle > 360} { set angle [expr {$angle - 360}] }
	upvar 1 $varname normalized
	set normalized $angle
	return 1
    }

    method FindByAngle {angle} {
	# Find nearest named angle.
	set name {}
	set delta 720
	foreach k [array names myinfo] {
	    if {![dict exists $myinfo($k) angle]} continue
	    set a [dict get $myinfo($k) angle]
	    if {$a eq {}} continue
	    set d [expr {abs($a-$angle)}]
	    if {$d < $delta} {
		set delta $d
		set name $k
	    }
	}
	return $name
    }

    # # ## ### ##### ######## ############# ######################
    ## Instance data, database tables as arrays, keyed by direction
    ## and alias names.

    # Standard directions, the eight sections of the compass rose,
    # with angles and opposite, complementary direction.
    #
    #  135   90  45
    #     nw n ne
    #       \|/
    # 180 w -*- e 0
    #       /|\.
    #     sw s se
    #  225  270  315

    variable myinfo -array {
	east       {angle   0  opposite west     }
	northeast  {angle  45  opposite southwest}
	north      {angle  90  opposite south    }
	northwest  {angle 135  opposite southeast}
	west       {angle 180  opposite east     }
	southwest  {angle 225  opposite northeast}
	south      {angle 270  opposite north    }
	southeast  {angle 315  opposite northwest}

	center     {}
    }

    # Predefined aliases for the standard directions
    # Cardinal and intermediate directions.
    # Names and appropriate unicode symbols.
    variable myalias -array {
	c         center

	w         west         left       west         \u2190 west
	s         south        down       south        \u2191 north
	e         east         right      east         \u2192 east
	n         north        up         north        \u2193 south

	t         north        top        north	       r      east
	b         south        bottom     south	       l      west
	bot       south

	nw        northwest    up-left    northwest    \u2196 northwest
	ne        northeast    up-right   northeast    \u2197 northeast
	se        southeast    down-right southeast    \u2198 southeast
	sw        southwest    down-left  southwest    \u2199 southwest

	upleft    northwest    leftup     northwest	
	upright   northeast    rightup    northeast
	downright southeast    rightdown  southeast
	downleft  southwest    leftdown   southwest	
    }

    ##
    # # ## ### ##### ######## ############# ######################
}

# # ## ### ##### ######## ############# ######################
## Ready

package provide diagram::direction 1
