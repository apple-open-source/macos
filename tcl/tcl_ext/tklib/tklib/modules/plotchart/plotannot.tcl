# plotannot.tcl --
#    Facilities for annotating charts
#
# Note:
#    This source file contains such functions as to draw a
#    balloon text in an xy-graph.
#    It is the companion of "plotchart.tcl"
#

#
# Static data
#
namespace eval ::Plotchart {
    # Index, three pairs of scale factors to determine xy-coordinates
    set BalloonDir(north-west) {0  0  1 -2 -2  1  0}
    set BalloonDir(north)      {1 -1  0  0 -3  1  0}
    set BalloonDir(north-east) {2 -1  0  2 -2  0  1}
    set BalloonDir(east)       {3  0 -1  3  0  0  1}
    set BalloonDir(south-east) {4  0 -1  2  2 -1  0}
    set BalloonDir(south)      {5  1  0  0  3 -1  0}
    set BalloonDir(south-west) {6  1  0 -2  2  0 -1}
    set BalloonDir(west)       {7  0  1 -3  0  0 -1}

    set TextDir(centre)     c
    set TextDir(center)     c
    set TextDir(c)          c
    set TextDir(west)       w
    set TextDir(w)          w
    set TextDir(north-west) nw
    set TextDir(nw)         nw
    set TextDir(north)      n
    set TextDir(n)          n
    set TextDir(north-east) ew
    set TextDir(ne)         ew
    set TextDir(east)       e
    set TextDir(e)          e
    set TextDir(south-west) nw
    set TextDir(sw)         sw
    set TextDir(south)      s
    set TextDir(s)          s
    set TextDir(south-east) ew
    set TextDir(east)       e
}

# DefaultBalloon --
#    Set the default properties of balloon text and other types of annotation
# Arguments:
#    w           Name of the canvas
# Result:
#    None
# Side effects:
#    Stores the default settings
#
proc ::Plotchart::DefaultBalloon { w } {
    variable settings

    foreach {option value} {font       fixed
                            margin     5
                            textcolour black
                            justify    left
                            arrowsize  5
                            background white
                            outline    black
                            rimwidth   1} {
        set settings($w,balloon$option) $value
    }
    foreach {option value} {font       fixed
                            colour     black
                            justify    left} {
        set settings($w,text$option) $value
    }
}

# ConfigBalloon --
#    Configure the properties of balloon text
# Arguments:
#    w           Name of the canvas
#    args        List of arguments
# Result:
#    None
# Side effects:
#    Stores the new settings for the next balloon text
#
proc ::Plotchart::ConfigBalloon { w args } {
    variable settings

    foreach {option value} $args {
        set option [string range $option 1 end]
        switch -- $option {
            "font" -
            "margin" -
            "textcolour" -
            "justify" -
            "arrowsize" -
            "background" -
            "outline" -
            "rimwidth" {
                set settings($w,balloon$option) $value
            }
            "textcolor" {
                set settings($w,balloontextcolour) $value
            }
        }
    }
}

# ConfigPlainText --
#    Configure the properties of plain text
# Arguments:
#    w           Name of the canvas
#    args        List of arguments
# Result:
#    None
# Side effects:
#    Stores the new settings for the next plain text
#
proc ::Plotchart::ConfigPlainText { w args } {
    variable settings

    foreach {option value} $args {
        set option [string range $option 1 end]
        switch -- $option {
            "font" -
            "textcolour" -
            "justify" {
                set settings($w,text$option) $value
            }
            "textcolor" {
                set settings($w,textcolour) $value
            }
            "textfont" {
                # Ugly hack!
                set settings($w,$option) $value
            }
        }
    }
}

