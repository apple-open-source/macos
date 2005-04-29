# plotpriv.tcl --
#    Facilities to draw simple plots in a dedicated canvas
#
# Note:
#    This source file contains the private functions.
#    It is the companion of "plotchart.tcl"
#

# SavePlot --
#    Save the plot/chart to a PostScript file (using default options)
# Arguments:
#    w           Name of the canvas
#    filename    Name of the file to write
# Result:
#    None
# Side effect:
#    A (new) PostScript file
#
proc ::Plotchart::SavePlot { w filename } {
   #
   # Wait for the canvas to become visible - just in case.
   # Then write the file
   #
   update idletasks
   $w postscript -file $filename
}

# MarginsRectangle --
#    Determine the margins for a rectangular plot/chart
# Arguments:
#    w           Name of the canvas
#    notext      Number of lines of text to make room for at the top
#                (default: 2.0)
# Result:
#    List of four values
#
proc ::Plotchart::MarginsRectangle { w {notext 2.0}} {
   set pxmin 80
   set pymin [expr {int(14*$notext)}]
   set pxmax [expr {[$w cget -width]  - 40}]
   set pymax [expr {[$w cget -height] - 30}]

   return [list $pxmin $pymin $pxmax $pymax]
}

# MarginsCircle --
#    Determine the margins for a circular plot/chart
# Arguments:
#    w           Name of the canvas
# Result:
#    List of four values
#
proc ::Plotchart::MarginsCircle { w } {
   set pxmin 80
   set pymin 30
   set pxmax [expr {[$w cget -width]  - 80}]
   set pymax [expr {[$w cget -height] - 30}]

   set dx [expr {$pxmax-$pxmin+1}]
   set dy [expr {$pymax-$pymin+1}]

   if { $dx < $dy } {
      set pyminn [expr {($pymin+$pymax-$dx)/2}]
      set pymaxn [expr {($pymin+$pymax+$dx)/2}]
      set pymin  $pyminn
      set pymax  $pymaxn
   } else {
      set pxminn [expr {($pxmin+$pxmax-$dy)/2}]
      set pxmaxn [expr {($pxmin+$pxmax+$dy)/2}]
      set pxmin  $pxminn
      set pxmax  $pxmaxn
   }

   return [list $pxmin $pymin $pxmax $pymax]
}

# Margins3DPlot --
#    Determine the margins for a 3D plot
# Arguments:
#    w           Name of the canvas
# Result:
#    List of four values
#
proc ::Plotchart::Margins3DPlot { w } {
   variable scaling

   set yfract 0.33
   set zfract 0.50
   if { [info exists scaling($w,yfract)] } {
      set yfract $scaling($w,yfract)
   } else {
      set scaling($w,yfract) $yfract
   }
   if { [info exists scaling($w,zfract)] } {
      set zfract $scaling($w,zfract)
   } else {
      set scaling($w,zfract) $zfract
   }

   set yzwidth  [expr {(-120+[$w cget -width])/(1.0+$yfract)}]
   set yzheight [expr {(-60+[$w cget -height])/(1.0+$zfract)}]

   set pxmin    [expr {60+$yfract*$yzwidth}]
   set pxmax    [expr {[$w cget -width] - 60}]
   set pymin    30
   set pymax    [expr {30+$yzheight}]

   return [list $pxmin $pymin $pxmax $pymax]
}

# SetColours --
#    Set the colours for those plots that treat them as a global resource
# Arguments:
#    w           Name of the canvas
#    args        List of colours to be used
# Result:
#    None
#
proc ::Plotchart::SetColours { w args } {
   variable scaling

   set scaling($w,colours) $args
}

# DataConfig --
#    Configure the data series
# Arguments:
#    w           Name of the canvas
#    series      Name of the series in question
#    args        Option and value pairs
# Result:
#    None
#
proc ::Plotchart::DataConfig { w series args } {
   variable data_series
   variable options
   variable option_keys
   variable option_values

   foreach {option value} $args {
      set idx [lsearch $options $option]
      if { $idx < 0 } {
         return -code error "Unknown or invalid option: $option (value: $value)"
      } else {
         set key [lindex $option_keys    $idx]
         set idx [lsearch $option_values $key]
         set values  [lindex $option_values [incr idx]]
         if { $values != "..." } {
            if { [lsearch $values $value] < 0 } {
               return -code error "Unknown or invalid value: $value for option $option - $values"
            }
         }
         set data_series($w,$series,$key) $value
      }
   }
}

