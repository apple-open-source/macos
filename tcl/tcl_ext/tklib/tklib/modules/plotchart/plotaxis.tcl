# plotaxis.tcl --
#    Facilities to draw simple plots in a dedicated canvas
#
# Note:
#    This source file contains the functions for drawing the axes.
#    It is the companion of "plotchart.tcl"
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

   set scaling($w,ydelt) $ydelt

   $w delete yaxis

   $w create line $scaling($w,pxmin) $scaling($w,pymin) \
                  $scaling($w,pxmin) $scaling($w,pymax) \
                  -fill black -tag yaxis

   set format ""
   if { [info exists scaling($w,-format,y)] } {
      set format $scaling($w,-format,y)
   }

   set y $ymin
   while { $y < $ymax+0.5*$ydelt } {
      foreach {xcrd ycrd} [coordsToPixel $w $scaling($w,xmin) $y] {break}
      set ylabel $y
      if { $format != "" } {
         set ylabel [format $format $y]
      }
      $w create text $xcrd $ycrd -text $ylabel -tag yaxis -anchor e
      set y [expr {$y+$ydelt}]
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

   set scaling($w,xdelt) $xdelt

   $w delete xaxis

   $w create line $scaling($w,pxmin) $scaling($w,pymax) \
                  $scaling($w,pxmax) $scaling($w,pymax) \
                  -fill black -tag xaxis

   set format ""
   if { [info exists scaling($w,-format,x)] } {
      set format $scaling($w,-format,x)
   }

   set x $xmin
   while { $x < $xmax+0.5*$xdelt } {
      foreach {xcrd ycrd} [coordsToPixel $w $x $scaling($w,ymin)] {break}

      set xlabel $x
      if { $format != "" } {
         set xlabel [format $format $x]
      }
      $w create text $xcrd $ycrd -text $xlabel -tag xaxis -anchor n
      set x [expr {$x+$xdelt}]
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

   set xt [expr {($scaling($w,pxmin)+$scaling($w,pxmax))/2}]
   set yt [expr {$scaling($w,pymax)+12}]

   $w create text $xt $yt -text $text -fill black -anchor n
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

   set xt $scaling($w,pxmin)
   set yt [expr {$scaling($w,pymin)-8}]

   $w create text $xt $yt -text $text -fill black -anchor se
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

   #
   # Draw the spikes
   #
   set angle 0.0

   foreach {xcentre ycentre} [polarToPixel $w 0.0 0.0] {break}

   while { $angle < 360.0 } {
      foreach {xcrd ycrd} [polarToPixel $w $rad_max $angle] {break}
      foreach {xtxt ytxt} [polarToPixel $w [expr {1.05*$rad_max}] $angle] {break}
      $w create line $xcentre $ycentre $xcrd $ycrd
      if { $xcrd > $xcentre } {
         set dir w
      } else {
         set dir e
      }
      $w create text $xtxt $ytxt -text $angle -anchor $dir

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

      $w create oval $xleft $ytop $xright $ybottom

      $w create text $xright [expr {$ytxt+3}] -text $rad -anchor n

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

   $w delete xaxis

   $w create line $scaling($w,pxmin) $scaling($w,pymax) \
                  $scaling($w,pxmax) $scaling($w,pymax) \
                  -fill black -tag xaxis

   set x 0.5
   set scaling($w,ybase) {}
   foreach label $xlabels {
      foreach {xcrd ycrd} [coordsToPixel $w $x $scaling($w,ymin)] {break}
      $w create text $xcrd $ycrd -text $label -tag xaxis -anchor n
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

   $w delete yaxis

   $w create line $scaling($w,pxmin) $scaling($w,pymin) \
                  $scaling($w,pxmin) $scaling($w,pymax) \
                  -fill black -tag yaxis

   set y 0.5
   set scaling($w,xbase) {}
   foreach label $ylabels {
      foreach {xcrd ycrd} [coordsToPixel $w $scaling($w,xmin) $y] {break}
      $w create text $xcrd $ycrd -text $label -tag yaxis -anchor e
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
         set clear   [lindex  $axis_option_clear  $idx]
         set values  [lindex  $axis_option_values [incr idx]]
         if { $values != "..." } {
            if { [lsearch $values $value] < 0 } {
               return -code error "Unknown or invalid value: $value for option $option - $values"
            }
         }
         set scaling($w,$option,$orient) $value
         if { $clear } {
            set clear_data 1
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
