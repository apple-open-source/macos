# geometry.tcl --
#
#	Collection of geometry functions.
#
# Copyright (c) 2001 by Ideogramic ApS and other parties.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# RCS: @(#) $Id: geometry.tcl,v 1.8 2005/10/04 17:31:23 andreas_kupries Exp $

namespace eval ::math::geometry {
}

package require math

###
#
# POINTS
#
#    A point P consists of an x-coordinate, Px, and a y-coordinate, Py,
#    and both coordinates are floating point values.
#
#    Points are usually denoted by A, B, C, P, or Q.
#
###
#
# LINES
#
#    There are basically three types of lines:
#         line           A line is defined by two points A and B as the
#                        _infinite_ line going through these two points.
#                        Often a line is given as a list of 4 coordinates
#                        instead of 2 points.
#         line segment   A line segment is defined by two points A and B
#                        as the _finite_ that starts in A and ends in B.
#                        Often a line segment is given as a list of 4
#                        coordinates instead of 2 points.
#         polyline       A polyline is a sequence of connected line segments.
#
#    Please note that given a point P, the closest point on a line is given
#    by the projection of P onto the line. The closest point on a line segment
#    may be the projection, but it may also be one of the end points of the
#    line segment.
#
###
#
# DISTANCES
#
#    The distances in this package are all floating point values.
#
###



# ::math::geometry::calculateDistanceToLine
#
#       Calculate the distance between a point and a line.
#
# Arguments:
#       P             a point
#       line          a line
#
# Results:
#       dist          the smallest distance between P and the line
#
# Examples:
#     - calculateDistanceToLine {5 10} {0 0 10 10}
#       Result: 3.53553390593
#     - calculateDistanceToLine {-10 0} {0 0 10 10}
#       Result: 7.07106781187
#
proc ::math::geometry::calculateDistanceToLine {P line} {
    # solution based on FAQ 1.02 on comp.graphics.algorithms
    # L = sqrt( (Bx-Ax)^2 + (By-Ay)^2 )
    #     (Ay-Cy)(Bx-Ax)-(Ax-Cx)(By-Ay)
    # s = -----------------------------
    #                 L^2
    # dist = |s|*L
    #
    # =>
    #
    #        | (Ay-Cy)(Bx-Ax)-(Ax-Cx)(By-Ay) |
    # dist = ---------------------------------
    #                       L
    set Ax [lindex $line 0]
    set Ay [lindex $line 1]
    set Bx [lindex $line 2]
    set By [lindex $line 3]
    set Cx [lindex $P 0]
    set Cy [lindex $P 1]
    if {$Ax==$Bx && $Ay==$By} {
	return [lengthOfPolyline [concat $P [lrange $line 0 1]]]
    } else {
	set L [expr {sqrt(pow($Bx-$Ax,2) + pow($By-$Ay,2))}]
	return [expr {abs(($Ay-$Cy)*($Bx-$Ax)-($Ax-$Cx)*($By-$Ay)) / $L}]
    }
}

# ::math::geometry::findClosestPointOnLine
#
#       Return the point on a line which is closest to a given point.
#
# Arguments:
#       P             a point
#       line          a line
#
# Results:
#       Q             the point on the line that has the smallest
#                     distance to P
#
# Examples:
#     - findClosestPointOnLine {5 10} {0 0 10 10}
#       Result: 7.5 7.5
#     - findClosestPointOnLine {-10 0} {0 0 10 10}
#       Result: -5.0 -5.0
#
proc ::math::geometry::findClosestPointOnLine {P line} {
    return [lindex [findClosestPointOnLineImpl $P $line] 0]
}

# ::math::geometry::findClosestPointOnLineImpl
#
#       PRIVATE FUNCTION USED BY OTHER FUNCTIONS.
#       Find the point on a line that is closest to a given point.
#
# Arguments:
#       P             a point
#       line          a line defined by points A and B
#
# Results:
#       Q             the point on the line that has the smallest
#                     distance to P
#       r             r has the following meaning:
#                        r=0      P = A
#                        r=1      P = B
#                        r<0      P is on the backward extension of AB
#                        r>1      P is on the forward extension of AB
#                        0<r<1    P is interior to AB
#
proc ::math::geometry::findClosestPointOnLineImpl {P line} {
    # solution based on FAQ 1.02 on comp.graphics.algorithms
    #   L = sqrt( (Bx-Ax)^2 + (By-Ay)^2 )
    #        (Cx-Ax)(Bx-Ax) + (Cy-Ay)(By-Ay)
    #   r = -------------------------------
    #                     L^2
    #   Px = Ax + r(Bx-Ax)
    #   Py = Ay + r(By-Ay)
    set Ax [lindex $line 0]
    set Ay [lindex $line 1]
    set Bx [lindex $line 2]
    set By [lindex $line 3]
    set Cx [lindex $P 0]
    set Cy [lindex $P 1]
    if {$Ax==$Bx && $Ay==$By} {
	return [list [list $Ax $Ay] 0]
    } else {
	set L [expr {sqrt(pow($Bx-$Ax,2) + pow($By-$Ay,2))}]
	set r [expr {(($Cx-$Ax)*($Bx-$Ax) + ($Cy-$Ay)*($By-$Ay))/pow($L,2)}]
	set Px [expr {$Ax + $r*($Bx-$Ax)}]
	set Py [expr {$Ay + $r*($By-$Ay)}]
	return [list [list $Px $Py] $r]
    }
}

