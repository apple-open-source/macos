# draw_diagram.tcl
#    A toy derived from "PIC" by B. Kernighan to draw diagrams
#
# TODO:
#    - Update the arrow and line drawing routines
#    - Routines to:
#      - Re-initialise a page
#      - Collect the height and width of text (for objects that
#        have several text strings possibly in different fonts)
#    - Consolidate examples/tests in separate examples
#
#
if { 0 } {
'''Concise user documentation:'''

''General commands for positioning and setup:''

drawin $canvas
    Set the canvas widget in which to draw

saveps $filename
    Save the drawing in a PostScript file

direction $newdir
    Set the direction for moving the current position
    direction is one of:
        north  northeast  east southeast  south
        southwest west northwest
        N      NE         E    SE         S     SW        W    NW
        up     up-right   left down-right down  down-left left up-left
        centre center     C

currentpos $pos
    Set the current position explicitly (argument must be
    a position "object"

getpos $anchor $obj
    Get the position of a particular "anchor" point of an object
    anchor should be one of the direction strings

position $xcoord $ycoord
    Create a position "object"
    xcoord and ycoord are in pixels

''Drawing objects:''

box $text $width $height
    Draw a box from the current position
    (width and height are both optional; if not given, the text
    determines the width and height)

plaintext $text $width $height
    Draw plain text from the current position
    (width and height are both optional; if not given, the text
    determines the width and height)

circle $text $radius
    Draw a circle from the current position
    (the radius is optional)

slanted $text $width $height $angle
    Draw a slanted box from the current position
    (width, height and angle are optional)

diamond $text $width $height
    Draw a diamond-shaped box from the current position
    (width and height are both optional; if not given, the text
    determines the width and height)

drum $text $width $height
    (width and height are both optional; if not given, the text
    determines the width and height)

arrow $text $length
    Draw an arrow from the current position to the next.
    The text is drawn next to the arrow, the length (in pixels) is
    optional. If not given the gap parameters are used.

line $args
    Draw a line specified via positions or via line segments
    The arguments are either position or length-angle pairs

''Attributes:''

(Note: attributes are persistent)

attach $anchor
    Set the anchor point for attaching arrows to

color $name
    Set the color for the outline of a box etc.

fillcolor $name
    Set the color to fill the objects

textcolor $name
    Set the color to draw the text in

usegap $use
    Turn the gap on (1) or off (0). Note: usually a small gap is used
    when positioning objects.

xgap $value
    Size of the gap in horizontal direction (in pixels)

ygap $value
    Size of the gap in vertical direction (in pixels)

textfont $name
    Set the name of the font to use

linewidth $pixels
    Set the width of the lines (in line objects and arrows)

linestyle $style
    Set the style of the lines and arrows

spline $yesno
    Draw curved lines and arrows or not


'''Commands for implmenting new objects:'''

Note: it is best to study how for instance box objects are implemented
first.

pushstate
    Save the current global settings
    Used when defining an object that is composed of other objects

popstate
    Restore the previously saved global settings

computepos
    Compute the current position

boxcoords $x1 $y1 $x2 $y2
    Compute the anchor coordinates for a box-like object
    (this is merely a convenience routine. In general, you will
    have to compute the list of coordinates yourself - see
    for instance the diamond object)

moveobject $obj
    Move the object to the right position and return the new
    information

}

namespace eval ::Diagrams {
    variable box_coord_name \
        {north  northeast  east southeast  south southwest west northwest
         N      NE         E    SE         S     SW        W    NW
         up     up-right   left down-right down  down-left left up-left
         centre center     C}
    variable box_coord_id   \
        {0      1          2    3          4     5         6    7
         0      1          2    3          4     5         6    7
         0      1          2    3          4     5         6    7
         8      8          8}

    variable state
    variable state_saved
    variable anchors
    variable dirinfo
    variable torad [expr {3.1415926/180.0}]

    namespace export currentpos getpos direction \
                     arrow box circle diamond drum line slanted \
                     pushstate popstate \
                     drawin saveps line position plaintext linestyle

    array set state {
        attach         "northwest"
        canvas         ""
        colour         "black"
        default_dir    "east"
        dir            "init"
        textfont       "Helvetica 12"
        justify        center
        default_width  "fitting"
        default_height 20
        xdir           1
        ydir           0
        xshift         0
        yshift         0
        xcurr          10
        ycurr          10
        xgap           10
        ygap           10
        scale          {1.0}
        xprev          10
        yprev          10
        lastitem       {}
        usegap         0
        spline         0
        color          "black"
        fillcolor      {}
        textcolor      "black"
        linewidth      1
        linestyle      {}
    }

    # Name of direction, xdir, ydir, default attachment, anchor for text near arrow
    set dirinfo(south)      {south  0  1 north e}
    set dirinfo(north)      {north  0 -1 south w}
    set dirinfo(west)       {west  -1  0 east n}
    set dirinfo(east)       {east   1  0 west s}
    set dirinfo(southwest)  {southwest  -1  1 north ne}
    set dirinfo(northwest)  {northwest  -1 -1 south se}
    set dirinfo(southeast)  {southeast   1  1 north nw}
    set dirinfo(northeast)  {northeast   1 -1 south sw}
    set dirinfo(down)       $dirinfo(south)
    set dirinfo(up)         $dirinfo(north)
    set dirinfo(left)       $dirinfo(west)
    set dirinfo(right)      $dirinfo(east)
    set dirinfo(SE)         $dirinfo(southeast)
    set dirinfo(NE)         $dirinfo(northeast)
    set dirinfo(SW)         $dirinfo(southwest)
    set dirinfo(NW)         $dirinfo(northwest)
}

# attach, fillcolor, ... --
#    Procedures to set simple attributes
# Arguments:
#    args     New value for the attribute or empty to get the current value
# Result:
#    Current or new value, depending on whether a new value was given
#
foreach p {attach color fillcolor textcolor usegap xgap ygap textfont
           linewidth spline} {
    eval [string map [list PP $p] \
        {proc ::Diagrams::PP {args} {
             variable state
             if { [llength $args] == 1 } {
                 set state(PP) [lindex $args 0]
             }
             return $state(PP)
         }
         namespace eval ::Diagrams {namespace export PP}
        }]
}

# linestyle --
#    Set the line style for all objects
# Arguments:
#    style     New style
# Result:
#    None
#
proc ::Diagrams::linestyle {style} {
    variable state

    switch -- $style {
        "solid"        { set pattern "" }
        "dot"          { set pattern . }
        "dash"         { set pattern - }
        "dash-dot"     { set pattern -. }
        "dash-dot-dot" { set pattern -.. }
        default        { set pattern $style }
    }

    set state(linestyle) $pattern
}

# drawin --
#    Set the canvas widget in which to draw
# Arguments:
#    widget    Name of the canvas widget to use
# Result:
#    None
#
proc ::Diagrams::drawin {widget} {
    variable state
    set state(canvas) $widget
}

# saveps --
#    Save the drawing in a PostScript file
# Arguments:
#    filename   Name of the file to write
# Result:
#    None
#
proc ::Diagrams::saveps {filename} {
    variable state
    update
    $state(canvas) postscript -file $filename
}

# direction --
#    Set the direction for moving the current position
# Arguments:
#    newdir    Direction (down, left, up, right)
# Result:
#    None
#
proc ::Diagrams::direction {newdir} {
    variable state
    variable dirinfo

    if { [info exists dirinfo($newdir)] } {
        foreach s {dir xdir ydir attach anchor} v $dirinfo($newdir) {
            set state($s) $v
        }
    } else {
        return
    }
    #
    # TODO: problem with arrows/lines
    #
    if { $state(lastitem) != {} && [lindex $state(lastitem) 0] == "BOX" } {
        currentpos [getpos $state(dir) $state(lastitem)]
    }
}

# horizontal --
#    Compute the length of a line segment whose horizontal
#    extension is given
# Arguments:
#    length    Length over which the line extends in horizontal direction
# Result:
#    None
#
proc ::Diagrams::horizontal {length} {
    variable state
    variable dirinfo

    #
    # Does the direction allow for a horizontal component?
    #
    TODO!
    if { $state(dir) == "NW" $newdir) } {
        foreach s {dir xdir ydir attach} v $dirinfo($newdir) {
            set state($s) $v
        }
    } else {
        return
    }
    #
    # TODO: problem with arrows/lines
    #
    if { $state(lastitem) != {} && [lindex $state(lastitem) 0] == "BOX" } {
        currentpos [getpos $state(dir) $state(lastitem)]
    }
}