# ScaleIsometric --
#    Determine the scaling for an isometric plot
# Arguments:
#    w           Name of the canvas
#    xmin        Minimum x coordinate
#    ymin        Minimum y coordinate
#    xmax        Maximum x coordinate
#    ymax        Maximum y coordinate
#                (default: 1.5)
# Result:
#    None
# Side effect:
#    Array with scaling parameters set
#
proc ::Plotchart::ScaleIsometric { w xmin ymin xmax ymax } {
   variable scaling

   set pxmin $scaling($w,pxmin)
   set pymin $scaling($w,pymin)
   set pxmax $scaling($w,pxmax)
   set pymax $scaling($w,pymax)

   set dx [expr {($xmax-$xmin)/($pxmax-$pxmin)}]
   set dy [expr {($ymax-$ymin)/($pymax-$pymin)}]

   #
   # Which coordinate is dominant?
   #
   if { $dy < $dx } {
      set yminn [expr {0.5*($ymax+$ymin) - 0.5 * $dx * ($pymax-$pymin)}]
      set ymaxn [expr {0.5*($ymax+$ymin) + 0.5 * $dx * ($pymax-$pymin)}]
      set ymin  $yminn
      set ymax  $ymaxn
   } else {
      set xminn [expr {0.5*($xmax+$xmin) - 0.5 * $dy * ($pxmax-$pxmin)}]
      set xmaxn [expr {0.5*($xmax+$xmin) + 0.5 * $dy * ($pxmax-$pxmin)}]
      set xmin  $xminn
      set xmax  $xmaxn
   }

   worldCoordinates $w $xmin $ymin $xmax $ymax
}

# PlotHandler --
#    Handle the subcommands for an XY plot or chart
# Arguments:
#    type        Type of plot/chart
#    w           Name of the canvas
#    command     Subcommand or method to run
#    args        Data for the command
# Result:
#    Whatever returned by the subcommand
#
proc ::Plotchart::PlotHandler { type w command args } {
   variable methodProc

   if { [info exists methodProc($type,$command)] } {
      eval $methodProc($type,$command) $w $args
   } else {
      return -code error "No such method - $command"
   }
}

# DrawMask --
#    Draw the stuff that masks the data lines outside the graph
# Arguments:
#    w           Name of the canvas
# Result:
#    None
# Side effects:
#    Several polygons drawn in the background colour
#
proc ::Plotchart::DrawMask { w } {
   variable scaling

   set width  [$w cget -width]
   set height [expr {[$w cget -height] + 1}]
   set colour [$w cget -background]
   set pxmin  $scaling($w,pxmin)
   set pxmax  $scaling($w,pxmax)
   set pymin  $scaling($w,pymin)
   set pymax  $scaling($w,pymax)
   $w create rectangle 0      0      $pxmin $height -fill $colour -outline $colour -tag mask
   $w create rectangle 0      0      $width $pymin  -fill $colour -outline $colour -tag mask
   $w create rectangle 0      $pymax $width $height -fill $colour -outline $colour -tag mask
   $w create rectangle $pxmax 0      $width $height -fill $colour -outline $colour -tag mask

   $w lower mask
}

# DrawTitle --
#    Draw the title
# Arguments:
#    w           Name of the canvas
#    title       Title to appear above the graph
# Result:
#    None
# Side effects:
#    Text string drawn
#
proc ::Plotchart::DrawTitle { w title } {
   variable scaling

   set width  [$w cget -width]
   set pymin  $scaling($w,pymin)

   $w create text [expr {$width/2}] 3 -text $title \
       -anchor n -tags title
}