# ::math::geometry::calculateDistanceToLineSegment
#
#       Calculate the distance between a point and a line segment.
#
# Arguments:
#       P             a point
#       linesegment   a line segment
#
# Results:
#       dist          the smallest distance between P and any point
#                     on the line segment
#
# Examples:
#     - calculateDistanceToLineSegment {5 10} {0 0 10 10}
#       Result: 3.53553390593
#     - calculateDistanceToLineSegment {-10 0} {0 0 10 10}
#       Result: 10.0
#
proc ::math::geometry::calculateDistanceToLineSegment {P linesegment} {
    set result [calculateDistanceToLineSegmentImpl $P $linesegment]
    set distToLine [lindex $result 0]
    set r [lindex $result 1]
    if {$r<0} {
	return [lengthOfPolyline [concat $P [lrange $linesegment 0 1]]]
    } elseif {$r>1} {
	return [lengthOfPolyline [concat $P [lrange $linesegment 2 3]]]
    } else {
	return $distToLine
    }
}

# ::math::geometry::calculateDistanceToLineSegmentImpl
#
#       PRIVATE FUNCTION USED BY OTHER FUNCTIONS.
#       Find the distance between a point and a line.
#
# Arguments:
#       P             a point
#       linesegment   a line segment A->B
#
# Results:
#       dist          the smallest distance between P and the line
#       r             r has the following meaning:
#                        r=0      P = A
#                        r=1      P = B
#                        r<0      P is on the backward extension of AB
#                        r>1      P is on the forward extension of AB
#                        0<r<1    P is interior to AB
#
proc ::math::geometry::calculateDistanceToLineSegmentImpl {P linesegment} {
    # solution based on FAQ 1.02 on comp.graphics.algorithms
    # L = sqrt( (Bx-Ax)^2 + (By-Ay)^2 )
    #     (Ay-Cy)(Bx-Ax)-(Ax-Cx)(By-Ay)
    # s = -----------------------------
    #                 L^2
    #      (Cx-Ax)(Bx-Ax) + (Cy-Ay)(By-Ay)
    # r = -------------------------------
    #                   L^2
    # dist = |s|*L
    #
    # =>
    #
    #        | (Ay-Cy)(Bx-Ax)-(Ax-Cx)(By-Ay) |
    # dist = ---------------------------------
    #                       L
    set Ax [lindex $linesegment 0]
    set Ay [lindex $linesegment 1]
    set Bx [lindex $linesegment 2]
    set By [lindex $linesegment 3]
    set Cx [lindex $P 0]
    set Cy [lindex $P 1]
    if {$Ax==$Bx && $Ay==$By} {
	return [list [lengthOfPolyline [concat $P [lrange $linesegment 0 1]]] 0]
    } else {
	set L [expr {sqrt(pow($Bx-$Ax,2) + pow($By-$Ay,2))}]
	set r [expr {(($Cx-$Ax)*($Bx-$Ax) + ($Cy-$Ay)*($By-$Ay))/pow($L,2)}]
	return [list [expr {abs(($Ay-$Cy)*($Bx-$Ax)-($Ax-$Cx)*($By-$Ay)) / $L}] $r]
    }
}

# ::math::geometry::findClosestPointOnLineSegment
#
#       Return the point on a line segment which is closest to a given point.
#
# Arguments:
#       P             a point
#       linesegment   a line segment
#
# Results:
#       Q             the point on the line segment that has the
#                     smallest distance to P
#
# Examples:
#     - findClosestPointOnLineSegment {5 10} {0 0 10 10}
#       Result: 7.5 7.5
#     - findClosestPointOnLineSegment {-10 0} {0 0 10 10}
#       Result: 0 0
#
proc ::math::geometry::findClosestPointOnLineSegment {P linesegment} {
    set result [findClosestPointOnLineImpl $P $linesegment]
    set Q [lindex $result 0]
    set r [lindex $result 1]
    if {$r<0} {
	return [lrange $linesegment 0 1]
    } elseif {$r>1} {
	return [lrange $linesegment 2 3]
    } else {
	return $Q
    }

}