# pushstate
#    Save the current global settings
# Arguments:
#    None
# Result:
#    None
# Side effect:
#    Pushes the global settings up a stack for later reuse
#
proc ::Diagrams::pushstate {} {
    variable state
    variable state_saved

    lappend state_saved [array get state]
}

# popstate
#    Restore the previously saved global settings
# Arguments:
#    None
# Result:
#    None
# Side effect:
#    Restores the previous settings and pops the stack
#
proc ::Diagrams::popstate {} {
    variable state
    variable state_saved

    if { [llength $state_saved] > 0 } {
        set old_state [lindex $state_saved end]
        set state_saved [lrange $state_saved 0 end-1]
        array set state $old_state
    }
}

# currentpos
#    Set the current position explicitly
# Arguments:
#    pos       Position "object" (optional)
# Result:
#    Current position as an "object"
# Side effect:
#    Current position set
#
proc ::Diagrams::currentpos { {pos {}} } {
    variable state

    if { [lindex $pos 0] == "POSITION" } {
        set state(xprev) $state(xcurr)
        set state(yprev) $state(ycurr)
        set state(xcurr) [lindex $pos 1]
        set state(ycurr) [lindex $pos 2]
    }

    return [list POSITION $state(xcurr) $state(ycurr)]
}

# getpos
#    Get the position of a particular "anchor" point of an object
# Arguments:
#    anchor    Which point to return
#    obj       Drawable "object"
# Result:
#    Position of the requested point
#
proc ::Diagrams::getpos {anchor obj} {
    variable state
    variable box_coord_name
    variable box_coord_id

    if { $anchor == "init" } {
        direction "east"
        set anchor "east"
    }

    if { [lindex $obj 0] == "BOX" } {
        set idx [lsearch $box_coord_name $anchor]
        if { $idx < 0 } {
            return -code error "Unknown anchor - $anchor"
        }
        set idx [lindex $box_coord_id $idx]
    } else {
        set idx $anchor
    }

    set xp [lindex [lindex $obj 2] [expr {2*$idx}]]
    set yp [lindex [lindex $obj 2] [expr {2*$idx+1}]]

    return [list POSITION $xp $yp]
}

