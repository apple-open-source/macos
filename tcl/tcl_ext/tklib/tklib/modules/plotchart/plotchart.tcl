# plotchart.tcl --
#    Facilities to draw simple plots in a dedicated canvas
#
# Note:
#    This source file contains the public functions.
#    The others are contained in "plotpriv.tcl"
#

# Plotchart --
#    Namespace to hold the procedures and the private data
#
namespace eval ::Plotchart {
   variable scaling
   variable methodProc
   variable data_series

   namespace export worldCoordinates viewPort coordsToPixel \
                    polarCoordinates setZoomPan \
                    createXYPlot createPolarPlot createPiechart \
                    createBarchart createHorizontalBarchart \
                    createTimechart createStripchart \
                    createIsometricPlot create3DPlot

   #
   # Array linking procedures with methods
   #
   set methodProc(xyplot,title)          DrawTitle
   set methodProc(xyplot,xtext)          DrawXtext
   set methodProc(xyplot,ytext)          DrawYtext
   set methodProc(xyplot,plot)           DrawData
   set methodProc(xyplot,saveplot)       SavePlot
   set methodProc(xyplot,dataconfig)     DataConfig
   set methodProc(xyplot,xconfig)        XConfig
   set methodProc(xyplot,yconfig)        YConfig
   set methodProc(piechart,title)        DrawTitle
   set methodProc(piechart,plot)         DrawPie
   set methodProc(piechart,saveplot)     SavePlot
   set methodProc(polarplot,title)       DrawTitle
   set methodProc(polarplot,plot)        DrawPolarData
   set methodProc(polarplot,saveplot)    SavePlot
   set methodProc(polarplot,dataconfig)  DataConfig
   set methodProc(horizbars,title)       DrawTitle
   set methodProc(horizbars,xtext)       DrawXtext
   set methodProc(horizbars,ytext)       DrawYtext
   set methodProc(horizbars,plot)        DrawHorizBarData
   set methodProc(horizbars,saveplot)    SavePlot
   set methodProc(horizbars,colours)     SetColours
   set methodProc(horizbars,colors)      SetColours
   set methodProc(horizbars,xconfig)     XConfig
   set methodProc(vertbars,title)        DrawTitle
   set methodProc(vertbars,xtext)        DrawXtext
   set methodProc(vertbars,ytext)        DrawYtext
   set methodProc(vertbars,plot)         DrawVertBarData
   set methodProc(vertbars,saveplot)     SavePlot
   set methodProc(vertbars,colours)      SetColours
   set methodProc(vertbars,colors)       SetColours
   set methodProc(vertbars,yconfig)      YConfig
   set methodProc(timechart,title)       DrawTitle
   set methodProc(timechart,period)      DrawTimePeriod
   set methodProc(timechart,milestone)   DrawTimeMilestone
   set methodProc(timechart,vertline)    DrawTimeVertLine
   set methodProc(timechart,saveplot)    SavePlot
   set methodProc(stripchart,title)      DrawTitle
   set methodProc(stripchart,xtext)      DrawXtext
   set methodProc(stripchart,ytext)      DrawYtext
   set methodProc(stripchart,plot)       DrawStripData
   set methodProc(stripchart,saveplot)   SavePlot
   set methodProc(stripchart,dataconfig) DataConfig
   set methodProc(stripchart,xconfig)    XConfig
   set methodProc(stripchart,yconfig)    YConfig
   set methodProc(isometric,title)       DrawTitle
   set methodProc(isometric,xtext)       DrawXtext
   set methodProc(isometric,ytext)       DrawYtext
   set methodProc(isometric,plot)        DrawIsometricData
   set methodProc(isometric,saveplot)    SavePlot
   set methodProc(3dplot,title)          DrawTitle
   set methodProc(3dplot,plotfunc)       Draw3DFunction
   set methodProc(3dplot,plotdata)       Draw3DData
   set methodProc(3dplot,gridsize)       GridSize3D
   set methodProc(3dplot,saveplot)       SavePlot
   set methodProc(3dplot,colour)         SetColours
   set methodProc(3dplot,color)          SetColours
   set methodProc(3dplot,xconfig)        XConfig
   set methodProc(3dplot,yconfig)        YConfig
   set methodProc(3dplot,zconfig)        ZConfig