# ::math::geometry::calculateDistanceToPolyline
#
#       Calculate the distance between a point and a polyline.
#
# Arguments:
#       P           a point
#       polyline    a polyline
#
# Results:
#       dist        the smallest distance between P and any point
#                   on the polyline
#
# Examples:
#     - calculateDistanceToPolyline {10 10} {0 0 10 5 20 0}
#       Result: 5.0
#     - calculateDistanceToPolyline {5 10} {0 0 10 5 20 0}
#       Result: 6.7082039325
#
proc ::math::geometry::calculateDistanceToPolyline {P polyline} {
    set minDist "none"
    foreach {Ax Ay} [lrange $polyline 0 end-2] {Bx By} [lrange $polyline 2 end] {
	set dist [calculateDistanceToLineSegment $P [list $Ax $Ay $Bx $By]]
	if {$minDist=="none" || $dist < $minDist} {
	    set minDist $dist
	}
    }
    return $minDist
}

# ::math::geometry::findClosestPointOnPolyline
#
#       Return the point on a polyline which is closest to a given point.
#
# Arguments:
#       P           a point
#       polyline    a polyline
#
# Results:
#       Q           the point on the polyline that has the smallest
#                   distance to P
#
# Examples:
#     - findClosestPointOnPolyline {10 10} {0 0 10 5 20 0}
#       Result: 10 5
#     - findClosestPointOnPolyline {5 10} {0 0 10 5 20 0}
#       Result: 8.0 4.0
#
proc ::math::geometry::findClosestPointOnPolyline {P polyline} {
    set closestPoint "none"
    foreach {Ax Ay} [lrange $polyline 0 end-2] {Bx By} [lrange $polyline 2 end] {
	set Q [findClosestPointOnLineSegment $P [list $Ax $Ay $Bx $By]]
	set dist [lengthOfPolyline [concat $P $Q]]
	if {$closestPoint=="none" || $dist<$closestDistance} {
	    set closestPoint $Q
	    set closestDistance $dist
	}
    }
    return $closestPoint
}






# ::math::geometry::lengthOfPolyline
#
#       Find the length of a polyline, i.e., the sum of the
#       lengths of the individual line segments.
#
# Arguments:
#       polyline      a polyline
#
# Results:
#       length        the length of the polyline
#
# Examples:
#     - lengthOfPolyline {0 0 5 0 5 10}
#       Result: 15.0
#
proc ::math::geometry::lengthOfPolyline {polyline} {
    set length 0
    foreach {x1 y1} [lrange $polyline 0 end-2] {x2 y2} [lrange $polyline 2 end] {
	set length [expr {$length + sqrt(pow($x1-$x2,2) + pow($y1-$y2,2))}]
	#set length [expr {$length + sqrt(($x1-$x2)*($x1-$x2) + ($y1-$y2)*($y1-$y2))}]
    }
    return $length
}




# ::math::geometry::movePointInDirection
#
#       Move a point in a given direction.
#
# Arguments:
#       P             the starting point
#       direction     the direction from P
#                     The direction is in 360-degrees going counter-clockwise,
#                     with "straight right" being 0 degrees
#       dist          the distance from P
#
# Results:
#       Q             the point which is found by starting in P and going
#                     in the given direction, until the distance between
#                     P and Q is dist
#
# Examples:
#     - movePointInDirection {0 0} 45.0 10
#       Result: 7.07106781187 7.07106781187
#
proc ::math::geometry::movePointInDirection {P direction dist} {
    set x [lindex $P 0]
    set y [lindex $P 1]
    set pi [expr {4*atan(1)}]
    set xt [expr {$x + $dist*cos(($direction*$pi)/180)}]
    set yt [expr {$y + $dist*sin(($direction*$pi)/180)}]
    return [list $xt $yt]
}


# ::math::geometry::angle
#
#       Calculates angle from the horizon (0,0)->(1,0) to a line.
#
# Arguments:
#       line          a line defined by two points A and B
#
# Results:
#       angle         the angle between the line (0,0)->(1,0) and (Ax,Ay)->(Bx,By).
#                     Angle is in 360-degrees going counter-clockwise
#
# Examples:
#     - angle {10 10 15 13}
#       Result: 30.9637565321
#
proc ::math::geometry::angle {line} {
    set x1 [lindex $line 0]
    set y1 [lindex $line 1]
    set x2 [lindex $line 2]
    set y2 [lindex $line 3]
    # - handle vertical lines
    if {$x1==$x2} {if {$y1<$y2} {return 90} else {return 270}}
    # - handle other lines
    set a [expr {atan(abs((1.0*$y1-$y2)/(1.0*$x1-$x2)))}] ; # a is between 0 and pi/2
    set pi [expr {4*atan(1)}]
    if {$y1<=$y2} {
	# line is going upwards
	if {$x1<$x2} {set b $a} else {set b [expr {$pi-$a}]}
    } else {
	# line is going downwards
	if {$x1<$x2} {set b [expr {2*$pi-$a}]} else {set b [expr {$pi+$a}]}
    }
    return [expr {$b/$pi*180}] ; # convert b to degrees
}




