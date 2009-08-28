# plotaxis.tcl --
#    Facilities to draw simple plots in a dedicated canvas
#
# Note:
#    This source file contains the functions for drawing the axes
#    and the legend. It is the companion of "plotchart.tcl"
#

# DrawYaxis --
#    Draw the y-axis
# Arguments:
#    w           Name of the canvas
#    ymin        Minimum y coordinate
#    ymax        Maximum y coordinate
#    ystep       Step size
# Result:
#    None
# Side effects:
#    Axis drawn in canvas
#
proc ::Plotchart::DrawYaxis { w ymin ymax ydelt } {
    variable scaling
    variable config

    set scaling($w,ydelt) $ydelt

    $w delete yaxis

    set linecolor  $config($w,leftaxis,color)
    set textcolor  $config($w,leftaxis,textcolor)
    set textfont   $config($w,leftaxis,font)
    set ticklength $config($w,leftaxis,ticklength)
    set thickness  $config($w,leftaxis,thickness)
    set offtick    [expr {($ticklength > 0)? $ticklength+2 : 2}]

    $w create line $scaling($w,pxmin) $scaling($w,pymin) \
                   $scaling($w,pxmin) $scaling($w,pymax) \
                   -fill $linecolor -tag yaxis -width $thickness

    set format $config($w,leftaxis,format)
    if { [info exists scaling($w,-format,y)] } {
        set format $scaling($w,-format,y)
    }

    if { $ymax > $ymin } {
        set y [expr {$ymin+0.0}]  ;# Make sure we have the number in the right format
        set ym $ymax
    } else {
        set y [expr {$ymax+0.0}]
        set ym $ymin
    }
    set yt [expr {$ymin+0.0}]

    set scaling($w,yaxis) {}

    while { $y < $ym+0.5*abs($ydelt) } {

        foreach {xcrd ycrd} [coordsToPixel $w $scaling($w,xmin) $yt] {break}
        set xcrd2 [expr {$xcrd-$ticklength}]
        set xcrd3 [expr {$xcrd-$offtick}]

        lappend scaling($w,yaxis) $ycrd

        set ylabel $yt
        if { $format != "" } {
            set ylabel [format $format $y]
        }
        $w create line $xcrd2 $ycrd $xcrd $ycrd -tag yaxis -fill $linecolor
        $w create text $xcrd3 $ycrd -text $ylabel -tag yaxis -anchor e \
            -fill $textcolor -font $textfont
        set y  [expr {$y+abs($ydelt)}]
        set yt [expr {$yt+$ydelt}]
        if { abs($yt) < 0.5*abs($ydelt) } {
            set yt 0.0
        }
    }
}

# DrawRightaxis --
#    Draw the y-axis on the right-hand side
# Arguments:
#    w           Name of the canvas
#    ymin        Minimum y coordinate
#    ymax        Maximum y coordinate
#    ystep       Step size
# Result:
#    None
# Side effects:
#    Axis drawn in canvas
#
proc ::Plotchart::DrawRightaxis { w ymin ymax ydelt } {
    variable scaling
    variable config

    set scaling($w,ydelt) $ydelt

    $w delete raxis

    set linecolor  $config($w,rightaxis,color)
    set textcolor  $config($w,rightaxis,textcolor)
    set textfont   $config($w,rightaxis,font)
    set thickness  $config($w,rightaxis,thickness)
    set ticklength $config($w,rightaxis,ticklength)
    set offtick    [expr {($ticklength > 0)? $ticklength+2 : 2}]

    $w create line $scaling($w,pxmax) $scaling($w,pymin) \
                   $scaling($w,pxmax) $scaling($w,pymax) \
                   -fill $linecolor -tag raxis -width $thickness

    set format $config($w,rightaxis,format)
    if { [info exists scaling($w,-format,y)] } {
        set format $scaling($w,-format,y)
    }

    if { $ymax > $ymin } {
        set y [expr {$ymin+0.0}]  ;# Make sure we have the number in the right format
        set ym $ymax
    } else {
        set y [expr {$ymax+0.0}]
        set ym $ymin
    }
    set yt [expr {$ymin+0.0}]

    set scaling($w,yaxis) {}

    while { $y < $ym+0.5*abs($ydelt) } {

        foreach {xcrd ycrd} [coordsToPixel $w $scaling($w,xmax) $yt] {break}
        set xcrd2 [expr {$xcrd+$ticklength}]
        set xcrd3 [expr {$xcrd+$offtick}]

        lappend scaling($w,yaxis) $ycrd

        set ylabel $yt
        if { $format != "" } {
            set ylabel [format $format $yt]
        }
        $w create line $xcrd2 $ycrd $xcrd $ycrd -tag raxis -fill $linecolor
        $w create text $xcrd3 $ycrd -text $ylabel -tag raxis -anchor w \
            -fill $textcolor -font $textfont
        set y  [expr {$y+abs($ydelt)}]
        set yt [expr {$yt+$ydelt}]
        if { abs($yt) < 0.5*abs($ydelt) } {
            set yt 0.0
        }
    }
}

