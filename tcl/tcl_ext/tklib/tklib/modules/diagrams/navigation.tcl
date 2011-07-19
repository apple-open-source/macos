## -*- tcl -*-
## (C) 2010 Andreas Kupries <andreas_kupries@users.sourceforge.net>
## BSD Licensed
# # ## ### ##### ######## ############# ######################

#
# Auto-layout management
#

##
# # ## ### ##### ######## ############# ######################
## Requisites

package require Tcl 8.5              ; # Want the nice things it
				       # brings (dicts, {*}, etc.)
package require snit                 ; # Object framework.
package require struct::stack
package require diagram::point

# # ## ### ##### ######## ############# ######################
## Implementation

snit::type ::diagram::navigation {

    # # ## ### ##### ######## ############# ######################
    ## Public API :: Modify the state

    method reset {} {
	set mylocation {0 0}
	set mydirection east
	set mycorner    west
	set mycorners   {}
	$mystack clear
	return
    }

    method turn {direction {commit 0}} {
	#puts T|$direction|$commit
	set mydirection [$mydirections validate $direction]
	set mycorner    [$mydirections get $mydirection opposite]
	#puts O|$mycorner

	if {$commit && [dict exists $mycorners $mydirection]} {
	    set mylocation \
		[diagram::point unbox \
		     [diagram::point absolute \
			  [dict get $mycorners $mydirection]]]
	}
	return
    }

    method move {newcorners} {
	#puts M|$newcorners
	if {[dict exists $newcorners end]} {
	    set mycorners {}
	    set at [dict get $newcorners end]
	} else {
	    # Note: We map mydirection to the corners to handle the
	    # possibility of directions which are not on the compass
	    # rose. Such are mapped to the nearest compass or other
	    # direction which is supported by the element we have
	    # moved to.
	    set mycorners $newcorners
	    set at [dict get $newcorners \
			[$mydirections map $newcorners $mydirection]]
	}

	set mylocation \
	    [diagram::point unbox [diagram::point absolute $at]]
	return
    }

    # # ## ### ##### ######## ############# ######################
    ## Public API :: State nesting

    method save {} {
	$mystack push [list \
			   $mylocation \
			   $mydirection \
			   $mycorner \
			   $mycorners]
	return
    }

    method restore {} {
	lassign [$mystack pop] \
	    mylocation \
	    mydirection \
	    mycorner \
	    mycorners
	return
    }

    # # ## ### ##### ######## ############# ######################
    ## Public API :: Querying

    method at {} {
	# TODO :: gap processing goes here -- maybe not required, given 'chop'.
	return $mylocation
    }

    method corner {} {
	return $mycorner
    }

    method direction {} {
	return $mydirection
    }

    # # ## ### ##### ######## ############# ######################
    ## Public API ::

    constructor {directions} {
	install mystack using struct::stack ${selfns}::STACK
	set mydirections $directions
	return
    }

    # # ## ### ##### ######## ############# ######################
    ## Instance data,

    component mystack
    component mydirections

    variable mylocation     {0 0} ; # attribute 'at' default
    variable mydirection    east  ; # current layout direction.
    variable mycorner       west  ; # attribute 'with' default
				    # (opposite of direction').
    variable mycorners      {}    ; # The corners we can turn to.

    ##
    # # ## ### ##### ######## ############# ######################
}

# # ## ### ##### ######## ############# ######################
## Ready

package provide diagram::navigation 1