###
#
# Intersection procedures
#
###

# ::math::geometry::lineSegmentsIntersect
#
#       Checks whether two line segments intersect.
#
# Arguments:
#       linesegment1  the first line segment
#       linesegment2  the second line segment
#
# Results:
#       dointersect   a boolean saying whether the line segments intersect
#                     (i.e., have any points in common)
#
# Examples:
#     - lineSegmentsIntersect {0 0 10 10} {0 10 10 0}
#       Result: 1
#     - lineSegmentsIntersect {0 0 10 10} {20 20 20 30}
#       Result: 0
#     - lineSegmentsIntersect {0 0 10 10} {10 10 15 15}
#       Result: 1
#
proc ::math::geometry::lineSegmentsIntersect {linesegment1 linesegment2} {
    # Algorithm based on Sedgewick.
    set l1x1 [lindex $linesegment1 0]
    set l1y1 [lindex $linesegment1 1]
    set l1x2 [lindex $linesegment1 2]
    set l1y2 [lindex $linesegment1 3]
    set l2x1 [lindex $linesegment2 0]
    set l2y1 [lindex $linesegment2 1]
    set l2x2 [lindex $linesegment2 2]
    set l2y2 [lindex $linesegment2 3]
    return [expr {([ccw [list $l1x1 $l1y1] [list $l1x2 $l1y2] [list $l2x1 $l2y1]]\
	    *[ccw [list $l1x1 $l1y1] [list $l1x2 $l1y2] [list $l2x2 $l2y2]] <= 0) \
	    && ([ccw [list $l2x1 $l2y1] [list $l2x2 $l2y2] [list $l1x1 $l1y1]]\
	    *[ccw [list $l2x1 $l2y1] [list $l2x2 $l2y2] [list $l1x2 $l1y2]] <= 0)}]
}

# ::math::geometry::findLineSegmentIntersection
#
#       Returns the intersection point of two line segments.
#       Note: may also return "coincident" and "none".
#
# Arguments:
#       linesegment1  the first line segment
#       linesegment2  the second line segment
#
# Results:
#       P             the intersection point of linesegment1 and linesegment2.
#                     If linesegment1 and linesegment2 have an infinite number
#                     of points in common, the procedure returns "coincident".
#                     If there are no intersection points, the procedure
#                     returns "none".
#
# Examples:
#     - findLineSegmentIntersection {0 0 10 10} {0 10 10 0}
#       Result: 5.0 5.0
#     - findLineSegmentIntersection {0 0 10 10} {20 20 20 30}
#       Result: none
#     - findLineSegmentIntersection {0 0 10 10} {10 10 15 15}
#       Result: 10.0 10.0
#     - findLineSegmentIntersection {0 0 10 10} {5 5 15 15}
#       Result: coincident
#
proc ::math::geometry::findLineSegmentIntersection {linesegment1 linesegment2} {
    if {[lineSegmentsIntersect $linesegment1 $linesegment2]} {
	set lineintersect [findLineIntersection $linesegment1 $linesegment2]
	switch -- $lineintersect {

	    "coincident" {
		# lines are coincident
		set l1x1 [lindex $linesegment1 0]
		set l1y1 [lindex $linesegment1 1]
		set l1x2 [lindex $linesegment1 2]
		set l1y2 [lindex $linesegment1 3]
		set l2x1 [lindex $linesegment2 0]
		set l2y1 [lindex $linesegment2 1]
		set l2x2 [lindex $linesegment2 2]
		set l2y2 [lindex $linesegment2 3]
		# check if the line SEGMENTS overlap
		# (NOT enough to check if the x-intervals overlap (vertical lines!))
		set overlapx [intervalsOverlap $l1x1 $l1x2 $l2x1 $l2x2 0]
		set overlapy [intervalsOverlap $l1y1 $l1y2 $l2y1 $l2y2 0]
		if {$overlapx && $overlapy} {
		    return "coincident"
		} else {
		    return "none"
		}
	    }

	    "none" {
		# should never happen, because we call "lineSegmentsIntersect" first
		puts stderr "::math::geometry::findLineSegmentIntersection: suddenly no intersection?"
		return "none"
	    }

	    default {
		# lineintersect = the intersection point
		return $lineintersect
	    }
	}
    } else {
	return "none"
    }
}