# DrawLogYaxis --
#    Draw the logarithmic y-axis
# Arguments:
#    w           Name of the canvas
#    ymin        Minimum y coordinate
#    ymax        Maximum y coordinate
#    ystep       Step size
# Result:
#    None
# Side effects:
#    Axis drawn in canvas
#
proc ::Plotchart::DrawLogYaxis { w ymin ymax ydelt } {
    variable scaling
    variable config

    set scaling($w,ydelt) $ydelt

    $w delete yaxis

    set linecolor  $config($w,leftaxis,color)
    set textcolor  $config($w,leftaxis,textcolor)
    set textfont   $config($w,leftaxis,font)
    set thickness  $config($w,leftaxis,thickness)
    set ticklength $config($w,leftaxis,ticklength)
    set offtick    [expr {($ticklength > 0)? $ticklength+2 : 2}]

    $w create line $scaling($w,pxmin) $scaling($w,pymin) \
                   $scaling($w,pxmin) $scaling($w,pymax) \
                   -fill $linecolor -tag yaxis -width $thickness

    set format $config($w,bottomaxis,format)
    set format $config($w,leftaxis,format)
    if { [info exists scaling($w,-format,y)] } {
        set format $scaling($w,-format,y)
    }

    set scaling($w,yaxis) {}

    set y       [expr {pow(10.0,floor(log10($ymin)))}]
    set ylogmax [expr {pow(10.0,ceil(log10($ymax)))+0.1}]

    while { $y < $ylogmax } {

        #
        # Labels and tickmarks
        #
        foreach factor {1.0 2.0 3.0 4.0 5.0 6.0 7.0 8.0 9.0} {
            set yt [expr {$y*$factor}]
            if { $yt < $ymin } continue
            if { $yt > $ymax } break

            foreach {xcrd ycrd} [coordsToPixel $w $scaling($w,xmin) [expr {log10($yt)}]] {break}
            set xcrd2 [expr {$xcrd-$ticklength}]
            set xcrd3 [expr {$xcrd-$offtick}]

            lappend scaling($w,yaxis) $ycrd

            set ylabel $y
            if { $format != "" } {
                set ylabel [format $format $y]
            }
            $w create line $xcrd2 $ycrd $xcrd $ycrd -tag yaxis -fill $linecolor
            if { $factor == 1.0 } {
                $w create text $xcrd3 $ycrd -text $ylabel -tag yaxis -anchor e \
                    -fill $textcolor -font $textfont
            }
        }
        set y [expr {10.0*$y}]
    }
}