# computepos
#    Compute the new position
# Arguments:
#    None
# Result:
#    X- and Y-coordinates
#
proc ::Diagrams::computepos {} {
    variable state

    set xcoord [expr {$state(xcurr)+$state(xgap)*$state(xdir)*$state(usegap)}]
    set ycoord [expr {$state(ycurr)+$state(ygap)*$state(ydir)*$state(usegap)}]

    return [list "POSITION" $xcoord $ycoord]
}

# position
#    Create a position "object"
# Arguments:
#    xcoord    X-coordinate
#    ycoord    Y-coordinate
# Result:
#    List representing the object
#
proc ::Diagrams::position {xcoord ycoord} {

    return [list "POSITION" $xcoord $ycoord]
}

# boxcoords --
#    Compute the anchor coordinates for a box-like object
# Arguments:
#    x1        X-coordinate top-left (order is important!)
#    y1        Y-coordinate top-left
#    x2        X-coordinate bottom-right
#    y2        Y-coordinate bottom-right
# Result:
#    List of coordinates in the right order
# Note:
#    The coordinates typically come from a [canvas bbox] command
#
proc ::Diagrams::boxcoords {x1 y1 x2 y2} {
    set coords {}
    set xns    [expr {($x1+$x2)/2.0}]
    set yew    [expr {($y1+$y2)/2.0}]

    return [list $xns $y1 $x2 $y1 $x2 $yew $x2 $y2 \
                 $xns $y2 $x1 $y2 $x1 $yew $x1 $y1 $xns $yew]
}

# moveobject --
#    Move the object to the right position and return the new
#    information
# Arguments:
#    obj       Object at the default position
# Result:
#    Updated list with new object coordinates
#
proc ::Diagrams::moveobject {obj} {
    variable state

    #
    # Compute the coordinates of the object (positioned correctly)
    #
    foreach {dummy xcurr ycurr}     [computepos] {break}
    foreach {dummy xanchor yanchor} [getpos $state(attach) $obj] {break}

    set xt [expr {$xcurr-$xanchor}]
    set yt [expr {$ycurr-$yanchor}]

    set newobj [lrange $obj 0 1]
    set items  [lindex $obj 1]

    foreach i $items {
        $state(canvas) move $i $xt $yt
    }
    set newcrd {}
    foreach {x y} [lindex $obj 2] {
        set xn [expr {$x+$xt}]
        set yn [expr {$y+$yt}]
        lappend newcrd $xn $yn
    }
    lappend newobj $newcrd

    currentpos [getpos $state(dir) $newobj]

    set state(lastitem) $newobj
    return $newobj
}

# box --
#    Draw a box from the current position
# Arguments:
#    text      Text to be fitted in the box
#    width     (Optional) width in pixels or "fitting"
#    height    (Optional) height in pixels
# Result:
#    ID of the box
# Side effect:
#    Box drawn with text inside, current position set
#
proc ::Diagrams::box {text {width {}} {height {}}} {
    variable state

    #
    # Before we create the text object, we need to store the
    # current position ...
    #
    pushstate
    set textobj [plaintext $text $width $height]

    foreach {dummy x1 y1} [getpos NW $textobj] {break}
    foreach {dummy x2 y2} [getpos SE $textobj] {break}

    set x1 [expr {$x1-5}]
    set y1 [expr {$y1-5}]
    set x2 [expr {$x2+5}]
    set y2 [expr {$y2+5}]

    #
    # Construct the box
    #
    set     items [lindex $textobj 1]
    lappend items [$state(canvas) create rectangle $x1 $y1 $x2 $y2 \
                       -fill    $state(fillcolor) \
                       -outline $state(color)     \
                       -width   $state(linewidth) \
                       -dash    $state(linestyle) ]
    $state(canvas) raise [lindex $items 0]

    #
    # Move the combined object to the original "current" position
    #
    popstate
    set obj [moveobject [list BOX $items [boxcoords $x1 $y1 $x2 $y2]]]
    set state(usegap)   1
    return $obj
}