# ::math::geometry::findLineIntersection {line1 line2}
#
#       Returns the intersection point of two lines.
#       Note: may also return "coincident" and "none".
#
# Arguments:
#       line1         the first line
#       line2         the second line
#
# Results:
#       P             the intersection point of line1 and line2.
#                     If line1 and line2 have an infinite number of points
#                     in common, the procedure returns "coincident".
#                     If there are no intersection points, the procedure
#                     returns "none".
#
# Examples:
#     - findLineIntersection {0 0 10 10} {0 10 10 0}
#       Result: 5.0 5.0
#     - findLineIntersection {0 0 10 10} {20 20 20 30}
#       Result: 20.0 20.0
#     - findLineIntersection {0 0 10 10} {10 10 15 15}
#       Result: coincident
#     - findLineIntersection {0 0 10 10} {5 5 15 15}
#       Result: coincident
#     - findLineIntersection {0 0 10 10} {0 1 10 11}
#       Result: none
#
proc ::math::geometry::findLineIntersection {line1 line2} {
    set l1x1 [lindex $line1 0]
    set l1y1 [lindex $line1 1]
    set l1x2 [lindex $line1 2]
    set l1y2 [lindex $line1 3]
    set l2x1 [lindex $line2 0]
    set l2y1 [lindex $line2 1]
    set l2x2 [lindex $line2 2]
    set l2y2 [lindex $line2 3]

    # Is one of the lines vertical?
    if {$l1x1==$l1x2 || $l2x1==$l2x2} {
	# One of the lines is vertical
	if {$l1x1==$l1x2 && $l2x1==$l2x2} {
	    # both lines are vertical
	    if {$l1x1==$l2x1} {
		return "coincident"
	    } else {
		return "none"
	    }
	}

	# make sure line1 is a vertical line
	if {$l1x1!=$l1x2} {
	    # interchange line 1 and 2
	    set l1x1 [lindex $line2 0]
	    set l1y1 [lindex $line2 1]
	    set l1x2 [lindex $line2 2]
	    set l1y2 [lindex $line2 3]
	    set l2x1 [lindex $line1 0]
	    set l2y1 [lindex $line1 1]
	    set l2x2 [lindex $line1 2]
	    set l2y2 [lindex $line1 3]
	}

	# get equation of line 2 (y=a*x+b)
	set a [expr {1.0*($l2y2-$l2y1)/($l2x2-$l2x1)}]
	set b [expr {$l2y1-$a*$l2x1}]

	# Calculate intersection
	set y [expr {$a*$l1x1+$b}]
	return [list $l1x1 $y]
    } else {
	# None of the lines are vertical
	# - get equation of line 1 (y=a1*x+b1)
	set a1 [expr {(1.0*$l1y2-$l1y1)/($l1x2-$l1x1)}]
	set b1 [expr {$l1y1-$a1*$l1x1}]
	# - get equation of line 2 (y=a2*x+b2)
	set a2 [expr {(1.0*$l2y2-$l2y1)/($l2x2-$l2x1)}]
	set b2 [expr {$l2y1-$a2*$l2x1}]
	
	if {abs($a2-$a1) > 0.0001} {
	    # the lines are not parallel
	    set x [expr {($b2-$b1)/($a1-$a2)}]
	    set y [expr {$a1*$x+$b1}]
	    return [list $x $y]
	} else {
	    # the lines are parallel
	    if {abs($b1-$b2) < 0.00001} {
		return "coincident"
	    } else {
		return "none"
	    }
	}
    }
}


# ::math::geometry::polylinesIntersect
#
#       Checks whether two polylines intersect.
#
# Arguments;
#       polyline1     the first polyline
#       polyline2     the second polyline
#
# Results:
#       dointersect   a boolean saying whether the polylines intersect
#
# Examples:
#     - polylinesIntersect {0 0 10 10 10 20} {0 10 10 0}
#       Result: 1
#     - polylinesIntersect {0 0 10 10 10 20} {5 4 10 4}
#       Result: 0
#
proc ::math::geometry::polylinesIntersect {polyline1 polyline2} {
    return [polylinesBoundingIntersect $polyline1 $polyline2 0]
}