# DrawXaxis --
#    Draw the x-axis
# Arguments:
#    w           Name of the canvas
#    xmin        Minimum x coordinate
#    xmax        Maximum x coordinate
#    xstep       Step size
# Result:
#    None
# Side effects:
#    Axis drawn in canvas
#
proc ::Plotchart::DrawXaxis { w xmin xmax xdelt } {
    variable scaling
    variable config

    set scaling($w,xdelt) $xdelt

    $w delete xaxis

    set linecolor  $config($w,bottomaxis,color)
    set textcolor  $config($w,bottomaxis,textcolor)
    set textfont   $config($w,bottomaxis,font)
    set thickness  $config($w,bottomaxis,thickness)
    set ticklength $config($w,bottomaxis,ticklength)
    set offtick    [expr {($ticklength > 0)? $ticklength+2 : 2}]

    $w create line $scaling($w,pxmin) $scaling($w,pymax) \
                   $scaling($w,pxmax) $scaling($w,pymax) \
                   -fill black -tag xaxis

    set format $config($w,bottomaxis,format)
    if { [info exists scaling($w,-format,x)] } {
        set format $scaling($w,-format,x)
    }

    if { $xmax > $xmin } {
        set x [expr {$xmin+0.0}]  ;# Make sure we have the number in the right format
        set xm $xmax
    } else {
        set x [expr {$xmax+0.0}]
        set xm $xmin
    }
    set xt [expr {$xmin+0.0}]
    set scaling($w,xaxis) {}

    while { $x < $xm+0.5*abs($xdelt) } {

        foreach {xcrd ycrd} [coordsToPixel $w $xt $scaling($w,ymin)] {break}
        set ycrd2 [expr {$ycrd+$ticklength}]
        set ycrd3 [expr {$ycrd+$offtick}]

        lappend scaling($w,xaxis) $xcrd

        set xlabel $xt
        if { $format != "" } {
            set xlabel [format $format $xt]
        }

        $w create line $xcrd $ycrd2 $xcrd $ycrd -tag xaxis -fill $linecolor
        $w create text $xcrd $ycrd3 -text $xlabel -tag xaxis -anchor n \
            -fill $textcolor -font $textfont
        set x  [expr {$x+abs($xdelt)}]
        set xt [expr {$xt+$xdelt}]
        if { abs($x) < 0.5*abs($xdelt) } {
            set xt 0.0
        }
    }

    set scaling($w,xdelt) $xdelt
}

# DrawXtext --
#    Draw text to the x-axis
# Arguments:
#    w           Name of the canvas
#    text        Text to be drawn
# Result:
#    None
# Side effects:
#    Text drawn in canvas
#
proc ::Plotchart::DrawXtext { w text } {
    variable scaling
    variable config

    set textcolor  $config($w,bottomaxis,textcolor)
    set textfont   $config($w,bottomaxis,font)

    set xt [expr {($scaling($w,pxmin)+$scaling($w,pxmax))/2}]
    set yt [expr {$scaling($w,pymax)+18}]

    $w create text $xt $yt -text $text -fill $textcolor -anchor n -font $textfont
}

# DrawYtext --
#    Draw text to the y-axis
# Arguments:
#    w           Name of the canvas
#    text        Text to be drawn
# Result:
#    None
# Side effects:
#    Text drawn in canvas
#
proc ::Plotchart::DrawYtext { w text } {
    variable scaling
    variable config


    if { [string match "r*" $w] == 0 } {
        set textcolor  $config($w,leftaxis,textcolor)
        set textfont   $config($w,leftaxis,font)
        set xt         $scaling($w,pxmin)
        set anchor     se
    } else {
        set textcolor  $config($w,rightaxis,textcolor)
        set textfont   $config($w,rightaxis,font)
        set xt $scaling($w,pxmax)
        set anchor     sw
    }
    set yt [expr {$scaling($w,pymin)-8}]

    $w create text $xt $yt -text $text -fill $textcolor -anchor $anchor -font $textfont
}

# DrawPolarAxes --
#    Draw thw two polar axes
# Arguments:
#    w           Name of the canvas
#    rad_max     Maximum radius
#    rad_step    Step in radius
# Result:
#    None
# Side effects:
#    Axes drawn in canvas
#
proc ::Plotchart::DrawPolarAxes { w rad_max rad_step } {
    variable config

    set linecolor  $config($w,axis,color)
    set textcolor  $config($w,axis,textcolor)
    set textfont   $config($w,axis,font)
    set thickness  $config($w,axis,thickness)
    set bgcolor    $config($w,background,innercolor)

    #
    # Draw the spikes
    #
    set angle 0.0

    foreach {xcentre ycentre} [polarToPixel $w 0.0 0.0] {break}

    while { $angle < 360.0 } {
        foreach {xcrd ycrd} [polarToPixel $w $rad_max $angle] {break}
        foreach {xtxt ytxt} [polarToPixel $w [expr {1.05*$rad_max}] $angle] {break}
        $w create line $xcentre $ycentre $xcrd $ycrd -fill $linecolor -width $thickness
        if { $xcrd > $xcentre } {
            set dir w
        } else {
            set dir e
        }
        $w create text $xtxt $ytxt -text $angle -anchor $dir -fill $textcolor -font $textfont

        set angle [expr {$angle+30}]
    }

    #
    # Draw the concentric circles
    #
    set rad $rad_step

    while { $rad < $rad_max+0.5*$rad_step } {
        foreach {xright ytxt}    [polarToPixel $w $rad    0.0] {break}
        foreach {xleft  ycrd}    [polarToPixel $w $rad  180.0] {break}
        foreach {xcrd   ytop}    [polarToPixel $w $rad   90.0] {break}
        foreach {xcrd   ybottom} [polarToPixel $w $rad  270.0] {break}

        set oval [$w create oval $xleft $ytop $xright $ybottom -outline $linecolor -width $thickness -fill {}]
        $w lower $oval

        $w create text $xright [expr {$ytxt+3}] -text $rad -anchor n -fill $textcolor -font $textfont

        set rad [expr {$rad+$rad_step}]
    }
}

