# draw_geometry.tcl --
#    Draw and manipulate (plane) geometrical objects. Well, it is
#    merely an illustration of what I mean by "geometrical drawing app"
#
#    Note:
#    Not quite an example of the use of the Diagrams pacakge, but
#    it is related
#

# PixelPoint --
#    Compute pixel coordinates
# Arguments:
#    x         X-coordinate/point
#    y         Y-coordinate/point
#    type      Type of object
# Result:
#    List of pixel coordinates
#
proc PixelPoint {x {y {}} {type {}}} {
    if { $y == {} } {
        set px [lindex $x 1]
        set py [lindex $x 2]
        return [list [expr {$px*100+200}] [expr {-$py*100+200}]]
    }
    if { $type == "oval" } {
        return [list [expr {$x*100+200-2}] [expr {-$y*100+200-2}] \
                     [expr {$x*100+200+2}] [expr {-$y*100+200+2}]]
    }
}

# point --
#    Create (and draw) a point at given coordinates
# Arguments:
#    x         X-coordinate
#    y         Y-coordinate
# Result:
#    A point at the given coordinates
#
proc point {x y} {
    .c create oval [PixelPoint $x $y oval] -fill black
    return [list POINT $x $y]
}

# line --
#    Create (and draw) a line through two points
# Arguments:
#    point1    First point
#    point2    Second point
# Result:
#    A line through the two points
#
proc line {point1 point2} {
    .c create line [concat [PixelPoint $point1] [PixelPoint $point2]] -fill black
    return [list LINE $point1 $point2]
}

# circle --
#    Create (and draw) a circle at given coordinates
# Arguments:
#    point     Centre of the circle
#    rad       Radius
# Result:
#    A circle at the given centre and given radius
#
proc circle {point rad} {
    set x  [lindex $point 1]
    set y  [lindex $point 2]
    set p1 [list POINT [expr {$x+$rad}] [expr {$y+$rad}]]
    set p2 [list POINT [expr {$x-$rad}] [expr {$y-$rad}]]
    .c create oval [concat [PixelPoint $p1] [PixelPoint $p2]] -outline black
    return [list CIRCLE $point $rad]
}

# distance --
#    Compute the distance between two objects
# Arguments:
#    obj1      Point, line, ...
#    obj2      Point, line, ...
# Result:
#    Distance between the given objects (now: only points)
#
proc distance {obj1 obj2} {
    if { [lindex $obj1 0] == "POINT" } {
        set px1 [lindex $obj1 1]
        set py1 [lindex $obj1 2]
        if { [lindex $obj2 0] == "POINT" } {
            set px2 [lindex $obj2 1]
            set py2 [lindex $obj2 2]
            return [expr {hypot($px2-$px1,$py2-$py1)}]
        } else {
            error "Types unsupported"
        }
    } else {
        error "Types unsupported"
    }
}

# inprod --
#    Compute the inproduct of two vectors
# Arguments:
#    vect1     First vector
#    vect2     Second vector
# Result:
#    Inproduct
#
proc inprod {vect1 vect2} {
    set vx1 [lindex $vect1 1]
    set vy1 [lindex $vect1 2]
    set vx2 [lindex $vect2 1]
    set vy2 [lindex $vect2 2]

    return [expr {$vx1*$vx2+$vy1*$vy2}]
}

# pointonline --
#    Compute the coordinates of a point on a line
# Arguments:
#    line      Line in question
#    lambda    Parameter value
# Result:
#    Point on the line
#
proc pointonline {line lambda} {
    set v   [vectorfromline $line]

    set vx  [lindex $v 1]
    set vy  [lindex $v 2]
    set px  [lindex $line 1 1]
    set py  [lindex $line 1 2]
    set x   [expr {$px+$lambda*$vx}]
    set y   [expr {$py+$lambda*$vy}]

    return [point $x $y] ;# Make it visible
}

# vectorfromline --
#    Compute the directional vector of a line
# Arguments:
#    line      Line in question
# Result:
#    Vector in the direction of the line
#
proc vectorfromline {line} {
    set px1 [lindex $line 1 1]
    set py1 [lindex $line 1 2]
    set px2 [lindex $line 2 1]
    set py2 [lindex $line 2 2]
    set vx  [expr {$px2-$px1}]
    set vy  [expr {$py2-$py1}]

    return [list VECTOR $vx $vy]
}

# diffvector --
#    Compute the vector from one point to the next
# Arguments:
#    point1    First point
#    point2    Second point
# Result:
#    Vector
#
proc diffvector {point1 point2} {
    set px1 [lindex $point1 1]
    set py1 [lindex $point1 2]
    set px2 [lindex $point2 1]
    set py2 [lindex $point2 2]
    set vx  [expr {$px2-$px1}]
    set vy  [expr {$py2-$py1}]

    return [list VECTOR $vx $vy]
}

# normal --
#    Compute the normal vector to another vector or a line
# Arguments:
#    obj       Directed object
# Result:
#    Vector normal to the direction of the object
#
proc normal {obj} {
    if { [lindex $obj 0] == "LINE" } {
        set obj [vectorfromline $obj]
    }

    set vy  [expr {-[lindex $obj 1]}]
    set vx  [lindex $obj 2]
    set len [expr {hypot($vx,$vy)}]

    return [list VECTOR [expr {$vx/$len}] [expr {$vy/$len}]]
}

# intersect --
#    Compute the intersection between two objects
# Arguments:
#    obj1      line, circle, ...
#    obj2      line, circle, ...
# Result:
#    One point or a collection of points (now: only lines)
#
proc intersect {obj1 obj2} {
    if { [lindex $obj1 0] == "LINE" } {
        #
        # Construct the equation for the line obj1
        #
        set n1 [normal $obj1]
        set p1 [lindex $obj1 1]

        if { [lindex $obj2 0] == "LINE" } {
            #
            # Get the parametrisation of the line obj2
            #
            set v2     [vectorfromline $obj2]
            set p2     [lindex $obj2 1]
            set lambda [expr {[inprod [diffvector $p2 $p1] $n1]/ \
                              [inprod $v2 $n1]}]
            return [pointonline $obj2 $lambda]
        } else {
            error "Types unsupported"
        }
    } else {
        error "Types unsupported"
    }
}

#
# Create the standard canvas
#
pack [canvas .c -width 400 -height 300 -bg white]

#
# Simple illustration:
# Define two lines, get their intersection and draw a circle with that
# point as the centre.
#

circle [point  0 0] 1
line   [point -3 0] [point 3 0]
set p  [point [expr {cos(1)}] [expr {sin(1)}]]
line   [point  1 0] $p
line   [point -1 0] $p

.c move all 0 -50
update
.c postscript -file circle.eps