# ::math::geometry::polylinesBoundingIntersect
#
#       Check whether two polylines intersect, but reduce
#       the correctness of the result to the given granularity.
#       Use this for faster, but weaker, intersection checking.
#
#       How it works:
#          Each polyline is split into a number of smaller polylines,
#          consisting of granularity points each. If a pair of those smaller
#          lines' bounding boxes intersect, then this procedure returns 1,
#          otherwise it returns 0.
#
# Arguments:
#       polyline1     the first polyline
#       polyline2     the second polyline
#       granularity   the number of points in each part-polyline
#                     granularity<=1 means full correctness
#
# Results:
#       dointersect   a boolean saying whether the polylines intersect
#
# Examples:
#     - polylinesBoundingIntersect {0 0 10 10 10 20} {0 10 10 0} 2
#       Result: 1
#     - polylinesBoundingIntersect {0 0 10 10 10 20} {5 4 10 4} 2
#       Result: 1
#
proc ::math::geometry::polylinesBoundingIntersect {polyline1 polyline2 granularity} {
    if {$granularity<=1} {
	# Use perfect intersect
	# => first pin down where an intersection point may be, and then
	#    call MultilinesIntersectPerfect on those parts
	set granularity 10 ; # optimal search granularity?
	set perfectmatch 1
    } else {
	set perfectmatch 0
    }

    # split the lines into parts consisting of $granularity points
    set polyline1parts {}
    for {set i 0} {$i<[llength $polyline1]} {incr i [expr {2*$granularity-2}]} {
	lappend polyline1parts [lrange $polyline1 $i [expr {$i+2*$granularity-1}]]
    }
    set polyline2parts {}
    for {set i 0} {$i<[llength $polyline2]} {incr i [expr {2*$granularity-2}]} {
	lappend polyline2parts [lrange $polyline2 $i [expr {$i+2*$granularity-1}]]
    }

    # do any of the parts overlap?
    foreach part1 $polyline1parts {
	foreach part2 $polyline2parts {
	    set part1bbox [bbox $part1]
	    set part2bbox [bbox $part2]
	    if {[rectanglesOverlap [lrange $part1bbox 0 1] [lrange $part1bbox 2 3] \
		    [lrange $part2bbox 0 1] [lrange $part2bbox 2 3] 0]} {
		# the lines' bounding boxes intersect
		if {$perfectmatch} {
		    foreach {l1x1 l1y1} [lrange $part1 0 end-2] {l1x2 l1y2} [lrange $part1 2 end] {
			foreach {l2x1 l2y1} [lrange $part2 0 end-2] {l2x2 l2y2} [lrange $part2 2 end] {
			    if {[lineSegmentsIntersect [list $l1x1 $l1y1 $l1x2 $l1y2] \
				    [list $l2x1 $l2y1 $l2x2 $l2y2]]} {
				# two line segments overlap
				return 1
			    }
			}
		    }
		    return 0
		} else {
		    return 1
		}
	    }
	}
    }
    return 0
}

# ::math::geometry::ccw
#
#       PRIVATE FUNCTION USED BY OTHER FUNCTIONS.
#       Returns whether traversing from A to B to C is CounterClockWise
#       Algorithm by Sedgewick.
#
# Arguments:
#       A             first point
#       B             second point
#       C             third point
#
# Reeults:
#       ccw           a boolean saying whether traversing from A to B to C
#                     is CounterClockWise
#
proc ::math::geometry::ccw {A B C} {
    set Ax [lindex $A 0]
    set Ay [lindex $A 1]
    set Bx [lindex $B 0]
    set By [lindex $B 1]
    set Cx [lindex $C 0]
    set Cy [lindex $C 1]
    set dx1 [expr {$Bx - $Ax}]
    set dy1 [expr {$By - $Ay}]
    set dx2 [expr {$Cx - $Ax}]
    set dy2 [expr {$Cy - $Ay}]
    if {$dx1*$dy2 > $dy1*$dx2} {return 1}
    if {$dx1*$dy2 < $dy1*$dx2} {return -1}
    if {($dx1*$dx2 < 0) || ($dy1*$dy2 < 0)} {return -1}
    if {($dx1*$dx1 + $dy1*$dy1) < ($dx2*$dx2+$dy2*$dy2)} {return 1}
    return 0
}







###
#
# Overlap procedures
#
###

# ::math::geometry::intervalsOverlap
#
#       Check whether two intervals overlap.
#       Examples:
#         - (2,4) and (5,3) overlap with strict=0 and strict=1
#         - (2,4) and (1,2) overlap with strict=0 but not with strict=1
#
# Arguments:
#       y1,y2         the first interval
#       y3,y4         the second interval
#       strict        choosing strict or non-strict interpretation
#
# Results:
#       dooverlap     a boolean saying whether the intervals overlap
#
# Examples:
#     - intervalsOverlap 2 4 4 6 1
#       Result: 0
#     - intervalsOverlap 2 4 4 6 0
#       Result: 1
#     - intervalsOverlap 4 2 3 5 0
#       Result: 1
#
proc ::math::geometry::intervalsOverlap {y1 y2 y3 y4 strict} {
    if {$y1>$y2} {
	set temp $y1
	set y1 $y2
	set y2 $temp
    }
    if {$y3>$y4} {
	set temp $y3
	set y3 $y4
	set y4 $temp
    }
    if {$strict} {
	return [expr {$y2>$y3 && $y4>$y1}]
    } else {
	return [expr {$y2>=$y3 && $y4>=$y1}]
    }
}

