## -*- tcl -*-
## (C) 2010 Andreas Kupries <andreas_kupries@users.sourceforge.net>
## BSD Licensed
# # ## ### ##### ######## ############# ######################

#
# Database of the created/drawn elements, with their canvas items,
# corners (named points), and sub-elements.
#

##
# # ## ### ##### ######## ############# ######################
## Requisites

package require Tcl 8.5              ; # Want the nice things it
				       # brings (dicts, {*}, etc.)
package require snit                 ; # Object framework.
package require math::geometry 1.1.2 ; # Vector math (points, line
				       # (segments), poly-lines).
package require diagram::point

# # ## ### ##### ######## ############# ######################
## Implementation

snit::type ::diagram::element {
    # # ## ### ##### ######## ############# ######################

    typemethod validate {id} {
	if {[$type is $id]} {return $id}
	return -code error "Expected element id, got \"$id\""
    }

    typemethod is {id} {
	return [expr {[llength $id] == 2 &&
		      [lindex $id 0] eq "element" &&
		      [string is integer -strict [lindex $id 1]] &&
		      ([lindex $id 1] >= 1)}]
    }

    # # ## ### ##### ######## ############# ######################

    method shape {shape} {
	set myshape($shape) .
	return
    }

    method isShape {shape} {
	return [info exists myshape($shape)]
    }


    # # ## ### ##### ######## ############# ######################
    ## Public API :: Extending the database

    method new {shape corners items subelements} {
	# Generate key
	set id [NewIdentifier]

	# Save the element information.
	set myelement($id) [dict create \
				shape    $shape \
				corners  $corners \
				items    $items \
				elements $subelements]

	lappend myhistory()       $id
	lappend myhistory($shape) $id

	return $id
    }

    method drop {} {
	set mycounter 0
	array unset myelement *
	array unset myhistory *
	set myhistory() {}
	return
    }

    method {history get} {} {
	return [array get myhistory]
    }

    method {history set} {history} {
	array unset myhistory *
	array set   myhistory $history
	return
    }

    method {history find} {shape offset} {
	#  1, 2,...: Offset from the beginning of history, forward.
	# -1,-2,...: Offset from the end history, backward.

	if {$offset < 0} {
	    set offset [expr {[llength $myhistory($shape)] + $offset}]
	} else {
	    incr offset -1
	}

	#parray myhistory
	#puts E|hf|$shape|$offset|

	return [lindex $myhistory($shape) $offset]
    }

    # # ## ### ##### ######## ############# ######################
    ## Public API :: Query database.

    method elements {} {
	return $myhistory()
    }

    method corner {id corner} {
	#puts MAP($corner)=|[MapCorner $id $corner]|
	set corners [dict get $myelement($id) corners]
	return [dict get $corners [$dir map $corners $corner]]
    }

    method corners {id} {
	return [dict get $myelement($id) corners]
    }

    method names {id {pattern *}} {
	return [dict keys [dict get $myelement($id) corners] $pattern]
    }

    method items {args} {
	set items {}
	foreach id $args {
	    lappend items {*}[dict get $myelement($id) items]
	    lappend items {*}[$self items {*}[dict get $myelement($id) elements]]
	}

	# Elements with sub-elements elements can cause canvas items
	# to appear multiple times. Reduce this to only one
	# appearance. Otherwise items may be processed multiple times
	# later.

	return [lsort -uniq $items]
    }

    method bbox {args} {
	# We compute the bounding box from the corners we have for the
	# specified elements. This makes the assumption that the
	# convex hull of the element's corners is a good approximation
	# of the areas they cover.
	#
	# (1) We cannot fall back to canvas items, as the items may
	# cover a much smaller area than the system believes. This
	# notably happens for text elements. In essence a user-
	# declared WxH would be ignored by looking at the canvas.
	#
	# (2) We have to look at all corners because the simple NW/SE
	# diagonal may underestimate the box. This happens for circles
	# where these anchors are near the circle boundary and thus
	# describe the in-scribed box, instead of the outer bounds.

	# Note that corners may contain other information than
	# points. This is why the corner values are type tagged,
	# allowing us to ignore the non-point corners.

	set polyline {}
	foreach id $args {
	    foreach v [dict values [dict get $myelement($id) corners]] {
		lassign $v cmd detail
		if {$cmd ne "point"} continue
		lappend polyline [geo::x $detail] [geo::y $detail]
	    }
	}

	return [geo::bbox $polyline]
    }

    # # ## ### ##### ######## ############# ######################
    ## Public API :: Move elements to a point.

    method relocate {id destination corner canvas} {

	#puts \trelocate($id).$corner\ @$destination

	# Move the id'entified element such that the corner's point is
	# at the destination.

	# Retrieve element data.
	array set el $myelement($id)

	# Find current location of the specified corner.
	set origin [diagram::point unbox [$self corner $id $corner]]

	#puts \t$corner=$origin

	# Determine the movement vector which brings the corner into
	# coincidence with the destination.
	set delta [geo::- $destination $origin]

	#puts \tdelta=$delta

	# And perform the movement.
	$self Move $id $delta $canvas
	return
    }

    method move {delta corners} {
	set newcorners {}
	foreach {key location} $corners {
	    #puts PLACE|$key|$location|$delta|
	    if {[llength $location] == 2} {
		lassign $location cmd detail
		if {$cmd eq "point"} {
		    #puts \tSHIFT
		    lappend newcorners $key \
			[list $cmd [geo::+ $detail $delta]]
		} else {
		    lappend newcorners $key $location
		}
	    } else {
		lappend newcorners $key $location
	    }
	}

	return $newcorners
    }

    method Move {id delta canvas} {
	# Retrieve element data.
	array set el $myelement($id)

	# Move the primary items on the canvas.
	foreach item $el(items) {
	    $canvas move $item {*}$delta
	}

	# Recursively move child elements
	foreach sid  $el(elements) {
	    $self Move $sid $delta $canvas
	}

	# And modify the corners appropriately

	set newcorners [$self move $delta $el(corners)]

	dict set myelement($id) corners $newcorners
	return
    }

    # # ## ### ##### ######## ############# ######################

    constructor {thedir} {
	set dir $thedir
	return
    }

    # # ## ### ##### ######## ############# ######################

    proc NewIdentifier {} {
	upvar 1 mycounter mycounter
	return [list element [incr mycounter]]
    }

    # # ## ### ##### ######## ############# ######################
    ## Instance data, database tables as arrays, keyed by direction
    ## and alias names.

    component dir                ; # Database of named directions.
                                   # Used to check for and resolve
                                   # corner aliases.
    variable mycounter 0         ; # Counter for the generation of
				   # element identifiers. See
				   # 'NewIdentifier' for the user.
    variable myelement -array {} ; # Database of drawn elements. Maps
				   # from element identifiers to a
				   # dictionary holding the pertinent
				   # information (type, canvas items,
				   # sub elements, and corners (aka
				   # attributes).
    variable myhistory -array {
	{} {}
    }                            ; # History database. Keyed by
				   # element type, they are mapped to
				   # lists of element identifiers
				   # naming the elements in order of
				   # creation. The empty key has the
				   # history without regard to type.

    variable myshape -array {}  ; # Database of element shapes.

    ##
    # # ## ### ##### ######## ############# ######################
}

namespace eval ::diagram::element::geo {
    namespace import ::math::geometry::*
}

# # ## ### ##### ######## ############# ######################
## Ready

package provide diagram::element 1