# DrawBalloon --
#    Plot a balloon text in a chart
# Arguments:
#    w           Name of the canvas
#    x           X-coordinate of the point the arrow points to
#    y           Y-coordinate of the point the arrow points to
#    text        Text in the balloon
#    dir         Direction of the arrow (north, north-east, ...)
# Result:
#    None
# Side effects:
#    Text and polygon drawn in the chart
#
proc ::Plotchart::DrawBalloon { w x y text dir } {
    variable settings
    variable BalloonDir

    #
    # Create the item and then determine the coordinates
    # of the frame around the text
    #
    set item [$w create text 0 0 -text $text -tag BalloonText \
                 -font $settings($w,balloonfont) -fill $settings($w,balloontextcolour) \
                 -justify $settings($w,balloonjustify)]

    if { ![info exists BalloonDir($dir)] } {
        set dir south-east
    }

    foreach {xmin ymin xmax ymax} [$w bbox $item] {break}

    set xmin   [expr {$xmin-$settings($w,balloonmargin)}]
    set xmax   [expr {$xmax+$settings($w,balloonmargin)}]
    set ymin   [expr {$ymin-$settings($w,balloonmargin)}]
    set ymax   [expr {$ymax+$settings($w,balloonmargin)}]

    set xcentr [expr {($xmin+$xmax)/2}]
    set ycentr [expr {($ymin+$ymax)/2}]
    set coords [list $xmin   $ymin   \
                     $xcentr $ymin   \
                     $xmax   $ymin   \
                     $xmax   $ycentr \
                     $xmax   $ymax   \
                     $xcentr $ymax   \
                     $xmin   $ymax   \
                     $xmin   $ycentr ]

    set idx    [lindex $BalloonDir($dir) 0]
    set scales [lrange $BalloonDir($dir) 1 end]

    set factor $settings($w,balloonarrowsize)
    set extraCoords {}

    set xbase  [lindex $coords [expr {2*$idx}]]
    set ybase  [lindex $coords [expr {2*$idx+1}]]

    foreach {xscale yscale} $scales {
        set xnew [expr {$xbase+$xscale*$factor}]
        set ynew [expr {$ybase+$yscale*$factor}]
        lappend extraCoords $xnew $ynew
    }

    #
    # Insert the extra coordinates
    #
    set coords [eval lreplace [list $coords] [expr {2*$idx}] [expr {2*$idx+1}] \
                          $extraCoords]

    set xpoint [lindex $coords [expr {2*$idx+2}]]
    set ypoint [lindex $coords [expr {2*$idx+3}]]

    set poly [$w create polygon $coords -tag BalloonFrame \
                  -fill $settings($w,balloonbackground) \
                  -width $settings($w,balloonrimwidth)  \
                  -outline $settings($w,balloonoutline)]

    #
    # Position the two items
    #
    foreach {xtarget ytarget} [coordsToPixel $w $x $y] {break}
    set dx [expr {$xtarget-$xpoint}]
    set dy [expr {$ytarget-$ypoint}]
    $w move $item  $dx $dy
    $w move $poly  $dx $dy
    $w raise BalloonFrame
    $w raise BalloonText
}

# DrawPlainText --
#    Plot plain text in a chart
# Arguments:
#    w           Name of the canvas
#    x           X-coordinate of the point the arrow points to
#    y           Y-coordinate of the point the arrow points to
#    text        Text to be drawn
#    anchor      Anchor position (north, north-east, ..., defaults to centre)
# Result:
#    None
# Side effects:
#    Text drawn in the chart
#
proc ::Plotchart::DrawPlainText { w x y text {anchor centre} } {
    variable settings
    variable TextDir

    foreach {xtext ytext} [coordsToPixel $w $x $y] {break}

    if { [info exists TextDir($anchor)] } {
        set anchor $TextDir($anchor)
    } else {
        set anchor c
    }

    $w create text $xtext $ytext -text $text -tag PlainText \
         -font $settings($w,textfont) -fill $settings($w,textcolour) \
         -justify $settings($w,textjustify) -anchor $anchor

    $w raise PlainText
}

# BrightenColour --
#    Compute a brighter colour
# Arguments:
#    color       Original colour
#    intensity   Colour to interpolate with
#    factor      Factor by which to brighten the colour
# Result:
#    New colour
# Note:
#    Adapted from R. Suchenwirths Wiki page on 3D bars
#
proc ::Plotchart::BrightenColour {color intensity factor} {
    foreach i {r g b} n [winfo rgb . $color] d [winfo rgb . $intensity] f [winfo rgb . white] {
        #checker exclude warnVarRef
        set $i [expr {int(255.*($n+($d-$n)*$factor)/$f)}]
    }
    #checker exclude warnUndefinedVar
    format #%02x%02x%02x $r $g $b
}