   #
   # Auxiliary parameters
   #
   variable torad
   set torad [expr {3.1415926/180.0}]

   variable options
   variable option_keys
   variable option_values
   set options       {-colour -color  -symbol -type}
   set option_keys   {-colour -colour -symbol -type}
   set option_values {-colour {...}
                      -symbol {plus cross circle up down dot upfilled downfilled}
                      -type {line symbol both}
                     }

   variable axis_options
   variable axis_option_clear
   variable axis_option_values
   set axis_options       {-format -ticklength -ticklines -scale}
   set axis_option_clear  { 0       0           0          1    }
   set axis_option_values {-format     {...}
                           -ticklength {...}
                           -ticklines  {0 1}
                           -scale      {...}
                          }
}

# setZoomPan --
#    Set up the bindings for zooming and panning
# Arguments:
#    w           Name of the canvas window
# Result:
#    None
# Side effect:
#    Bindings set up
#
proc ::Plotchart::setZoomPan { w } {
   set sqrt2  [expr {sqrt(2.0)}]
   set sqrt05 [expr {sqrt(0.5)}]

   bind $w <Control-Button-1> [list ::Plotchart::ScaleItems $w %x %y $sqrt2]
   bind $w <Control-Prior>    [list ::Plotchart::ScaleItems $w %x %y $sqrt2]
   bind $w <Control-Button-2> [list ::Plotchart::ScaleItems $w %x %y $sqrt05]
   bind $w <Control-Button-3> [list ::Plotchart::ScaleItems $w %x %y $sqrt05]
   bind $w <Control-Next>     [list ::Plotchart::ScaleItems $w %x %y $sqrt05]
   bind $w <Control-Up>       [list ::Plotchart::MoveItems  $w   0 -40]
   bind $w <Control-Down>     [list ::Plotchart::MoveItems  $w   0  40]
   bind $w <Control-Left>     [list ::Plotchart::MoveItems  $w -40   0]
   bind $w <Control-Right>    [list ::Plotchart::MoveItems  $w  40   0]
   focus $w
}

# viewPort --
#    Set the pixel extremes for the graph
# Arguments:
#    w           Name of the canvas window
#    pxmin       Minimum X-coordinate
#    pymin       Minimum Y-coordinate
#    pxmax       Maximum X-coordinate
#    pymax       Maximum Y-coordinate
# Result:
#    None
# Side effect:
#    Array scaling filled
#
proc ::Plotchart::viewPort { w pxmin pymin pxmax pymax } {
   variable scaling

   if { $pxmin >= $pxmax || $pymin >= $pymax } {
      return -code error "Inconsistent bounds for viewport"
   }

   set scaling($w,pxmin)    $pxmin
   set scaling($w,pymin)    $pymin
   set scaling($w,pxmax)    $pxmax
   set scaling($w,pymax)    $pymax
   set scaling($w,new)      1
}

# worldCoordinates --
#    Set the extremes for the world coordinates
# Arguments:
#    w           Name of the canvas window
#    xmin        Minimum X-coordinate
#    ymin        Minimum Y-coordinate
#    xmax        Maximum X-coordinate
#    ymax        Maximum Y-coordinate
# Result:
#    None
# Side effect:
#    Array scaling filled
#
proc ::Plotchart::worldCoordinates { w xmin ymin xmax ymax } {
   variable scaling

   if { $xmin == $xmax || $ymin == $ymax } {
      return -code error "Minimum and maximum must differ for world coordinates"
   }

   set scaling($w,xmin)    [expr {double($xmin)}]
   set scaling($w,ymin)    [expr {double($ymin)}]
   set scaling($w,xmax)    [expr {double($xmax)}]
   set scaling($w,ymax)    [expr {double($ymax)}]

   set scaling($w,new)     1
}

