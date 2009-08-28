# plotbusiness.tcl --
#    Facilities aimed at business type charts
#
# Note:
#    This source file contains the private functions for various
#    business type charts.
#    It is the companion of "plotchart.tcl"
#

# Config3DBar --
#    Configuration options for the 3D barchart
# Arguments:
#    w           Name of the canvas
#    args        List of arguments
# Result:
#    None
# Side effects:
#    Items that are already visible will be changed to the new look
#
proc ::Plotchart::Config3DBar { w args } {
    variable settings

    foreach {option value} $args {
        set option [string range $option 1 end]
        set settings($w,$option) $value

        switch -- $option {
            "usebackground" {
                if { $value } {
                    $w itemconfigure background -fill grey65 -outline black
                } else {
                    $w itemconfigure background -fill {} -outline {}
                }
            }
            "useticklines" {
                if { $value } {
                    $w itemconfigure ticklines -fill black
                } else {
                    $w itemconfigure ticklines -fill {}
                }
            }
            "showvalues" {
                if { $value } {
                    $w itemconfigure values -fill $settings($w,valuecolour)
                } else {
                    $w itemconfigure values -fill {}
                }
            }
            "valuecolour" - "valuecolor" {
                set settings($w,valuecolour) $value
                set settings($w,valuecolor)  $value
                $w itemconfigure values -fill $settings($w,valuecolour)
            }
            "valuefont" {
                set settings($w,valuefont) $value
                $w itemconfigure labels -font $settings($w,valuefont)
            }
            "labelcolour" - "labelcolor" {
                set settings($w,labelcolour) $value
                set settings($w,labelcolor)  $value
                $w itemconfigure labels -fill $settings($w,labelcolour)
            }
            "labelfont" {
                set settings($w,labelfont) $value
                $w itemconfigure labels -font $settings($w,labelfont)
            }
        }
    }
}

# Draw3DBarchart --
#    Draw the basic elements of the 3D barchart
# Arguments:
#    w           Name of the canvas
#    yscale      Minimum, maximum and step for the y-axis
#    nobars      Number of bars
# Result:
#    None
# Side effects:
#    Default settings are introduced
#
proc ::Plotchart::Draw3DBarchart { w yscale nobars } {
    variable settings
    variable scaling

    #
    # Default settings
    #
    set settings($w,labelfont)     "fixed"
    set settings($w,valuefont)     "fixed"
    set settings($w,labelcolour)   "black"
    set settings($w,valuecolour)   "black"
    set settings($w,usebackground) 0
    set settings($w,useticklines)  0
    set settings($w,showvalues)    1

    #
    # Horizontal positioning parameters
    #
    set scaling($w,xbase)    0.0
    set scaling($w,xshift)   0.2
    set scaling($w,barwidth) 0.6

    #
    # Shift the vertical axis a bit
    #
    $w move yaxis -10 0
    #
    # Draw the platform and the walls
    #
    set x1 $scaling($w,pxmin)
    set x2 $scaling($w,pxmax)
    foreach {dummy y1} [coordsToPixel $w $scaling($w,xmin) 0.0] {break}

    set x1 [expr {$x1-10}]
    set x2 [expr {$x2+10}]
    set y1 [expr {$y1+10}]

    set y2 [expr {$y1-30}]
    set x3 [expr {$x1+30}]
    set y3 [expr {$y1-30}]
    set x4 [expr {$x2-30}]
    set y4 $y1

    $w create polygon $x1 $y1 $x3 $y3 $x2 $y2 $x4 $y4 -fill gray65 -tag platform \
         -outline black

    set xw1 $x1
    foreach {dummy yw1} [coordsToPixel $w 0.0 $scaling($w,ymin)] {break}
    set xw2 $x1
    foreach {dummy yw2} [coordsToPixel $w 0.0 $scaling($w,ymax)] {break}

    set xw3 $x3
    set yw3 [expr {$yw2-30}]
    set xw4 $x3
    set yw4 [expr {$yw1-30}]

    $w create polygon $xw1 $yw1 $xw2 $yw2 $xw3 $yw3 $xw4 $yw4 \
        -outline black -fill gray65 -tag background

    set xw5 $x2
    $w create polygon $xw3 $yw3 $xw5 $yw3 $xw5 $yw4 $xw3 $yw4 \
        -outline black -fill gray65 -tag background

    #
    # Draw the ticlines (NOTE: Something is wrong here!)
    #
#   foreach {ymin ymax ystep} $yscale {break}
#   if { $ymin > $ymax } {
#       foreach {ymax ymin ystep} $yscale {break}
#       set ystep [expr {abs($ystep)}]
#   }
#   set yv $ymin
#   while { $yv < ($ymax-0.5*$ystep) } {
#       foreach {dummy pyv} [coordsToPixel $w $scaling($w,xmin) $yv] {break}
#       set pyv1 [expr {$pyv-5}]
#       set pyv2 [expr {$pyv-35}]
#       $w create line $xw1 $pyv1 $xw3 $pyv2 $xw5 $pyv2 -fill black -tag ticklines
#       set yv [expr {$yv+$ystep}]
#   }

    Config3DBar $w -usebackground 0 -useticklines 0
}