# plaintext --
#    Draw plain text from the current position
# Arguments:
#    text      Text to be fitted in the box
#    width     (Optional) width in pixels or "fitting"
#    height    (Optional) height in pixels
# Result:
#    ID of the box
# Side effect:
#    Text drawn, current position set
#
proc ::Diagrams::plaintext {text {width {}} {height {}}} {
    variable state

    if { $width == {} } {
        set width $state(default_width)
    }

    if { $height == {} } {
        set height $state(default_height)
    }

    set items [$state(canvas) create text 0 0 -text $text \
                  -font         $state(textfont) \
                  -fill         $state(textcolor) \
                  -justify      $state(justify)]


    if { $width == "fitting" } {
        foreach {x1 y1 x2 y2} [$state(canvas) bbox $items] {break}
    } else {
        set x1 [expr {-$width/2}]
        set x2 [expr {$width/2}]
        set y1 [expr {-$height/2}]
        set y2 [expr {$height/2}]
       # set width  [expr {$x2-$x1}]
       # set height [expr {$y2-$y1}]
    }

    #
    # Construct the coordinates and the object
    #
    set coords [boxcoords $x1 $y1 $x2 $y2]
    set obj    [list BOX $items $coords]

    #
    # Move the object to the right position
    #
    set obj    [moveobject $obj]
    set state(usegap)   1
    return $obj
}

# circle --
#    Draw a circle from the current position
# Arguments:
#    text      Text to be fitted in the circle
#    radius    (Optional) radius in pixels or "fitting"
# Result:
#    ID of the circle
# Side effect:
#    Circle drawn with text inside, current position set
#
proc ::Diagrams::circle {text {radius {}} } {
    variable state
    variable torad

    #
    # Before we create the text object, we need to store the
    # current state ...
    #
    pushstate
    set textobj [plaintext $text $radius $radius]

    foreach {dummy x1 y1} [getpos NW $textobj] {break}
    foreach {dummy x2 y2} [getpos SE $textobj] {break}

    set xc [expr {($x1+$x2)/2.0}]
    set yc [expr {($y1+$y2)/2.0}]
    if { $radius == {} } {
       set radius [expr {hypot($x1-$xc,$y1-$yc)}]
    }
    set x1 [expr {$xc-$radius-5}]
    set x2 [expr {$xc+$radius+5}]
    set y1 [expr {$yc-$radius-5}]
    set y2 [expr {$yc+$radius+5}]

    #
    # Construct the circle
    #
    set     items [lindex $textobj 1]
    lappend items [$state(canvas) create oval $x1 $y1 $x2 $y2 \
                       -fill    $state(fillcolor) \
                       -outline $state(color)     \
                       -width   $state(linewidth) \
                       -dash    $state(linestyle) ]
    $state(canvas) raise [lindex $items 0]

    #
    # Move the combined object to the original "current" position
    #
    popstate

    #
    # Construct the list of coordinates
    #
    set coords {}
    set radius [expr {$radius+5}]
    foreach angle {90 45 0 -45 -90 -135 180 135} {
        set x [expr {$xc+$radius*cos($angle*$torad)}]
        set y [expr {$yc-$radius*sin($angle*$torad)}]
        lappend coords $x $y
    }
    lappend coords $xc $yc
    set obj [moveobject [list BOX $items $coords]]
    set state(usegap)   1
    return $obj
}