# polarCoordinates --
#    Set the extremes for the polar coordinates
# Arguments:
#    w           Name of the canvas window
#    radmax      Maximum radius
# Result:
#    None
# Side effect:
#    Array scaling filled
#
proc ::Plotchart::polarCoordinates { w radmax } {
   variable scaling

   if { $radmax <= 0.0 } {
      return -code error "Maximum radius must be positive"
   }

   set scaling($w,xmin)    [expr {-double($radmax)}]
   set scaling($w,ymin)    [expr {-double($radmax)}]
   set scaling($w,xmax)    [expr {double($radmax)}]
   set scaling($w,ymax)    [expr {double($radmax)}]

   set scaling($w,new)     1
}

# world3DCoordinates --
#    Set the extremes for the world coordinates in 3D plots
# Arguments:
#    w           Name of the canvas window
#    xmin        Minimum X-coordinate
#    ymin        Minimum Y-coordinate
#    zmin        Minimum Z-coordinate
#    xmax        Maximum X-coordinate
#    ymax        Maximum Y-coordinate
#    zmax        Maximum Z-coordinate
# Result:
#    None
# Side effect:
#    Array scaling filled
#
proc ::Plotchart::world3DCoordinates { w xmin ymin zmin xmax ymax zmax } {
   variable scaling

   if { $xmin == $xmax || $ymin == $ymax || $zmin == $zmax } {
      return -code error "Minimum and maximum must differ for world coordinates"
   }

   set scaling($w,xmin)    [expr {double($xmin)}]
   set scaling($w,ymin)    [expr {double($ymin)}]
   set scaling($w,zmin)    [expr {double($zmin)}]
   set scaling($w,xmax)    [expr {double($xmax)}]
   set scaling($w,ymax)    [expr {double($ymax)}]
   set scaling($w,zmax)    [expr {double($zmax)}]

   set scaling($w,new)     1
}

# coordsToPixel --
#    Convert world coordinates to pixel coordinates
# Arguments:
#    w           Name of the canvas
#    xcrd        X-coordinate
#    ycrd        Y-coordinate
# Result:
#    List of two elements, x- and y-coordinates in pixels
#
proc ::Plotchart::coordsToPixel { w xcrd ycrd } {
   variable scaling

   if { $scaling($w,new) == 1 } {
      set scaling($w,new)     0
      set width               [expr {$scaling($w,pxmax)-$scaling($w,pxmin)}]
      set height              [expr {$scaling($w,pymax)-$scaling($w,pymin)}]

      set dx                  [expr {$scaling($w,xmax)-$scaling($w,xmin)}]
      set dy                  [expr {$scaling($w,ymax)-$scaling($w,ymin)}]
      set scaling($w,xfactor) [expr {$width/$dx}]
      set scaling($w,yfactor) [expr {$height/$dy}]
   }

   set xpix [expr {$scaling($w,pxmin)+($xcrd-$scaling($w,xmin))*$scaling($w,xfactor)}]
   set ypix [expr {$scaling($w,pymin)+($scaling($w,ymax)-$ycrd)*$scaling($w,yfactor)}]
   return [list $xpix $ypix]
}