# Draw3DBar --
#    Draw a 3D bar in a barchart
# Arguments:
#    w           Name of the canvas
#    label       Label for the bar
#    yvalue      The height of the bar
#    fill        The colour of the bar
# Result:
#    None
# Side effects:
#    The bar is drawn, the display order is adjusted
#
proc ::Plotchart::Draw3DBar { w label yvalue fill } {
    variable settings
    variable scaling

    set xv1 [expr {$scaling($w,xbase)+$scaling($w,xshift)}]
    set xv2 [expr {$xv1+$scaling($w,barwidth)}]

    foreach {x0 y0} [coordsToPixel $w $xv1 0.0]     {break}
    foreach {x1 y1} [coordsToPixel $w $xv2 $yvalue] {break}

    if { $yvalue < 0.0 } {
        foreach {y0 y1} [list $y1 $y0] {break}
        set tag d
    } else {
        set tag u
    }

    set d [expr {($x1-$x0)/3}]
    set x2 [expr {$x0+$d+1}]
    set x3 [expr {$x1+$d}]
    set y2 [expr {$y0-$d+1}]
    set y3 [expr {$y1-$d-1}]
    set y4 [expr {$y1-$d-1}]
    $w create rect $x0 $y0 $x1 $y1 -fill $fill -tag $tag
    $w create poly $x0 $y1 $x2 $y4 $x3 $y4 $x1 $y1 -fill [DimColour $fill 0.8] -outline black -tag u
    $w create poly $x1 $y1 $x3 $y3 $x3 $y2 $x1 $y0 -fill [DimColour $fill 0.6] -outline black -tag $tag

    #
    # Add the text
    #
    if { $settings($w,showvalues) } {
        $w create text [expr {($x0+$x3)/2}] [expr {$y3-5}] -text $yvalue \
            -font $settings($w,valuefont) -fill $settings($w,valuecolour) \
            -anchor s
    }
    $w create text [expr {($x0+$x3)/2}] [expr {$y0+8}] -text $label \
        -font $settings($w,labelfont) -fill $settings($w,labelcolour) \
        -anchor n

    #
    # Reorder the various bits
    #
    $w lower u
    $w lower platform
    $w lower d
    $w lower ticklines
    $w lower background

    #
    # Move to the next bar
    #
    set scaling($w,xbase) [expr {$scaling($w,xbase)+1.0}]
}

# DimColour --
#    Compute a dimmer colour
# Arguments:
#    color       Original colour
#    factor      Factor by which to reduce the colour
# Result:
#    New colour
# Note:
#    Shamelessly copied from R. Suchenwirths Wiki page on 3D bars
#
proc ::Plotchart::DimColour {color factor} {
   foreach i {r g b} n [winfo rgb . $color] d [winfo rgb . white] {
       set $i [expr int(255.*$n/$d*$factor)]
   }
   format #%02x%02x%02x $r $g $b
}

# GreyColour --
#    Compute a greyer colour
# Arguments:
#    color       Original colour
#    factor      Factor by which to mix in grey
# Result:
#    New colour
# Note:
#    Shamelessly adapted from R. Suchenwirths Wiki page on 3D bars
#
proc ::Plotchart::GreyColour {color factor} {
   foreach i {r g b} n [winfo rgb . $color] d [winfo rgb . white] e [winfo rgb . lightgrey] {
       set $i [expr int(255.*($n*$factor+$e*(1.0-$factor))/$d)]
   }
   format #%02x%02x%02x $r $g $b
}