# DrawGradientBackground --
#    Add a gradient background to the plot
# Arguments:
#    w           Name of the canvas
#    colour      Main colour
#    dir         Direction of the gradient (left-right, top-down,
#                bottom-up, right-left)
#    intensity   Brighten (white) or darken (black) the colours
#    rect        (Optional) coordinates of the rectangle to be filled
# Result:
#    None
# Side effects:
#    Gradient background drawn in the chart
#
proc ::Plotchart::DrawGradientBackground { w colour dir intensity {rect {}} } {
    variable scaling

    set pxmin $scaling($w,pxmin)
    set pxmax $scaling($w,pxmax)
    set pymin $scaling($w,pymin)
    set pymax $scaling($w,pymax)

    if { $rect != {} } {
        foreach {rxmin rymin rxmax rymax} $rect {break}
    } else {
        set rxmin $pxmin
        set rxmax $pxmax
        set rymin $pymin
        set rymax $pymax
    }

    switch -- $dir {
        "left-right" {
            set dir   h
            set first 0.0
            set last  1.0
            set fac   [expr {($pxmax-$pxmin)/50.0}]
        }
        "right-left" {
            set dir   h
            set first 1.0
            set last  0.0
            set fac   [expr {($pxmax-$pxmin)/50.0}]
        }
        "top-down" {
            set dir   v
            set first 0.0
            set last  1.0
            set fac   [expr {($pymin-$pymax)/50.0}]
        }
        "bottom-up" {
            set dir   v
            set first 1.0
            set last  0.0
            set fac   [expr {($pymin-$pymax)/50.0}]
        }
        default {
            set dir   v
            set first 0.0
            set last  1.0
            set fac   [expr {($pymin-$pymax)/50.0}]
        }
    }

    if { $dir == "h" } {
        set x2 $rxmin
        set y1 $rymin
        set y2 $rymax
    } else {
        set y2 $rymax
        set x1 $rxmin
        set x2 $rxmax
    }

    set n 50
    if { $dir == "h" } {
        set nmax [expr {ceil($n*($rxmax-$rxmin)/double($pxmax-$pxmin))}]
    } else {
        set nmax [expr {ceil($n*($rymin-$rymax)/double($pymin-$pymax))}]
    }
    for { set i 0 } { $i < $nmax } { incr i } {
        set factor [expr {($first*$i+$last*($n-$i-1))/double($n)}]
        set gcolour [BrightenColour $colour $intensity $factor]

        if { $dir == "h" } {
            set x1     $x2
            set x2     [expr {$rxmin+($i+1)*$fac}]
            if { $i == $nmax-1 } {
                set x2 $rxmax
            }
        } else {
            set y1     $y2
            set y2     [expr {$rymax+($i+1)*$fac}]
            if { $i == $nmax-1 } {
                set y2 $rymin
            }
        }

        $w create rectangle $x1 $y1 $x2 $y2 -fill $gcolour -outline $gcolour -tag {data background}
    }

    $w lower data
    $w lower background
}

# DrawImageBackground --
#    Add an image (tilde) to the background to the plot
# Arguments:
#    w           Name of the canvas
#    colour      Main colour
#    image       Name of the image
# Result:
#    None
# Side effects:
#    Image appears in the plot area, tiled if needed
#
proc ::Plotchart::DrawImageBackground { w image } {
    variable scaling

    set pxmin $scaling($w,pxmin)
    set pxmax $scaling($w,pxmax)
    set pymin $scaling($w,pymin)
    set pymax $scaling($w,pymax)

    set iwidth  [image width $image]
    set iheight [image height $image]

    for { set y $pymax } { $y > $pymin } { set y [expr {$y-$iheight}] } {
        for { set x $pxmin } { $x < $pxmax } { incr x $iwidth } {
            $w create image $x $y -image $image -anchor sw -tags {data background}
        }
    }

    $w lower data
    $w lower background
}