# coords3DToPixel --
#    Convert world coordinates to pixel coordinates (3D plots)
# Arguments:
#    w           Name of the canvas
#    xcrd        X-coordinate
#    ycrd        Y-coordinate
#    zcrd        Z-coordinate
# Result:
#    List of two elements, x- and y-coordinates in pixels
#
proc ::Plotchart::coords3DToPixel { w xcrd ycrd zcrd } {
   variable scaling

   if { $scaling($w,new) == 1 } {
      set scaling($w,new)      0
      set width                [expr {$scaling($w,pxmax)-$scaling($w,pxmin)}]
      set height               [expr {$scaling($w,pymax)-$scaling($w,pymin)}]

      set dx                   [expr {$scaling($w,xmax)-$scaling($w,xmin)}]
      set dy                   [expr {$scaling($w,ymax)-$scaling($w,ymin)}]
      set dz                   [expr {$scaling($w,zmax)-$scaling($w,zmin)}]
      set scaling($w,xyfactor) [expr {$scaling($w,yfract)*$width/$dx}]
      set scaling($w,xzfactor) [expr {$scaling($w,zfract)*$height/$dx}]
      set scaling($w,yfactor)  [expr {$width/$dy}]
      set scaling($w,zfactor)  [expr {$height/$dz}]
   }

   #
   # The values for xcrd = xmax
   #
   set xpix [expr {$scaling($w,pxmin)+($ycrd-$scaling($w,ymin))*$scaling($w,yfactor)}]
   set ypix [expr {$scaling($w,pymin)+($scaling($w,zmax)-$zcrd)*$scaling($w,zfactor)}]

   #
   # Add the shift due to xcrd-xmax
   #
   set xpix [expr {$xpix + $scaling($w,xyfactor)*($xcrd-$scaling($w,xmax))}]
   set ypix [expr {$ypix - $scaling($w,xzfactor)*($xcrd-$scaling($w,xmax))}]

   return [list $xpix $ypix]
}

# pixelToCoords --
#    Convert pixel coordinates to world coordinates
# Arguments:
#    w           Name of the canvas
#    xpix        X-coordinate (pixel)
#    ypix        Y-coordinate (pixel)
# Result:
#    List of two elements, x- and y-coordinates in world coordinate system
#
proc ::Plotchart::pixelToCoords { w xpix ypix } {
   variable scaling

   if { $scaling($w,new) == 1 } {
      set scaling($w,new)     0
      set width               [expr {$scaling($w,pxmax)-$scaling($w,pxmin)}]
      set height              [expr {$scaling($w,pymax)-$scaling($w,pymin)}]

      set dx                  [expr {$scaling($w,xmax)-$scaling($w,xmin)}]
      set dy                  [expr {$scaling($w,ymax)-$scaling($w,ymin)}]
      set scaling($w,xfactor) [expr {$width/$dx}]
      set scaling($w,yfactor) [expr {$height/$dy}]
   }

   set xcrd [expr {$scaling($w,xmin)+($xpix-$scaling($w,pxmin))/$scaling($w,xfactor)}]
   set ycrd [expr {$scaling($w,ymax)-($ypix-$scaling($w,pymin))/$scaling($w,yfactor)}]
   return [list $xcrd $ycrd]
}

# polarToPixel --
#    Convert polar coordinates to pixel coordinates
# Arguments:
#    w           Name of the canvas
#    rad         Radius of the point
#    phi         Angle of the point (degrees)
# Result:
#    List of two elements, x- and y-coordinates in pixels
#
proc ::Plotchart::polarToPixel { w rad phi } {
   variable torad

   set xcrd [expr {$rad*cos($phi*$torad)}]
   set ycrd [expr {$rad*sin($phi*$torad)}]

   coordsToPixel $w $xcrd $ycrd
}

# createXYPlot --
#    Create a command for drawing an XY plot
# Arguments:
#    w           Name of the canvas
#    xscale      Minimum, maximum and step for x-axis (initial)
#    yscale      Minimum, maximum and step for y-axis
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the XY plot.
#    The plot will be drawn with axes
#
proc ::Plotchart::createXYPlot { w xscale yscale } {
   variable data_series

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "xyplot_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler xyplot $w

   foreach {pxmin pymin pxmax pymax} [MarginsRectangle $w] {break}

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

   viewPort         $w $pxmin $pymin $pxmax $pymax
   worldCoordinates $w $xmin  $ymin  $xmax  $ymax

   DrawYaxis        $w $ymin  $ymax  $ydelt
   DrawXaxis        $w $xmin  $xmax  $xdelt
   DrawMask         $w

   return $newchart
}

# createStripchart --
#    Create a command for drawing a strip chart
# Arguments:
#    w           Name of the canvas
#    xscale      Minimum, maximum and step for x-axis (initial)
#    yscale      Minimum, maximum and step for y-axis
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the stripchart.
#    The stripchart will be drawn with axes
#
proc ::Plotchart::createStripchart { w xscale yscale } {
   variable data_series

   set newchart [createXYPlot $w $xscale $yscale]

   interp alias {} $newchart {}

   set newchart "stripchart_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler stripchart $w

   return $newchart
}