# slanted --
#    Draw a slanted box from the current position
# Arguments:
#    text      Text to be fitted in the box
#    width     (Optional) width in pixels or "fitting"
#    height    (Optional) height in pixels
#    angle     (Optional) angle of the slant (90 degrees gives a rectangle)
# Result:
#    ID of the slanted box
# Side effect:
#    Slanted box drawn with text inside, current position set
#
proc ::Diagrams::slanted {text {width {}} {height {}} {angle 70} } {
    variable state
    variable torad

    #
    # Before we create the text object, we need to store the
    # current state ...
    #
    pushstate

    #
    # Compute the available width
    #
    set cosa [expr {cos($angle*3.1415926/180.0)}]
    set sina [expr {sin($angle*3.1415926/180.0)}]

    set twidth  $width
    set theight $height
    if { $width != {} && $width != "fitting" } {
        set twidth  [expr {($width-10)-$cosa*($height-10)}]
        set theight [expr {$height-10}]
    }

    set textobj [plaintext $text $twidth $theight]

    foreach {dummy x1 y1} [getpos NW $textobj] {break}
    foreach {dummy x2 y2} [getpos SE $textobj] {break}

    #
    # Construct the coordinates
    #
    set bwidth  [expr {10+$x2-$x1}]
    set bheight [expr {10+$y2-$y1}]
    set width   [expr {$bwidth+$cosa*$bheight}]
    set height  $bheight
    set xc      [expr {($x1+$x2)/2.0}]
    set yc      [expr {($y1+$y2)/2.0}]

    set xnw     [expr {$xc-$bwidth/2.0}]
    set ynw     [expr {$yc-$bheight/2.0}]
    set xne     [expr {$xc+$bwidth/2.0+$cosa*$bheight}]
    set yne     $ynw
    set xn      [expr {($xnw+$xne)/2.0}]
    set yn      [expr {($ynw+$yne)/2.0}]

    set xse     [expr {$xc+$bwidth/2.0}]
    set yse     [expr {$yc+$height/2.0}]
    set xe      [expr {($xne+$xse)/2.0}]
    set ye      [expr {($yne+$yse)/2.0}]

    set xsw     [expr {$xc-$bwidth/2.0-$cosa*$bheight}]
    set ysw     $yse
    set xs      [expr {($xse+$xsw)/2.0}]
    set ys      [expr {($yse+$ysw)/2.0}]
    set xw      [expr {($xnw+$xsw)/2.0}]
    set yw      [expr {($ynw+$ysw)/2.0}]

    set coords  [list $xn $yn $xne $yne $xe $ye $xse $yse $xs $ys \
                      $xsw $ysw $xw $yw $xnw $ynw $xc $yc]

    #
    # Create the object
    #
    set     items [lindex $textobj 1]
    lappend items [$state(canvas) create polygon  \
                       $xnw $ynw $xne $yne $xse $yse $xsw $ysw $xnw $ynw \
                       -fill    $state(fillcolor) \
                       -outline $state(color)     \
                       -width   $state(linewidth) \
                       -dash    $state(linestyle) ]
    $state(canvas) raise [lindex $items 0]

    #
    # Move the combined object to the original "current" position
    #
    popstate
    set obj [moveobject [list BOX $items $coords]]
    set state(usegap)   1
    return $obj
}

# diamond --
#    Draw a diamond-shaped box from the current position
# Arguments:
#    text      Text to be fitted in the diamond
#    width     (Optional) width in pixels or "fitting"
#    height    (Optional) height in pixels
# Result:
#    ID of the diamond
# Side effect:
#    Diamond-shaped box drawn with text inside, current position set
# Note:
#    The aspect ratio of the diamond in case of fitting the text
#    is set to width:heihgt = 2:1
#
proc ::Diagrams::diamond {text {width {}} {height {}} } {
    variable state

    #
    # Before we create the text object, we need to store the
    # current position ...
    #
    pushstate
    set textobj [plaintext $text $width $height]

    foreach {dummy x1 y1} [getpos NW $textobj] {break}
    foreach {dummy x2 y2} [getpos SE $textobj] {break}

    set alpha 2.0

    if { $width == {} || $width == "fitting" } {
        set width  [expr {$x2-$x1+4+($y2-$y1+4)*$alpha}]
        set height [expr {$width/$alpha}]
    }

    set xc  [expr {($x1+$x2)/2.0}]
    set yc  [expr {($y1+$y2)/2.0}]

    set xn  $xc
    set yn  [expr {$yc-$height/2.0}]

    set xs  $xc
    set ys  [expr {$yc+$height/2.0}]

    set xe  [expr {$xc+$width/2.0}]
    set ye  $yc

    set xw  [expr {$xc-$width/2.0}]
    set yw  $yc

    set xnw [expr {($xn+$xw)/2.0}]
    set ynw [expr {($yn+$yw)/2.0}]
    set xsw [expr {($xs+$xw)/2.0}]
    set ysw [expr {($ys+$yw)/2.0}]

    set xne [expr {($xn+$xe)/2.0}]
    set yne [expr {($yn+$ye)/2.0}]
    set xse [expr {($xs+$xe)/2.0}]
    set yse [expr {($ys+$ye)/2.0}]

    set coords  [list $xn $yn $xne $yne $xe $ye $xse $yse $xs $ys \
                      $xsw $ysw $xw $yw $xnw $ynw $xc $yc]

    #
    # Construct the diamond
    #
    set     items [lindex $textobj 1]
    lappend items [$state(canvas) create polygon \
                       $xn $yn $xe $ye $xs $ys $xw $yw \
                       -fill    $state(fillcolor) \
                       -outline $state(color)     \
                       -width   $state(linewidth) \
                       -dash    $state(linestyle) ]
    $state(canvas) raise [lindex $items 0]

    #
    # Move the combined object to the original "current" position
    #
    popstate
    set obj [moveobject [list BOX $items $coords]]
    set state(usegap)   1
    return $obj
}