# DrawXlabels --
#    Draw the labels to an x-axis (barchart)
# Arguments:
#    w           Name of the canvas
#    xlabels     List of labels
#    noseries    Number of series or "stacked"
# Result:
#    None
# Side effects:
#    Axis drawn in canvas
#
proc ::Plotchart::DrawXlabels { w xlabels noseries } {
    variable scaling
    variable config

    set linecolor  $config($w,bottomaxis,color)
    set textcolor  $config($w,bottomaxis,textcolor)
    set textfont   $config($w,bottomaxis,font)
    set thickness  $config($w,bottomaxis,thickness)

    $w delete xaxis

    $w create line $scaling($w,pxmin) $scaling($w,pymax) \
                   $scaling($w,pxmax) $scaling($w,pymax) \
                   -fill $linecolor -width $thickness -tag xaxis

    set x 0.5
    set scaling($w,ybase) {}
    foreach label $xlabels {
        foreach {xcrd ycrd} [coordsToPixel $w $x $scaling($w,ymin)] {break}
        set ycrd [expr {$ycrd+2}]
        $w create text $xcrd $ycrd -text $label -tag xaxis -anchor n \
            -fill $textcolor -font $textfont
        set x [expr {$x+1.0}]

        lappend scaling($w,ybase) 0.0
    }

    set scaling($w,xbase) 0.0

    if { $noseries != "stacked" } {
        set scaling($w,stacked)  0
        set scaling($w,xshift)   [expr {1.0/$noseries}]
        set scaling($w,barwidth) [expr {1.0/$noseries}]
    } else {
        set scaling($w,stacked)  1
        set scaling($w,xshift)   0.0
        set scaling($w,barwidth) 0.8
        set scaling($w,xbase)    0.1
    }
}

# DrawYlabels --
#    Draw the labels to a y-axis (barchart)
# Arguments:
#    w           Name of the canvas
#    ylabels     List of labels
#    noseries    Number of series or "stacked"
# Result:
#    None
# Side effects:
#    Axis drawn in canvas
#
proc ::Plotchart::DrawYlabels { w ylabels noseries } {
    variable scaling
    variable config

    set linecolor  $config($w,leftaxis,color)
    set textcolor  $config($w,leftaxis,textcolor)
    set textfont   $config($w,leftaxis,font)
    set thickness  $config($w,leftaxis,thickness)

    $w delete yaxis

    $w create line $scaling($w,pxmin) $scaling($w,pymin) \
                   $scaling($w,pxmin) $scaling($w,pymax) \
                   -fill $linecolor -width $thickness -tag yaxis

    set y 0.5
    set scaling($w,xbase) {}
    foreach label $ylabels {
        foreach {xcrd ycrd} [coordsToPixel $w $scaling($w,xmin) $y] {break}
        set xcrd [expr {$xcrd-2}]
        $w create text $xcrd $ycrd -text $label -tag yaxis -anchor e \
            -fill $textcolor -font $textfont
        set y [expr {$y+1.0}]

        lappend scaling($w,xbase) 0.0
    }

    set scaling($w,ybase) 0.0

    if { $noseries != "stacked" } {
        set scaling($w,stacked)  0
        set scaling($w,yshift)   [expr {1.0/$noseries}]
        set scaling($w,barwidth) [expr {1.0/$noseries}]
    } else {
        set scaling($w,stacked)  1
        set scaling($w,yshift)   0.0
        set scaling($w,barwidth) 0.8
        set scaling($w,ybase)    0.1
    }
}