# createIsometricPlot --
#    Create a command for drawing an "isometric" plot
# Arguments:
#    w           Name of the canvas
#    xscale      Minimum and maximum for x-axis
#    yscale      Minimum and maximum for y-axis
#    stepsize    Step size for numbers on the axes or "noaxes"
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the plot
#    The plot will be drawn with or without axes
#
proc ::Plotchart::createIsometricPlot { w xscale yscale stepsize } {
   variable data_series

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "isometric_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler isometric $w

   if { $stepsize != "noaxes" } {
      foreach {pxmin pymin pxmax pymax} [MarginsRectangle $w] {break}
   } else {
      set pxmin 0
      set pymin 0
      set pxmax [$w cget -width]
      set pymax [$w cget -height]
   }

   foreach {xmin xmax} $xscale {break}
   foreach {ymin ymax} $yscale {break}

   if { $xmin == $xmax || $ymin == $ymax } {
      return -code error "Extremes for axes must be different"
   }

   viewPort         $w $pxmin $pymin $pxmax $pymax
   ScaleIsometric   $w $xmin  $ymin  $xmax  $ymax

   if { $stepsize != "noaxes" } {
      DrawYaxis        $w $ymin  $ymax  $ydelt
      DrawXaxis        $w $xmin  $xmax  $xdelt
      DrawMask         $w
   }

   return $newchart
}

# createPiechart --
#    Create a command for drawing a pie chart
# Arguments:
#    w           Name of the canvas
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the pie chart.
#
proc ::Plotchart::createPiechart { w } {
   variable data_series

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "piechart_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler piechart $w

   foreach {pxmin pymin pxmax pymax} [MarginsCircle $w] {break}

   viewPort $w $pxmin $pymin $pxmax $pymax
   $w create oval $pxmin $pymin $pxmax $pymax

   SetColours $w blue lightblue green yellow orange red magenta brown

   return $newchart
}

# createPolarplot --
#    Create a command for drawing a polar plot
# Arguments:
#    w             Name of the canvas
#    radius_data   Maximum radius and step
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the polar plot
#    Possible additional arguments (optional): nautical/mathematical
#    step in phi
#
proc ::Plotchart::createPolarplot { w radius_data } {
   variable data_series

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "polarplot_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler polarplot $w

   set rad_max   [lindex $radius_data 0]
   set rad_step  [lindex $radius_data 1]

   if { $rad_step <= 0.0 } {
      return -code error "Step size can not be zero or negative"
   }
   if { $rad_max <= 0.0 } {
      return -code error "Maximum radius can not be zero or negative"
   }

   foreach {pxmin pymin pxmax pymax} [MarginsCircle $w] {break}

   viewPort         $w $pxmin     $pymin     $pxmax   $pymax
   polarCoordinates $w $rad_max
   DrawPolarAxes    $w $rad_max   $rad_step

   return $newchart
}

# createBarchart --
#    Create a command for drawing a barchart with vertical bars
# Arguments:
#    w           Name of the canvas
#    xlabels     List of labels for x-axis
#    yscale      Minimum, maximum and step for y-axis
#    noseries    Number of series or the keyword "stacked"
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the barchart.
#
proc ::Plotchart::createBarchart { w xlabels yscale noseries } {
   variable data_series

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "barchart_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler vertbars $w

   foreach {pxmin pymin pxmax pymax} [MarginsRectangle $w] {break}

   set xmin  0.0
   set xmax  [expr {[llength $xlabels] + 0.1}]

   foreach {ymin ymax ydelt} $yscale {break}

   if { $ydelt == 0.0 } {
      return -code error "Step size can not be zero"
   }

   if { ($ymax-$ymin)*$ydelt < 0.0 } {
      set ydelt [expr {-$ydelt}]
   }

   viewPort         $w $pxmin $pymin $pxmax $pymax
   worldCoordinates $w $xmin  $ymin  $xmax  $ymax

   DrawYaxis        $w $ymin  $ymax  $ydelt
   DrawXlabels      $w $xlabels $noseries
   DrawMask         $w

   SetColours $w blue lightblue green yellow orange red magenta brown

   return $newchart
}