# Draw3DLine --
#    Plot a ribbon of z-data as a function of y
# Arguments:
#    w           Name of the canvas
#    data        List of coordinate pairs y, z
#    colour      Colour to use
# Result:
#    None
# Side effect:
#    The plot of the data
#
proc ::Plotchart::Draw3DLine { w data colour } {
    variable data_series
    variable scaling

    set bright $colour
    set dim    [DimColour $colour 0.6]

    #
    # Draw the ribbon as a series of quadrangles
    #
    set xe $data_series($w,xbase)
    set xb [expr {$xe-$data_series($w,xwidth)}]

    set data_series($w,xbase) [expr {$xe-$data_series($w,xstep)}]

    foreach {yb zb} [lrange $data 0 end-2] {ye ze} [lrange $data 2 end] {

        foreach {px11 py11} [coords3DToPixel $w $xb $yb $zb] {break}
        foreach {px12 py12} [coords3DToPixel $w $xe $yb $zb] {break}
        foreach {px21 py21} [coords3DToPixel $w $xb $ye $ze] {break}
        foreach {px22 py22} [coords3DToPixel $w $xe $ye $ze] {break}

        #
        # Use the angle of the line to determine if the top or the
        # bottom side is visible
        #
        if { $px21 == $px11 ||
             ($py21-$py11)/($px21-$px11) < ($py12-$py11)/($px12-$px11) } {
            set colour $dim
        } else {
            set colour $bright
        }

        $w create polygon $px11 $py11 $px21 $py21 $px22 $py22 \
                          $px12 $py12 $px11 $py11 \
                          -fill $colour -outline black
    }
}

# Draw3DArea --
#    Plot a ribbon of z-data as a function of y with a "facade"
# Arguments:
#    w           Name of the canvas
#    data        List of coordinate pairs y, z
#    colour      Colour to use
# Result:
#    None
# Side effect:
#    The plot of the data
#
proc ::Plotchart::Draw3DArea { w data colour } {
    variable data_series
    variable scaling

    set bright $colour
    set dimmer [DimColour $colour 0.8]
    set dim    [DimColour $colour 0.6]

    #
    # Draw the ribbon as a series of quadrangles
    #
    set xe $data_series($w,xbase)
    set xb [expr {$xe-$data_series($w,xwidth)}]

    set data_series($w,xbase) [expr {$xe-$data_series($w,xstep)}]

    set facade {}

    foreach {yb zb} [lrange $data 0 end-2] {ye ze} [lrange $data 2 end] {

        foreach {px11 py11} [coords3DToPixel $w $xb $yb $zb] {break}
        foreach {px12 py12} [coords3DToPixel $w $xe $yb $zb] {break}
        foreach {px21 py21} [coords3DToPixel $w $xb $ye $ze] {break}
        foreach {px22 py22} [coords3DToPixel $w $xe $ye $ze] {break}

        $w create polygon $px11 $py11 $px21 $py21 $px22 $py22 \
                          $px12 $py12 $px11 $py11 \
                          -fill $dimmer -outline black

        lappend facade $px11 $py11
    }

    #
    # Add the last point
    #
    lappend facade $px21 $py21

    #
    # Add the polygon at the right
    #
    set zmin $scaling($w,zmin)
    foreach {px2z py2z} [coords3DToPixel $w $xe $ye $zmin] {break}
    foreach {px1z py1z} [coords3DToPixel $w $xb $ye $zmin] {break}

    $w create polygon $px21 $py21 $px22 $py22 \
                      $px2z $py2z $px1z $py1z \
                      -fill $dim -outline black

    foreach {pxb pyb} [coords3DToPixel $w $xb $ye $zmin] {break}

    set yb [lindex $data 0]
    foreach {pxe pye} [coords3DToPixel $w $xb $yb $zmin] {break}

    lappend facade $px21 $py21 $pxb $pyb $pxe $pye

    $w create polygon $facade -fill $colour -outline black
}