# drum --
#    Draw a drum-shape from the current position
# Arguments:
#    text      Text to be fitted in the drum
#    width     (Optional) width in pixels or "fitting"
#    height    (Optional) height in pixels
# Result:
#    ID of the drum
# Side effect:
#    Drum-shape box drawn with text inside, current position set
#
proc ::Diagrams::drum {text {width {}} {height {}} } {
    variable state

    #
    # Before we create the text object, we need to store the
    # current position ...
    #
    pushstate
    set textobj [plaintext $text $width $height]

    foreach {dummy x1 y1} [getpos NW $textobj] {break}
    foreach {dummy x2 y2} [getpos SE $textobj] {break}

    set aspect 0.35

    if { $width == {} || $width == "fitting" } {
        set width  [expr {$x2-$x1+10}]
        set height [expr {$y2-$y1+10+$aspect*$width}]
    }

    set xc  [expr {($x1+$x2)/2.0}]
    set yc  [expr {($y1+$y2)/2.0}]

    set hellips [expr {$height*$aspect}]

    set xtop1   [expr {$xc-$width/2}]
    set xtop2   [expr {$xc+$width/2}]
    set ytop1   [expr {$yc-$height/2+$hellips/2}]
    set ytop2   [expr {$yc-$height/2-$hellips/2}]

    set xline1  $xtop1
    set xline2  $xtop2
    set yline1  [expr {$yc-$height/2}]
    set yline2  [expr {$yc+$height/2}]

    set xbot1   $xtop1
    set xbot2   $xtop2
    set ybot1   [expr {$yc+$height/2+$hellips/2}]
    set ybot2   [expr {$yc+$height/2-$hellips/2}]

    set coords  [list $xc     $ytop2  $xline2 $yline1 $xline2 $yc     \
                      $xline2 $yline2 $xc     $ybot2  $xline1 $yline2 \
                      $xline1 $yc     $xline1 $yline2 $xc     $yc     ]

    #
    # Construct the drum
    # (We need quite a few pieces here ...)
    #
    set     items [lindex $textobj 1]
    lappend items \
        [$state(canvas) create rectangle $xline1 $yline1 $xline2 $yline2 \
             -fill $state(fillcolor) -outline {}] \
        [$state(canvas) create line $xline1 $yline1 $xline1 $yline2 \
             -fill $state(color)] \
        [$state(canvas) create line $xline2 $yline1 $xline2 $yline2 \
             -fill $state(color)] \
        [$state(canvas) create oval $xtop1  $ytop1  $xtop2  $ytop2 \
             -fill  $state(fillcolor) -outline $state(color) \
             -width $state(linewidth) -dash    $state(linestyle) ] \
        [$state(canvas) create arc  $xbot1  $ybot1  $xbot2  $ybot2 \
             -fill $state(fillcolor) -outline {} \
             -dash    $state(linestyle) \
             -start 179 -extent 182 -style chord] \
        [$state(canvas) create arc  $xbot1  $ybot1  $xbot2  $ybot2 \
             -fill  $state(fillcolor) -outline $state(color) \
             -width $state(linewidth) -dash    $state(linestyle) \
             -start 179 -extent 182 -style arc]
    $state(canvas) raise [lindex $items 0]

    #
    # Move the combined object to the original "current" position
    #
    popstate
    set obj [moveobject [list BOX $items $coords]]
    set state(usegap)   1
    return $obj
}