# ::math::geometry::rectanglesOverlap
#
#       Check whether two rectangles overlap (see also intervalsOverlap).
#
# Arguments:
#       P1            upper-left corner of the first rectangle
#       P2            lower-right corner of the first rectangle
#       Q1            upper-left corner of the second rectangle
#       Q2            lower-right corner of the second rectangle
#       strict        choosing strict or non-strict interpretation
#
# Results:
#       dooverlap     a boolean saying whether the rectangles overlap
#
# Examples:
#     - rectanglesOverlap {0 10} {10 0} {10 10} {20 0} 1
#       Result: 0
#     - rectanglesOverlap {0 10} {10 0} {10 10} {20 0} 0
#       Result: 1
#
proc ::math::geometry::rectanglesOverlap {P1 P2 Q1 Q2 strict} {
    set b1x1 [lindex $P1 0]
    set b1y1 [lindex $P1 1]
    set b1x2 [lindex $P2 0]
    set b1y2 [lindex $P2 1]
    set b2x1 [lindex $Q1 0]
    set b2y1 [lindex $Q1 1]
    set b2x2 [lindex $Q2 0]
    set b2y2 [lindex $Q2 1]
    # ensure b1x1<=b1x2 etc.
    if {$b1x1 > $b1x2} {
	set temp $b1x1
	set b1x1 $b1x2
	set b1x2 $temp
    }
    if {$b1y1 > $b1y2} {
	set temp $b1y1
	set b1y1 $b1y2
	set b1y2 $temp
    }
    if {$b2x1 > $b2x2} {
	set temp $b2x1
	set b2x1 $b2x2
	set b2x2 $temp
    }
    if {$b2y1 > $b2y2} {
	set temp $b2y1
	set b2y1 $b2y2
	set b2y2 $temp
    }
    # Check if the boxes intersect
    # (From: Cormen, Leiserson, and Rivests' "Algorithms", page 889)
    if {$strict} {
	return [expr {($b1x2>$b2x1) && ($b2x2>$b1x1) \
		&& ($b1y2>$b2y1) && ($b2y2>$b1y1)}]
    } else {
	return [expr {($b1x2>=$b2x1) && ($b2x2>=$b1x1) \
		&& ($b1y2>=$b2y1) && ($b2y2>=$b1y1)}]
    }
}



# ::math::geometry::bbox
#
#       Calculate the bounding box of a polyline.
#
# Arguments:
#       polyline      a polyline
#
# Results:
#       x1,y1,x2,y2   four coordinates where (x1,y1) is the upper-left corner
#                     of the bounding box, and (x2,y2) is the lower-right corner
#
# Examples:
#     - bbox {0 10 4 1 6 23 -12 5}
#       Result: -12 1 6 23
#
proc ::math::geometry::bbox {polyline} {
    set minX [lindex $polyline 0]
    set maxX $minX
    set minY [lindex $polyline 1]
    set maxY $minY
    foreach {x y} $polyline {
	if {$x < $minX} {set minX $x}
	if {$x > $maxX} {set maxX $x}
	if {$y < $minY} {set minY $y}
	if {$y > $maxY} {set maxY $y}
    }
    return [list $minX $minY $maxX $maxY]
}






# ::math::geometry::pointInsidePolygon
#
#       Determine if a point is completely inside a polygon. If the point
#       touches the polygon, then the point is not complete inside the
#       polygon.
#
# Arguments:
#       P             a point
#       polygon       a polygon
#
# Results:
#       isinside      a boolean saying whether the point is
#                     completely inside the polygon or not
#
# Examples:
#     - pointInsidePolygon {5 5} {4 4 4 6 6 6 6 4}
#       Result: 1
#     - pointInsidePolygon {5 5} {6 6 6 7 7 7}
#       Result: 0
#
proc ::math::geometry::pointInsidePolygon {P polygon} {
    # check if P is on one of the polygon's sides (if so, P is not
    # inside the polygon)
    set closedPolygon [concat $polygon [lrange $polygon 0 1]]
    foreach {x1 y1} [lrange $closedPolygon 0 end-2] {x2 y2} [lrange $closedPolygon 2 end] {
	if {[calculateDistanceToLineSegment $P [list $x1 $y1 $x2 $y2]]<0.0000001} {
	    return 0
	}
    }

    # Algorithm
    #
    # Consider a straight line going from P to a point far away from both
    # P and the polygon (in particular outside the polygon).
    #   - If the line intersects with 0 of the polygon's sides, then
    #     P must be outside the polygon.
    #   - If the line intersects with 1 of the polygon's sides, then
    #     P must be inside the polygon (since the other end of the line
    #     is outside the polygon).
    #   - If the line intersects with 2 of the polygon's sides, then
    #     the line must pass into one polygon area and out of it again,
    #     and hence P is outside the polygon.
    #   - In general: if the line intersects with the polygon's sides an odd
    #     number of times, then P is inside the polygon. Note: we also have
    #     to check whether the line crosses one of the polygon's
    #     bend points for the same reason.

    # get point far away and define the line
    set polygonBbox [bbox $polygon]
    set pointFarAway [list [expr {[lindex $polygonBbox 0]-1}] [expr {[lindex $polygonBbox 1]-1}]]
    set infinityLine [concat $pointFarAway $P]
    # calculate number of intersections
    set noOfIntersections 0
    #   1. count intersections between the line and the polygon's sides
    set closedPolygon [concat $polygon [lrange $polygon 0 1]]
    foreach {x1 y1} [lrange $closedPolygon 0 end-2] {x2 y2} [lrange $closedPolygon 2 end] {
	if {[lineSegmentsIntersect $infinityLine [list $x1 $y1 $x2 $y2]]} {
	    incr noOfIntersections
	}
    }
    #   2. count intersections between the line and the polygon's points
    foreach {x1 y1} $polygon {
	if {[calculateDistanceToLineSegment [list $x1 $y1] $infinityLine]<0.0000001} {
	    incr noOfIntersections
	}
    }
    return [expr {$noOfIntersections % 2}]
}