# DrawData --
#    Draw the data in an XY-plot
# Arguments:
#    w           Name of the canvas
#    series      Data series
#    xcrd        Next x coordinate
#    ycrd        Next y coordinate
# Result:
#    None
# Side effects:
#    New data drawn in canvas
#
proc ::Plotchart::DrawData { w series xcrd ycrd } {
   variable data_series
   variable scaling

   #
   # Draw the line piece
   #
   set colour "black"
   if { [info exists data_series($w,$series,-colour)] } {
      set colour $data_series($w,$series,-colour)
   }

   set type "line"
   if { [info exists data_series($w,$series,-type)] } {
      set type $data_series($w,$series,-type)
   }

   foreach {pxcrd pycrd} [coordsToPixel $w $xcrd $ycrd] {break}

   if { [info exists data_series($w,$series,x)] } {
      set xold $data_series($w,$series,x)
      set yold $data_series($w,$series,y)
      foreach {pxold pyold} [coordsToPixel $w $xold $yold] {break}
      if { $type == "line" || $type == "both" } {
         $w create line $pxold $pyold $pxcrd $pycrd \
                        -fill $colour -tag data
      }
   }

   if { $type == "symbol" || $type == "both" } {
      set symbol "dot"
      if { [info exists data_series($w,$series,-symbol)] } {
         set symbol $data_series($w,$series,-symbol)
      }
      DrawSymbolPixel $w $series $pxcrd $pycrd $symbol $colour
   }

   $w lower data

   set data_series($w,$series,x) $xcrd
   set data_series($w,$series,y) $ycrd
}

# DrawStripData --
#    Draw the data in a stripchart
# Arguments:
#    w           Name of the canvas
#    series      Data series
#    xcrd        Next x coordinate
#    ycrd        Next y coordinate
# Result:
#    None
# Side effects:
#    New data drawn in canvas
#
proc ::Plotchart::DrawStripData { w series xcrd ycrd } {
   variable data_series
   variable scaling

   if { $xcrd > $scaling($w,xmax) } {
      set xdelt $scaling($w,xdelt)
      set xmin  $scaling($w,xmin)
      set xmax  $scaling($w,xmax)

      set xminorg $xmin
      while { $xmax < $xcrd } {
         set xmin [expr {$xmin+$xdelt}]
         set xmax [expr {$xmax+$xdelt}]
      }
      set ymin  $scaling($w,ymin)
      set ymax  $scaling($w,ymax)

      worldCoordinates $w $xmin $ymin $xmax $ymax
      DrawXaxis $w $xmin $xmax $xdelt

      foreach {pxminorg pyminorg} [coordsToPixel $w $xminorg $ymin] {break}
      foreach {pxmin pymin}       [coordsToPixel $w $xmin    $ymin] {break}
      $w move data [expr {$pxminorg-$pxmin+1}] 0
   }

   DrawData $w $series $xcrd $ycrd
}

# DrawSymbolPixel --
#    Draw a symbol in an xy-plot, polar plot or stripchart
# Arguments:
#    w           Name of the canvas
#    series      Data series
#    pxcrd       Next x (pixel) coordinate
#    pycrd       Next y (pixel) coordinate
#    symbol      What symbol to draw
#    colour      What colour to use
# Result:
#    None
# Side effects:
#    New symbol drawn in canvas
#
proc ::Plotchart::DrawSymbolPixel { w series pxcrd pycrd symbol colour } {
   variable data_series
   variable scaling

   set pxmin  [expr {$pxcrd-4}]
   set pxmax  [expr {$pxcrd+4}]
   set pymin  [expr {$pycrd-4}]
   set pymax  [expr {$pycrd+4}]

   switch -- $symbol {
   "plus"     { $w create line $pxmin $pycrd $pxmax $pycrd \
                               $pxcrd $pycrd $pxcrd $pymin \
                               $pxcrd $pymax \
                               -fill $colour -tag data \
                               -capstyle projecting
              }
   "cross"    { $w create line $pxmin $pymin $pxmax $pymax \
                               $pxcrd $pycrd $pxmax $pymin \
                               $pxmin $pymax \
                               -fill $colour -tag data \
                               -capstyle projecting
              }
   "circle"   { $w create oval $pxmin $pymin $pxmax $pymax \
                               -outline $colour -tag data
              }
   "dot"      { $w create oval $pxmin $pymin $pxmax $pymax \
                               -outline $colour -fill $colour -tag data
              }
   "up"       { $w create polygon $pxmin $pymax $pxmax $pymax \
                               $pxcrd $pymin \
                               -outline $colour -fill {} -tag data
              }
   "upfilled" { $w create polygon $pxmin $pymax $pxmax $pymax \
                              $pxcrd $pymin \
                              -outline $colour -fill $colour -tag data
              }
   "down"     { $w create polygon $pxmin $pymin $pxmax $pymin \
                              $pxcrd $pymax \
                              -outline $colour -fill {} -tag data
              }
   "downfilled" { $w create polygon $pxmin $pymin $pxmax $pymin \
                              $pxcrd $pymax \
                              -outline $colour -fill $colour -tag data
              }
   }
}