# XConfig --
#    Configure the x-axis for an XY plot
# Arguments:
#    w           Name of the canvas
#    args        Option and value pairs
# Result:
#    None
#
proc ::Plotchart::XConfig { w args } {
    AxisConfig xyplot $w x DrawXaxis $args
}

# YConfig --
#    Configure the y-axis for an XY plot
# Arguments:
#    w           Name of the canvas
#    args        Option and value pairs
# Result:
#    None
#
proc ::Plotchart::YConfig { w args } {
    AxisConfig xyplot $w y DrawYaxis $args
}

# AxisConfig --
#    Configure an axis and redraw it if necessary
# Arguments:
#    plottype       Type of plot
#    w              Name of the canvas
#    orient         Orientation of the axis
#    drawmethod     Drawing method
#    option_values  Option/value pairs
# Result:
#    None
#
proc ::Plotchart::AxisConfig { plottype w orient drawmethod option_values } {
    variable scaling
    variable axis_options
    variable axis_option_clear
    variable axis_option_values

    set clear_data 0

    foreach {option value} $option_values {
        set idx [lsearch $axis_options $option]
        if { $idx < 0 } {
            return -code error "Unknown or invalid option: $option (value: $value)"
        } else {
            set clear_data [lindex  $axis_option_clear  $idx]
            set values     [lindex  $axis_option_values [expr {2*$idx+1}]]
            if { $values != "..." } {
                if { [lsearch $values $value] < 0 } {
                    return -code error "Unknown or invalid value: $value for option $option - $values"
                }
            }
            set scaling($w,$option,$orient) $value
            if { $option == "-scale" } {
                set min  ${orient}min
                set max  ${orient}max
                set delt ${orient}delt
                foreach [list $min $max $delt] $value {break}
                set scaling($w,$min)  [set $min]
                set scaling($w,$max)  [set $max]
                set scaling($w,$delt) [set $delt]
            }
        }
    }

    if { $clear_data }  {
        $w delete data
    }

    if { $orient == "x" } {
        $drawmethod $w $scaling($w,xmin) $scaling($w,xmax) $scaling($w,xdelt)
    }
    if { $orient == "y" } {
        $drawmethod $w $scaling($w,ymin) $scaling($w,ymax) $scaling($w,ydelt)
    }
    if { $orient == "z" } {
        $drawmethod $w $scaling($w,zmin) $scaling($w,zmax) $scaling($w,zdelt)
    }
}

# DrawXTicklines --
#    Draw the ticklines for the x-axis
# Arguments:
#    w           Name of the canvas
#    colour      Colour of the ticklines
# Result:
#    None
#
proc ::Plotchart::DrawXTicklines { w {colour black} } {
    DrawTicklines $w x $colour
}

# DrawYTicklines --
#    Draw the ticklines for the y-axis
# Arguments:
#    w           Name of the canvas
#    colour      Colour of the ticklines
# Result:
#    None
#
proc ::Plotchart::DrawYTicklines { w {colour black} } {
    DrawTicklines $w y $colour
}

# DrawTicklines --
#    Draw the ticklines
# Arguments:
#    w           Name of the canvas
#    axis        Which axis (x or y)
#    colour      Colour of the ticklines
# Result:
#    None
#
proc ::Plotchart::DrawTicklines { w axis {colour black} } {
    variable scaling

    if { $axis == "x" } {
        #
        # Cater for both regular x-axes and time-axes
        #
        if { [info exists scaling($w,xaxis)] } {
            set botaxis xaxis
        } else {
            set botaxis taxis
        }
        if { $colour != {} } {
            foreach x $scaling($w,$botaxis) {
                $w create line $x $scaling($w,pymin) \
                               $x $scaling($w,pymax) \
                               -fill $colour -tag xtickline
            }
        } else {
            $w delete xtickline
        }
    } else {
        if { $colour != {} } {
            foreach y $scaling($w,yaxis) {
                $w create line $scaling($w,pxmin) $y \
                               $scaling($w,pxmax) $y \
                               -fill $colour -tag ytickline
            }
        } else {
            $w delete ytickline
        }
    }
}

