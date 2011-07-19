## -*- tcl -*-
## (C) 2010 Andreas Kupries <andreas_kupries@users.sourceforge.net>
## BSD Licensed
# # ## ### ##### ######## ############# ######################

#
# diagram points.
#
# Type validation and implementation of the various operations on
# points and lines. The low-level commands for this come from
# math::geometry. The operations here additionally (un)box from/to
# tagged values. They also handle operations mixing polar and
# cartesian specifications.
#

##
# # ## ### ##### ######## ############# ######################
## Requisites

package require Tcl 8.5              ; # Want the nice things it
                                       # brings (dicts, {*}, etc.)
package require math::geometry 1.1.2 ; # Vector math (points, line
				       # (segments), poly-lines).

namespace eval ::diagram::point {
    namespace export is isa validate absolute at delta by unbox + - | resolve
    namespace ensemble create
}

# # ## ### ##### ######## ############# ######################
## Implementation
# # ## ### ##### ######## ############# ######################
## Public API :: validation

proc ::diagram::point::validate {value} {
    if {[is $value]} {return $value}
    return -code error "Expected diagram::point, got \"$value\""
}

proc ::diagram::point::absolute {value} {
    if {[isa $value]} {return $value}
    return -code error "Expected absolute diagram::point, got \"$value\""
}

proc ::diagram::point::is {value} {
    return [expr {([llength $value] == 2) &&
		  ([lindex $value 0] in {point + by})}]
}

proc ::diagram::point::isa {value} {
    # note overlap with constructor 'at'.
    return [expr {([llength $value] == 2) ||
		  ([lindex $value 0] eq "point")}]
}

# # ## ### ##### ######## ############# ######################
## Public API :: Constructors

# Absolute location
proc ::diagram::point::at {x y} {
    return [list point [list $x $y]]
}

# Relative location, cartesian
proc ::diagram::point::delta {dx dy} {
    return [list + [list $dx $dy]]
}

# Relative location, polar
proc ::diagram::point::by {distance angle} {
    return [list by [list $distance $angle]]
}

# # ## ### ##### ######## ############# ######################

proc ::diagram::point::unbox {p} {
    return [lindex $p 1]
}

# # ## ### ##### ######## ############# ######################
## Public API :: Point arithmetic

proc ::diagram::point::+ {a b} {
    set a [2cartesian [validate $a]]
    set b [2cartesian [validate $b]]

    # Unboxing

    lassign $a atag adetail
    lassign $b btag bdetail

    # Calculation and result type determination

    set result [geo::+ $adetail $bdetail]
    set rtype  [expr {(($atag eq "point") || ($btag eq "point"))
		      ? "at"
		      : "delta"}]

    return [$rtype {*}$result]
}

proc ::diagram::point::- {a b} {
    set a [2cartesian [validate $a]]
    set b [2cartesian [validate $b]]

    # Unboxing

    lassign $a atag adetail
    lassign $b btag bdetail

    # Calculation and result type determination

    set result [geo::- $adetail $bdetail]
    set rtype  [expr {(($atag eq "point") || ($btag eq "point"))
		      ? "at"
		      : "delta"}]

    return [$rtype {*}$result]
}

proc ::diagram::point::| {a b} {
    set a [2cartesian [absolute $a]]
    set b [2cartesian [absolute $b]]

    # Unboxing

    lassign $a atag adetail ; lassign $adetail ax ay
    lassign $b btag bdetail ; lassign $bdetail bx by

    # Calculation of the projection.
    return [at $ax $by]
}

# # ## ### ##### ######## ############# ######################

proc ::diagram::point::resolve {base p} {
    #puts P|resolve|$base|$p|

    # The base is an untagged point, p is a tagged point or delta.
    lassign $p tag detail

    # A point is returned unchanged.
    if {$tag eq "point"} { return [unbox $p] }

    # A delta is normalized, then added to the base.

    #puts R|$base|$p|
    #puts R|[2cartesian $p]|
    #puts R|[unbox [2cartesian $p]]|

    return [geo::+ $base [unbox [2cartesian $p]]]
}

# # ## ### ##### ######## ############# ######################

# Normalize point/delta information to cartesian
# coordinates. Input and output are both tagged, and points not
# using a polar representation are not modified.

proc ::diagram::point::2cartesian {p} {
    lassign $p tag details
    if {$tag ne "by"} { return $p }
    return [delta {*}[polar2cartesian $details]]
}

# Conversion of a delta from polar to cartesian coordinates,
# operating on untagged data.

proc ::diagram::point::polar2cartesian {polar} {
    lassign $polar distance angle
    return [geo::s* $distance [geo::direction $angle]]
}

##
# # ## ### ##### ######## ############# ######################

# # ## ### ##### ######## ############# ######################
## Ready

namespace eval ::diagram::point::geo {
    namespace import ::math::geometry::*
}

package provide diagram::point 1