# DrawPie --
#    Draw the pie
# Arguments:
#    w           Name of the canvas
#    data        Data series (pairs of label-value)
# Result:
#    None
# Side effects:
#    Pie filled
#
proc ::Plotchart::DrawPie { w data } {
   variable data_series
   variable scaling

   set pxmin $scaling($w,pxmin)
   set pymin $scaling($w,pymin)
   set pxmax $scaling($w,pxmax)
   set pymax $scaling($w,pymax)

   #
   # Determine the scale for the values
   # (so we can draw the correct angles)
   #
   set sum 0.0
   foreach {label value} $data {
      set sum [expr {$sum + $value}]
   }
   set factor [expr {360.0/$sum}]

   #
   # Draw the line piece
   #
   set angle_bgn 0.0
   set angle_ext 0.0
   set sum       0.0

   set colours $scaling($w,colours)

   set idx 0
   foreach {label value} $data {
      set colour [lindex $colours $idx]
      incr idx

      set angle_bgn [expr {$sum   * $factor}]
      set angle_ext [expr {$value * $factor}]

      $w create arc  $pxmin $pymin $pxmax $pymax \
                     -start $angle_bgn -extent $angle_ext \
                     -fill $colour -style pieslice

      set rad   [expr {($angle_bgn+0.5*$angle_ext)*3.1415926/180.0}]
      set xtext [expr {($pxmin+$pxmax+cos($rad)*($pxmax-$pxmin+20))/2}]
      set ytext [expr {($pymin+$pymax-sin($rad)*($pymax-$pymin+20))/2}]
      if { $xtext > ($pxmin+$pymax)/2 } {
         set dir w
      } else {
         set dir e
      }

      $w create text $xtext $ytext -text $label -anchor $dir

      set sum [expr {$sum + $value}]
   }
}

# DrawPolarData --
#    Draw data given in polar coordinates
# Arguments:
#    w           Name of the canvas
#    series      Data series
#    rad         Next radius
#    phi         Next angle (in degrees)
# Result:
#    None
# Side effects:
#    Data drawn in canvas
#
proc ::Plotchart::DrawPolarData { w series rad phi } {
   variable torad
   set xcrd [expr {$rad*cos($phi*$torad)}]
   set ycrd [expr {$rad*sin($phi*$torad)}]

   DrawData $w $series $xcrd $ycrd
}

# DrawVertBarData --
#    Draw the vertical bars
# Arguments:
#    w           Name of the canvas
#    series      Data series
#    ydata       Series of y data
#    colour      The colour to use (optional)
# Result:
#    None
# Side effects:
#    Data bars drawn in canvas
#
proc ::Plotchart::DrawVertBarData { w series ydata {colour black}} {
   variable data_series
   variable scaling

   #
   # Draw the bars
   #
   set x $scaling($w,xbase)

   set newbase {}

   foreach yvalue $ydata ybase $scaling($w,ybase) {
      set xnext [expr {$x+$scaling($w,barwidth)}]
      set y     [expr {$yvalue+$ybase}]
      foreach {px1 py1} [coordsToPixel $w $x     $ybase] {break}
      foreach {px2 py2} [coordsToPixel $w $xnext $y    ] {break}
      $w create rectangle $px1 $py1 $px2 $py2 \
                     -fill $colour -tag data
      $w lower data

      set x [expr {$x+1.0}]

      lappend newbase $y
   }

   #
   # Prepare for the next series
   #
   if { $scaling($w,stacked) } {
      set scaling($w,ybase) $newbase
   }

   set scaling($w,xbase) [expr {$scaling($w,xbase)+$scaling($w,xshift)}]
}

# DrawHorizBarData --
#    Draw the horizontal bars
# Arguments:
#    w           Name of the canvas
#    series      Data series
#    xdata       Series of x data
#    colour      The colour to use (optional)
# Result:
#    None
# Side effects:
#    Data bars drawn in canvas
#
proc ::Plotchart::DrawHorizBarData { w series xdata {colour black}} {
   variable data_series
   variable scaling

   #
   # Draw the bars
   #
   set y $scaling($w,ybase)

   set newbase {}

   foreach xvalue $xdata xbase $scaling($w,xbase) {
      set ynext [expr {$y+$scaling($w,barwidth)}]
      set x     [expr {$xvalue+$xbase}]
      foreach {px1 py1} [coordsToPixel $w $xbase $y    ] {break}
      foreach {px2 py2} [coordsToPixel $w $x     $ynext] {break}
      $w create rectangle $px1 $py1 $px2 $py2 \
                     -fill $colour -tag data
      $w lower data

      set y [expr {$y+1.0}]

      lappend newbase $x
   }

   #
   # Prepare for the next series
   #
   if { $scaling($w,stacked) } {
      set scaling($w,xbase) $newbase
   }

   set scaling($w,ybase) [expr {$scaling($w,ybase)+$scaling($w,yshift)}]
}