# createHorizontalBarchart --
#    Create a command for drawing a barchart with horizontal bars
# Arguments:
#    w           Name of the canvas
#    xscale      Minimum, maximum and step for x-axis
#    ylabels     List of labels for y-axis
#    noseries    Number of series or the keyword "stacked"
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the barchart.
#
proc ::Plotchart::createHorizontalBarchart { w xscale ylabels noseries } {
   variable data_series

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "hbarchart_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler horizbars $w

   foreach {pxmin pymin pxmax pymax} [MarginsRectangle $w] {break}

   set ymin  0.0
   set ymax  [expr {[llength $ylabels] + 0.1}]

   foreach {xmin xmax xdelt} $xscale {break}

   if { $xdelt == 0.0 } {
      return -code error "Step size can not be zero"
   }

   if { ($xmax-$xmin)*$xdelt < 0.0 } {
      set xdelt [expr {-$xdelt}]
   }

   viewPort         $w $pxmin $pymin $pxmax $pymax
   worldCoordinates $w $xmin  $ymin  $xmax  $ymax

   DrawXaxis        $w $xmin  $xmax  $xdelt
   DrawYlabels      $w $ylabels $noseries
   DrawMask         $w

   SetColours $w blue lightblue green yellow orange red magenta brown

   return $newchart
}

# createTimechart --
#    Create a command for drawing a simple timechart
# Arguments:
#    w           Name of the canvas
#    time_begin  Start time (in the form of a date/time)
#    time_end    End time (in the form of a date/time)
#    noitems     Number of items to be shown (determines spacing)
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the timechart.
#
proc ::Plotchart::createTimechart { w time_begin time_end noitems } {
   variable data_series
   variable scaling

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "timechart_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler timechart $w

   foreach {pxmin pymin pxmax pymax} [MarginsRectangle $w 3] {break}

   set ymin  0.0
   set ymax  $noitems

   set xmin  [expr {1.0*[clock scan $time_begin]}]
   set xmax  [expr {1.0*[clock scan $time_end]}]

   viewPort         $w $pxmin $pymin $pxmax $pymax
   worldCoordinates $w $xmin  $ymin  $xmax  $ymax

   set scaling($w,current) $ymax
   set scaling($w,dy)      -0.7

   return $newchart
}

# create3DPlot --
#    Create a simple 3D plot
# Arguments:
#    w           Name of the canvas
#    xscale      Minimum, maximum and step for x-axis (initial)
#    yscale      Minimum, maximum and step for y-axis
#    zscale      Minimum, maximum and step for z-axis
# Result:
#    Name of a new command
# Note:
#    The entire canvas will be dedicated to the 3D plot
#
proc ::Plotchart::create3DPlot { w xscale yscale zscale } {
   variable data_series

   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "3dplot_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler 3dplot $w

   foreach {pxmin pymin pxmax pymax} [Margins3DPlot $w] {break}

   foreach {xmin xmax xstep} $xscale {break}
   foreach {ymin ymax ystep} $yscale {break}
   foreach {zmin zmax zstep} $zscale {break}

   viewPort           $w $pxmin $pymin $pxmax $pymax
   world3DCoordinates $w $xmin  $ymin  $zmin  $xmax  $ymax $zmax

   Draw3DAxes         $w $xmin  $ymin  $zmin  $xmax  $ymax $zmax \
                         $xstep $ystep $zstep

   SetColours $w grey black

   return $newchart
}

# Load the private procedures
#
source [file join [file dirname [info script]] "plotpriv.tcl"]
source [file join [file dirname [info script]] "plotaxis.tcl"]
source [file join [file dirname [info script]] "plot3d.tcl"]
source [file join [file dirname [info script]] "scaling.tcl"]

# Announce our presence
#
package provide Plotchart 0.9