# ::math::geometry::rectangleInsidePolygon
#
#       Determine if a rectangle is completely inside a polygon. If polygon
#       touches the rectangle, then the rectangle is not complete inside the
#       polygon.
#
# Arguments:
#       P1            upper-left corner of the rectangle
#       P2            lower-right corner of the rectangle
#       polygon       a polygon
#
# Results:
#       isinside      a boolean saying whether the rectangle is
#                     completely inside the polygon or not
#
# Examples:
#     - rectangleInsidePolygon {0 10} {10 0} {-10 -10 0 11 11 11 11 0}
#       Result: 1
#     - rectangleInsidePolygon {0 0} {0 0} {-16 14 5 -16 -16 -25 -21 16 -19 24}
#       Result: 1
#     - rectangleInsidePolygon {0 0} {0 0} {2 2 2 4 4 4 4 2}
#       Result: 0
#
proc ::math::geometry::rectangleInsidePolygon {P1 P2 polygon} {
    # get coordinates of rectangle
    set bx1 [lindex $P1 0]
    set by1 [lindex $P1 1]
    set bx2 [lindex $P2 0]
    set by2 [lindex $P2 1]

    # if rectangle does not overlap with the bbox of polygon, then the
    # rectangle cannot be inside the polygon (this is a quick way to
    # get an answer in many cases)
    set polygonBbox [bbox $polygon]
    set polygonP1x [lindex $polygonBbox 0]
    set polygonP1y [lindex $polygonBbox 1]
    set polygonP2x [lindex $polygonBbox 2]
    set polygonP2y [lindex $polygonBbox 3]
    if {![rectanglesOverlap [list $bx1 $by1] [list $bx2 $by2] \
	    [list $polygonP1x $polygonP1y] [list $polygonP2x $polygonP2y] 0]} {
	return 0
    }

    # 1. if one of the points of the polygon is inside the rectangle,
    # then the rectangle cannot be inside the polygon
    foreach {x y} $polygon {
	if {$bx1<$x && $x<$bx2 && $by1<$y && $y<$by2} {
	    return 0
	}
    }

    # 2. if one of the line segments of the polygon intersect with the
    # rectangle, then the rectangle cannot be inside the polygon
    set rectanglePolyline [list $bx1 $by1 $bx2 $by1 $bx2 $by2 $bx1 $by2 $bx1 $by1]
    set closedPolygon [concat $polygon [lrange $polygon 0 1]]
    if {[polylinesIntersect $closedPolygon $rectanglePolyline]} {
	return 0
    }

    # at this point we know that:
    #  1. the polygon has no points inside the rectangle
    #  2. the polygon's sides don't intersect with the rectangle
    # therefore:
    #  either the rectangle is (completely) inside the polygon, or
    #  the rectangle is (completely) outside the polygon

    # final test: if one of the points on the rectangle is inside the
    # polygon, then the whole rectangle must be inside the rectangle
    return [pointInsidePolygon [list $bx1 $by1] $polygon]
}


# ::math::geometry::areaPolygon
#
#       Determine the area enclosed by a (non-complex) polygon
#
# Arguments:
#       polygon       a polygon
#
# Results:
#       area          the area enclosed by the polygon
#
# Examples:
#     - areaPolygon {-10 -10 10 -10 10 10 -10 10}
#       Result: 400
#
proc ::math::geometry::areaPolygon {polygon} {

    foreach {a1 a2 b1 b2} $polygon {break}

    set area 0.0
    foreach {c1 c2} [lrange $polygon 4 end] {
        set area [expr {$area + $b1*$c2 - $b2*$c1}]
        set b1   $c1
        set b2   $c2
    }
    expr {0.5*abs($area)}
}

package provide math::geometry 1.0.3