# DrawTimePeriod --
#    Draw a period
# Arguments:
#    w           Name of the canvas
#    text        Text to identify the "period" item
#    time_begin  Start time
#    time_end    Stop time
#    colour      The colour to use (optional)
# Result:
#    None
# Side effects:
#    Data bars drawn in canvas
#
proc ::Plotchart::DrawTimePeriod { w text time_begin time_end {colour black}} {
   variable data_series
   variable scaling

   #
   # Draw the text first
   #
   set ytext [expr {$scaling($w,current)+0.5*$scaling($w,dy)}]
   foreach {x y} [coordsToPixel $w $scaling($w,xmin) $ytext] {break}

   $w create text 5 $y -text $text -anchor w

   #
   # Draw the bar to indicate the period
   #
   set xmin  [clock scan $time_begin]
   set xmax  [clock scan $time_end]
   set ybott [expr {$scaling($w,current)+$scaling($w,dy)}]

   foreach {x1 y1} [coordsToPixel $w $xmin $scaling($w,current)] {break}
   foreach {x2 y2} [coordsToPixel $w $xmax $ybott              ] {break}

   $w create rectangle $x1 $y1 $x2 $y2 -fill $colour

   set scaling($w,current) [expr {$scaling($w,current)-1.0}]
}

# DrawTimeVertLine --
#    Draw a vertical line with a label
# Arguments:
#    w           Name of the canvas
#    text        Text to identify the line
#    time        Time for which the line is drawn
# Result:
#    None
# Side effects:
#    Line drawn in canvas
#
proc ::Plotchart::DrawTimeVertLine { w text time {colour black}} {
   variable data_series
   variable scaling

   #
   # Draw the text first
   #
   set xtime [clock scan $time]
   set ytext [expr {$scaling($w,ymax)-0.5*$scaling($w,dy)}]
   foreach {x y} [coordsToPixel $w $xtime $ytext] {break}

   $w create text $x $y -text $text -anchor w

   #
   # Draw the line
   #
   foreach {x1 y1} [coordsToPixel $w $xtime $scaling($w,ymin)] {break}
   foreach {x2 y2} [coordsToPixel $w $xtime $scaling($w,ymax)] {break}

   $w create line $x1 $y1 $x2 $y2 -fill black
}

# DrawTimeMilestone --
#    Draw a "milestone"
# Arguments:
#    w           Name of the canvas
#    text        Text to identify the line
#    time        Time for which the milestone is drawn
#    colour      Optionally the colour
# Result:
#    None
# Side effects:
#    Line drawn in canvas
#
proc ::Plotchart::DrawTimeMilestone { w text time {colour black}} {
   variable data_series
   variable scaling

   #
   # Draw the text first
   #
   set ytext [expr {$scaling($w,current)+0.5*$scaling($w,dy)}]
   foreach {x y} [coordsToPixel $w $scaling($w,xmin) $ytext] {break}

   $w create text 5 $y -text $text -anchor w

   #
   # Draw an upside-down triangle to indicate the time
   #
   set xcentre [clock scan $time]
   set ytop    $scaling($w,current)
   set ybott   [expr {$scaling($w,current)+0.8*$scaling($w,dy)}]

   foreach {x1 y1} [coordsToPixel $w $xcentre $ybott] {break}
   foreach {x2 y2} [coordsToPixel $w $xcentre $ytop]  {break}

   set x2 [expr {$x1-0.4*($y1-$y2)}]
   set x3 [expr {$x1+0.4*($y1-$y2)}]
   set y3 $y2

   $w create polygon $x1 $y1 $x2 $y2 $x3 $y3 -fill $colour

   set scaling($w,current) [expr {$scaling($w,current)-1.0}]
}