# arrow --
#    Draw an arrow from the current position to the next
# Arguments:
#    text      (Optional) text to written above the arrow
#    length    (Optional) length in pixels
#    heads     (Optional) which arrow heads (defaults to end)
# Result:
#    ID of the arrow
# Side effect:
#    Arrow drawn
#
proc ::Diagrams::arrow { {text {}} {length {}} {heads last}} {
    variable state

    if { $length != {} } {
        set factor  [expr {hypot($state(xdir),$state(ydir))}]
        set dxarrow [expr {$length*$state(xdir)/$factor}]
        set dyarrow [expr {$length*$state(ydir)/$factor}]
    } else {
        set dxarrow [expr {$state(xdir)*$state(xgap)}]
        set dyarrow [expr {$state(ydir)*$state(ygap)}]
    }

    set x1      $state(xcurr)
    set y1      $state(ycurr)
    set x2      [expr {$state(xcurr)+$dxarrow}]
    set y2      [expr {$state(ycurr)+$dyarrow}]

    set item [$state(canvas) create line $x1 $y1 $x2 $y2 \
                 -fill    $state(colour)    \
                 -smooth  $state(spline)    \
                 -arrow   $heads            \
                 -width   $state(linewidth) \
                 -dash    $state(linestyle) ]

    set xt [expr {($x1+$x2)/2}]
    set yt [expr {($y1+$y2)/2}]

    set item [$state(canvas) create text $xt $yt -text $text \
                 -font    $state(textfont) \
                 -anchor  $state(anchor)   \
                 -justify $state(justify)]

    set item [list ARROW $item [list $x1 $y1 $x2 $y2]]

    #
    # Ignore the direction of motion - we need the end point
    #
    currentpos [position $x2 $y2]

    set state(lastitem) $item
    set state(usegap)   0
    return $item
}

# line --
#    Draw a line specified via positions or via line segments
# Arguments:
#    args        All arguments (either position or length-angle pairs)
# Result:
#    ID of the line
# Side effect:
#    Line drawn
#
proc ::Diagrams::line {args} {
    variable state
    variable torad

    #
    # Get the current position if the first arguments
    # are line segments (this guarantees that x, y are
    # defined)
    #
    if { [lindex [lindex $args 0] 0] != "POSITION" } {
        set args [linsert $args 0 [currentpos]]
    }

    set xycoords {}
    set x1       {}
    set x2       {}
    set y1       {}
    set y2       {}

    set idx 0
    set number [llength $args]
    while { $idx < $number } {
        set arg [lindex $args $idx]

        if { [lindex $arg 0] != "POSITION" } {
            incr idx
            set length $arg
            set angle  [lindex $args $idx]

            set x      [expr {$x+$length*cos($torad*$angle)}]
            set y      [expr {$y-$length*sin($torad*$angle)}]
        } else {
            foreach {dummy x y} [lindex $args $idx] {break}
        }

        lappend xycoords $x $y

        if { $x1 == {} || $x1 > $x } { set x1 $x }
        if { $x2 == {} || $x2 < $x } { set x2 $x }
        if { $y1 == {} || $y1 > $y } { set y1 $y }
        if { $y2 == {} || $y2 < $y } { set y2 $y }

        incr idx
    }

    set item [$state(canvas) create line $xycoords \
                 -smooth  $state(spline)    \
                 -fill    $state(colour)    \
                 -width   $state(linewidth) \
                 -dash    $state(linestyle) ]

    set item [list LINE $item [list $x1 $y1 $x2 $y2]]

    currentpos [getpos 1 $item] ;# Absolute index, rather than particular direction

    set state(lastitem) $item
    set state(usegap)   0
    return $item
}

# bracket --
#    Draw three line segments in the form of a square bracket from
#    one position to the next
# Arguments:
#    dir         Direction of the bracket (east, west, north or south)
#    dist        Distance of the
# Result:
#    ID of the "bracket"
# Side effect:
#    Three line segments drawn
#
proc ::Diagrams::bracket {dir dist begin end} {
    variable state

    set coords [lrange $begin 1 2]
    if { $dir == "west" } {
       lappend coords [expr {[lindex $begin 1]-$dist}] [lindex $begin 2]
       lappend coords [expr {[lindex $begin 1]-$dist}] [lindex $end 2]
    }
    if { $dir == "east" } {
       lappend coords [expr {[lindex $begin 1]+$dist}] [lindex $begin 2]
       lappend coords [expr {[lindex $begin 1]+$dist}] [lindex $end 2]
    }
    if { $dir == "south" } {
       lappend coords [lindex $begin 1] [expr {[lindex $begin 2]+$dist}]
       lappend coords [lindex $end 1]   [expr {[lindex $begin 2]+$dist}]
    }
    if { $dir == "north" } {
       lappend coords [lindex $begin 1] [expr {[lindex $begin 2]-$dist}]
       lappend coords [lindex $end 1]   [expr {[lindex $begin 2]-$dist}]
    }
    lappend coords [lindex $end 1] [lindex $end 2]

    $state(canvas) create line $coords -arrow last \
                       -width   $state(linewidth) \
                       -dash    $state(linestyle) ]

    set item [list ARROW $item [join [lrange $coords 0 1] [lrange $coords end-1 end]]]

    #
    # Ignore the direction of motion - we need the end point
    #
    currentpos [position [lindex $coords end-1] [lindex $coords end]]

    set state(lastitem) $item
    set state(usegap)   0
    return $item
}

# Announce our presence
#
package provide Diagrams 0.2