# DefaultLegend --
#    Set all legend options to default
# Arguments:
#    w              Name of the canvas
# Result:
#    None
#
proc ::Plotchart::DefaultLegend { w } {
    variable legend
    variable config

    set legend($w,background) $config($w,legend,background)
    set legend($w,border)     $config($w,legend,border)
    set legend($w,canvas)     $w
    set legend($w,position)   $config($w,legend,position)
    set legend($w,series)     ""
    set legend($w,text)       ""
}

# LegendConfigure --
#    Configure the legend
# Arguments:
#    w              Name of the canvas
#    args           Key-value pairs
# Result:
#    None
#
proc ::Plotchart::LegendConfigure { w args } {
    variable legend

    foreach {option value} $args {
        switch -- $option {
            "-background" {
                 set legend($w,background) $value
            }
            "-border" {
                 set legend($w,border) $value
            }
            "-canvas" {
                 set legend($w,canvas) $value
            }
            "-position" {
                 if { [lsearch {top-left top-right bottom-left bottom-right} $value] >= 0 } {
                     set legend($w,position) $value
                 } else {
                     return -code error "Unknown or invalid position: $value"
                 }
            }
            default {
                return -code error "Unknown or invalid option: $option (value: $value)"
            }
        }
    }
}

# DrawLegend --
#    Draw or extend the legend
# Arguments:
#    w              Name of the canvas
#    series         For which series?
#    text           Text to be shown
# Result:
#    None
#
proc ::Plotchart::DrawLegend { w series text } {
    variable legend
    variable scaling
    variable data_series

    if { [string match r* $w] } {
        set w [string range $w 1 end]
    }

    lappend legend($w,series) $series
    lappend legend($w,text)   $text
    set legendw               $legend($w,canvas)

    $legendw delete legend
    $legendw delete legendbg

    set y 0
    foreach series $legend($w,series) text $legend($w,text) {

        set colour "black"
        if { [info exists data_series($w,$series,-colour)] } {
            set colour $data_series($w,$series,-colour)
        }
        set type "line"
        if { [info exists data_series($w,$series,-type)] } {
            set type $data_series($w,$series,-type)
        }
        if { [info exists data_series($w,legendtype)] } {
            set type $data_series($w,legendtype)
        }

        # TODO: line or rectangle!

        if { $type != "rectangle" } {
            $legendw create line 0 $y 15 $y -fill $colour -tag legend

            if { $type == "symbol" || $type == "both" } {
                set symbol "dot"
                if { [info exists data_series($w,$series,-symbol)] } {
                    set symbol $data_series($w,$series,-symbol)
                }
                DrawSymbolPixel $legendw $series 7 $y $symbol $colour [list legend legend_$series]
            }
        } else {
            $legendw create rectangle 0 [expr {$y-3}] 15 [expr {$y+3}] \
                -fill $colour -tag [list legend legend_$series]
        }

        $legendw create text 25 $y -text $text -anchor w -tag [list legend legend_$series]

        incr y 10   ;# TODO: size of font!
    }

    #
    # Now the frame and the background
    #
    foreach {xl yt xr yb} [$legendw bbox legend] {break}

    set xl [expr {$xl-2}]
    set xr [expr {$xr+2}]
    set yt [expr {$yt-2}]
    set yb [expr {$yb+2}]

    $legendw create rectangle $xl $yt $xr $yb -fill $legend($w,background) \
        -outline $legend($w,border) -tag legendbg

    $legendw raise legend

    if { $w == $legendw } {
        switch -- $legend($w,position) {
            "top-left" {
                 set dx [expr { 10+$scaling($w,pxmin)-$xl}]
                 set dy [expr { 10+$scaling($w,pymin)-$yt}]
            }
            "top-right" {
                 set dx [expr {-10+$scaling($w,pxmax)-$xr}]
                 set dy [expr { 10+$scaling($w,pymin)-$yt}]
            }
            "bottom-left" {
                 set dx [expr { 10+$scaling($w,pxmin)-$xl}]
                 set dy [expr {-10+$scaling($w,pymax)-$yb}]
            }
            "bottom-right" {
                 set dx [expr {-10+$scaling($w,pxmax)-$xr}]
                 set dy [expr {-10+$scaling($w,pymax)-$yb}]
            }
        }
    } else {
        set dx 10
        set dy 10
    }

    $legendw move legend   $dx $dy
    $legendw move legendbg $dx $dy
}