# ScaleItems --
#    Scale all items by a given factor
# Arguments:
#    w           Name of the canvas
#    xcentre     X-coordinate of centre
#    ycentre     Y-coordinate of centre
#    factor      The factor to scale them by
# Result:
#    None
# Side effects:
#    All items are scaled by the given factor and the
#    world coordinates are adjusted.
#
proc ::Plotchart::ScaleItems { w xcentre ycentre factor } {
   variable scaling

   $w scale all $xcentre $ycentre $factor $factor

   foreach {xc yc} [pixelToCoords $w $xcentre $ycentre] {break}

   set rfact               [expr {1.0/$factor}]
   set scaling($w,xfactor) [expr {$scaling($w,xfactor)*$factor}]
   set scaling($w,yfactor) [expr {$scaling($w,yfactor)*$factor}]
   set scaling($w,xmin)    [expr {(1.0-$rfact)*$xc+$rfact*$scaling($w,xmin)}]
   set scaling($w,xmax)    [expr {(1.0-$rfact)*$xc+$rfact*$scaling($w,xmax)}]
   set scaling($w,ymin)    [expr {(1.0-$rfact)*$yc+$rfact*$scaling($w,ymin)}]
   set scaling($w,ymax)    [expr {(1.0-$rfact)*$yc+$rfact*$scaling($w,ymax)}]
}

# MoveItems --
#    Move all items by a given vector
# Arguments:
#    w           Name of the canvas
#    xmove       X-coordinate of move vector
#    ymove       Y-coordinate of move vector
# Result:
#    None
# Side effects:
#    All items are moved by the given vector and the
#    world coordinates are adjusted.
#
proc ::Plotchart::MoveItems { w xmove ymove } {
   variable scaling

   $w move all $xmove $ymove

   set dx                  [expr {$scaling($w,xfactor)*$xmove}]
   set dy                  [expr {$scaling($w,yfactor)*$ymove}]
   set scaling($w,xmin)    [expr {$scaling($w,xmin)+$dx}]
   set scaling($w,xmax)    [expr {$scaling($w,xmax)+$dx}]
   set scaling($w,ymin)    [expr {$scaling($w,ymin)+$dy}]
   set scaling($w,ymax)    [expr {$scaling($w,ymax)+$dy}]
}

# DrawIsometricData --
#    Draw the data in an isometric plot
# Arguments:
#    w           Name of the canvas
#    type        Type of data
#    args        Coordinates and so on
# Result:
#    None
# Side effects:
#    New data drawn in canvas
#
proc ::Plotchart::DrawIsometricData { w type args } {
   variable data_series

   #
   # What type of data?
   #
   if { $type == "rectangle" } {
      foreach {x1 y1 x2 y2 colour} [concat $args "black"] {break}
      foreach {px1 py1} [coordsToPixel $w $x1 $y1] {break}
      foreach {px2 py2} [coordsToPixel $w $x2 $y2] {break}
      $w create rectangle $px1 $py1 $px2 $py2 \
                     -outline $colour -tag data
      $w lower data
   }

   if { $type == "filled-rectangle" } {
      foreach {x1 y1 x2 y2 colour} [concat $args "black"] {break}
      foreach {px1 py1} [coordsToPixel $w $x1 $y1] {break}
      foreach {px2 py2} [coordsToPixel $w $x2 $y2] {break}
      $w create rectangle $px1 $py1 $px2 $py2 \
                     -outline $colour -fill $colour -tag data
      $w lower data
   }

   if { $type == "filled-circle" } {
      foreach {x1 y1 rad colour} [concat $args "black"] {break}
      set x2 [expr {$x1+$rad}]
      set y2 [expr {$y1+$rad}]
      set x1 [expr {$x1-$rad}]
      set y1 [expr {$y1-$rad}]
      foreach {px1 py1} [coordsToPixel $w $x1 $y1] {break}
      foreach {px2 py2} [coordsToPixel $w $x2 $y2] {break}
      $w create oval $px1 $py1 $px2 $py2 \
                     -outline $colour -fill $colour -tag data
      $w lower data
   }

   if { $type == "circle" } {
      foreach {x1 y1 rad colour} [concat $args "black"] {break}
      set x2 [expr {$x1+$rad}]
      set y2 [expr {$y1+$rad}]
      set x1 [expr {$x1-$rad}]
      set y1 [expr {$y1-$rad}]
      foreach {px1 py1} [coordsToPixel $w $x1 $y1] {break}
      foreach {px2 py2} [coordsToPixel $w $x2 $y2] {break}
      $w create oval $px1 $py1 $px2 $py2 \
                     -outline $colour -tag data
      $w lower data
   }

}