#
# A small demonstration ...
#
if { 0 } {
pack [canvas .c -width 500 -height 500 -bg white]

namespace import ::Diagrams::*

console show
drawin .c
linestyle dot

textcolor green
set C [circle "Hi there!" 20]
arrow "XX" 40 none
direction south
arrow "YY" 40
set B [box Aha]

puts "Pos: [getpos S $C]"
line [getpos S $B] 100 270
line [getpos S $B] 100 0

}
if { 0 } {
pack [canvas .c -width 500 -height 500 -bg white]

namespace import ::Diagrams::*

console show
drawin .c
#linestyle dot
#linewidth 3

textcolor green
box "There is\nstill a lot to\ndo!"
arrow "" 230
textcolor blue
box "But it looks nice"
direction south
textcolor black
color magenta; circle "Or does it?" ;color black
direction southwest
arrow "" 100
set B1 [box "Yes, it sure does!"]

fillcolor red
foreach {text dir} {A southwest B south C southeast} {
    direction $dir
    currentpos [getpos $dir $B1]
    arrow "" 100
    box $text
}
slanted "Hm, this\nis okay ..."
direction south
arrow ""
diamond "Okay\n?"
direction west
arrow "" 50
drum "Yes!"

fillcolor green
currentpos [position 70 200]
slanted Hm
direction north ; arrow N 30
#
# This does not work cleanly: lastitem = arrow :(
#
direction south
arrow S 30

#line 20 45 20 90 20 135 30 10

}
#
# Experiments with mathematical formulae
#
proc ::Diagrams::segm {args} {
    variable state
    #
    # TODO: the horizontal centre!!
    #
    set pos [currentpos]
    set items {}
    foreach text $args {
        usegap 0
        if { [lindex [split $text] 0] != "BOX" } {
            puts "text: $text"
            set text [plaintext $text]
        } else {
            puts "original object: $text"
            set text [moveobject $text]
        }
        puts "object: $text"
        set items [concat $items [lindex $text 1]]
        direction east
    }

    foreach {x1 y1 x2 y2} [eval $state(canvas) bbox $items] {break}
    set xc     [expr {($x1+$x2)/2}]
    set yc     [expr {($y1+$y2)/2}]
    set coords [list $xc $y1 $x2 $y1 $x2 $yc $x2 $y2 $xc $y2 \
                     $x1 $y2 $x1 $yc $x1 $y1 $xc $yc]
    set obj [list BOX $items $coords]
    usegap 0
    puts "result: $obj"
    currentpos $pos
    return $obj
}
proc ::Diagrams::div {numerator denominator} {
    variable state

    usegap 0
    set pos [currentpos]
    direction north
    set num [segm $numerator]
    currentpos $pos
    direction south
    usegap 0
    set den [segm $denominator]
    currentpos $pos

    foreach {dummy xn1 yn1} [getpos NW $num] {break}
    foreach {dummy xn2 yn2} [getpos SE $num] {break}
    foreach {dummy xd1 yd1} [getpos NW $den] {break}
    foreach {dummy xd2 yd2} [getpos SE $den] {break}

    set twidth [expr {$xn2-$xn1}]
    if { ($xn2-$xn1) < ($xd2-$xd1) } {
        set twidth [expr {$xd2-$xd1}]
    }
    set x1   [expr {[lindex $pos 1]-$twidth/2}]
    set x2   [expr {[lindex $pos 1]+$twidth/2}]
    set y    [lindex $pos 2]
    set item [$state(canvas) create line $x1 $y $x2 $y \
                 -fill    $state(colour)]

    puts "line: $x1 $y $x2 $y"

    set items [concat [lindex $num 1] [lindex $den 1] $item]

    set xc [expr {$x1+$x2}]
    set coords [list $xc  $yn1   $x2  $yn1   $x2  $y   \
                     $x2  $yd2   $xc  $yd2   $x1  $yd2 \
                     $x1  $y     $x1  $yn1   $xc  $y ]

   #set obj [moveobject [list BOX $items $coords]]
    set obj [list BOX $items $coords]
    usegap 1
    return $obj
}
if { 0 } {
currentpos [position 100 100]
set nn [::Diagrams::div A B]
puts "nn: $nn"
set bb [::Diagrams::segm B+C+ $nn]
currentpos [position 100 100]
puts [::Diagrams::div A $bb]


#
# Experiments with chemical structure formulae
# (Awaits update of "line")
#
if { 0 } {
proc ring {} {
   set side 20
   line $side 60 $side 0 $side -60 $side -120 $side 180 $side 120
}

proc bond {} {
   set side 20
   line $side 0
}

#
# Very primitive chemical formula
# -- order of direction/currentpos important!
#
direction east
currentpos [position 100 400]
ring; bond; ring; bond; plaintext CH3
}

saveps arjen.eps
}