# DrawTimeaxis --
#    Draw the date/time-axis
# Arguments:
#    w           Name of the canvas
#    tmin        Minimum date/time
#    tmax        Maximum date/time
#    tstep       Step size in days
# Result:
#    None
# Side effects:
#    Axis drawn in canvas
#
proc ::Plotchart::DrawTimeaxis { w tmin tmax tdelt } {
    variable scaling
    variable config

    set linecolor  $config($w,bottomaxis,color)
    set textcolor  $config($w,bottomaxis,textcolor)
    set textfont   $config($w,bottomaxis,font)
    set thickness  $config($w,bottomaxis,thickness)
    set ticklength $config($w,bottomaxis,ticklength)
    set offtick    [expr {($ticklength > 0)? $ticklength+2 : 2}]


    set scaling($w,tdelt) $tdelt

    $w delete taxis

    $w create line $scaling($w,pxmin) $scaling($w,pymax) \
                   $scaling($w,pxmax) $scaling($w,pymax) \
                   -fill $linecolor -width $thickness -tag taxis

    set format $config($w,bottomaxis,format)
    if { [info exists scaling($w,-format,x)] } {
        set format $scaling($w,-format,x)
    }

    set ttmin  [clock scan $tmin]
    set ttmax  [clock scan $tmax]
    set t      [expr {int($ttmin)}]
    set ttdelt [expr {$tdelt*86400.0}]

    set scaling($w,taxis) {}

    while { $t < $ttmax+0.5*$ttdelt } {

        foreach {xcrd ycrd} [coordsToPixel $w $t $scaling($w,ymin)] {break}
        set ycrd2 [expr {$ycrd+$ticklength}]
        set ycrd3 [expr {$ycrd+$offtick}]

        lappend scaling($w,taxis) $xcrd

        if { $format != "" } {
            set tlabel [clock format $t -format $format]
        } else {
            set tlabel [clock format $t -format "%Y-%m-%d"]
        }
        $w create line $xcrd $ycrd2 $xcrd $ycrd -tag taxis -fill $linecolor
        $w create text $xcrd $ycrd3 -text $tlabel -tag taxis -anchor n \
            -fill $textcolor -font $textfont
        set t [expr {int($t+$ttdelt)}]
    }

    set scaling($w,tdelt) $tdelt
}

# RescalePlot --
#    Partly redraw the XY plot to allow for new axes
# Arguments:
#    w           Name of the canvas
#    xscale      New minimum, maximum and step for x-axis
#    yscale      New minimum, maximum and step for y-axis
# Result:
#    None
# Side effects:
#    Axes redrawn in canvas, data scaled and moved
# Note:
#    Symbols will be scaled as well!
#
proc ::Plotchart::RescalePlot { w xscale yscale } {
    variable scaling

   foreach {xmin xmax xdelt} $xscale {break}
   foreach {ymin ymax ydelt} $yscale {break}

   if { $xdelt == 0.0 || $ydelt == 0.0 } {
      return -code error "Step size can not be zero"
   }

   if { ($xmax-$xmin)*$xdelt < 0.0 } {
      set xdelt [expr {-$xdelt}]
   }
   if { ($ymax-$ymin)*$ydelt < 0.0 } {
      set ydelt [expr {-$ydelt}]
   }

   $w delete xaxis
   $w delete yaxis

   #
   # Zoom in to the new scaling: move and scale the existing data
   #

   foreach {xb  yb}  [coordsToPixel $w  $scaling($w,xmin) $scaling($w,ymin)] {break} ;# Extreme pixels
   foreach {xe  ye}  [coordsToPixel $w  $scaling($w,xmax) $scaling($w,ymax)] {break} ;# Extreme pixels
   foreach {xnb ynb} [coordsToPixel $w  $xmin $ymin] {break} ;# Current pixels of new rectangle
   foreach {xne yne} [coordsToPixel $w  $xmax $ymax] {break}

   set fx [expr {($xe-$xb)/double($xne-$xnb)}]
   set fy [expr {($ye-$yb)/double($yne-$ynb)}]

   $w scale data $xnb $ynb $fx $fy
   $w move  data [expr {$xb-$xnb}] [expr {$yb-$ynb}]

   worldCoordinates $w $xmin  $ymin  $xmax  $ymax

   DrawYaxis        $w $ymin  $ymax  $ydelt
   DrawXaxis        $w $xmin  $xmax  $xdelt
}
