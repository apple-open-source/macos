# plotpriv.tcl --
#    Facilities to draw simple plots in a dedicated canvas
#
# Note:
#    This source file contains the private functions.
#    It is the companion of "plotchart.tcl"
#

# WidthCanvas --
#    Return the width of the canvas
# Arguments:
#    w           Name of the canvas
# Result:
#    Width in pixels
#
proc ::Plotchart::WidthCanvas {w} {
    set width [winfo width $w]

    if { $width < 10 } {
        set width [$w cget -width]
    }
    return $width
}

# HeightCanvas --
#    Return the height of the canvas
# Arguments:
#    w           Name of the canvas
# Result:
#    Height in pixels
#
proc ::Plotchart::HeightCanvas {w} {
    set height [winfo height $w]

    if { $height < 10 } {
        set height [$w cget -height]
    }
    return $height
}

# SavePlot --
#    Save the plot/chart to a PostScript file (using default options)
# Arguments:
#    w           Name of the canvas
#    filename    Name of the file to write
#    args        Optional format (-format name)
# Result:
#    None
# Side effect:
#    A (new) PostScript file
#
proc ::Plotchart::SavePlot { w filename args } {

   if { [llength $args] == 0 } {
       #
       # Wait for the canvas to become visible - just in case.
       # Then write the file
       #
       update idletasks
       $w postscript -file $filename
   } else {
       if { [llength $args] == 2 && [lindex $args 0] == "-format" } {
           package require Img
           set format [lindex $args 1]

           #
           # This is a kludge:
           # Somehow tkwait does not always work (on Windows XP, that is)
           #
           raise [winfo toplevel $w]
          # tkwait visibility [winfo toplevel $w]
           after 2000 {set ::Plotchart::waited 0}
           vwait ::Plotchart::waited
           set img [image create photo -data $w -format window]
           $img write $filename -format $format
       } else {
           return -code error "Unknown option: $args - must be: -format img-format"
       }
   }
}

# MarginsRectangle --
#    Determine the margins for a rectangular plot/chart
# Arguments:
#    w           Name of the canvas
#    notext      Number of lines of text to make room for at the top
#                (default: 2.0)
#    text_width  Number of characters to be displayed at most on left
#                (default: 8)
# Result:
#    List of four values
#
proc ::Plotchart::MarginsRectangle { w {notext 2.0} {text_width 8}} {
   variable config
   set pxmin [expr {10*$text_width}]
   if { $pxmin < $config($w,margin,left) } {
       set pxmin $config($w,margin,left)
   }
   set pymin [expr {int(14*$notext)}]
   if { $pymin < $config($w,margin,top) } {
       set pymin $config($w,margin,top)
   }
   set pxmax [expr {[WidthCanvas $w]  - $config($w,margin,right)}]
   set pymax [expr {[HeightCanvas $w] - $config($w,margin,bottom)}]

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
   set pxmax [expr {[WidthCanvas $w]  - 80}]
   set pymax [expr {[HeightCanvas $w] - 30}]
   #set pxmax [expr {[$w cget -width]  - 80}]
   #set pymax [expr {[$w cget -height] - 30}]

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

   set yzwidth  [expr {(-120+[WidthCanvas $w])/(1.0+$yfract)}]
   set yzheight [expr {(-60+[HeightCanvas $w])/(1.0+$zfract)}]
   #set yzwidth  [expr {(-120+[$w cget -width])/(1.0+$yfract)}]
   #set yzheight [expr {(-60+[$w cget -height])/(1.0+$zfract)}]

   set pxmin    [expr {60+$yfract*$yzwidth}]
   set pxmax    [expr {[WidthCanvas $w] - 60}]
   #set pxmax    [expr {[$w cget -width] - 60}]
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

# CycleColours --
#    create cycling colours for those plots that treat them as a global resource
# Arguments:
#    colours     List of colours to be used. An empty list will activate to default colours
#    nr_data     Number of data records
# Result:
#    List of 'nr_data' colours to be used
#
proc ::Plotchart::CycleColours { colours nr_data } {
   if {![llength ${colours}]} {
       # force to most usable default colour list
       set colours {green blue red cyan yellow magenta}
   }

   if {[llength ${colours}] < ${nr_data}} {
	# cycle through colours
	set init_colours ${colours}
        set colours {}
        set pos 0
        for {set nr 0} {${nr} < ${nr_data}} {incr nr} {
            lappend colours [lindex ${init_colours} ${pos}]
            incr pos
            if {[llength ${init_colours}] <= ${pos}} {
                set pos 0
            }
	}
        if {[string equal [lindex ${colours} 0] [lindex ${colours} end]]} {
            # keep first and last colour different from selected colours
	    #    this will /sometimes fail in cases with only one/two colours in list
	    set colours [lreplace ${colours} end end [lindex ${colours} 1]]
        }
   }
   return ${colours}
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
   variable config

   set width  [expr {[WidthCanvas $w]  + 1}]
   set height [expr {[HeightCanvas $w] + 1}]
   set colour $config($w,background,outercolor)
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
   variable config

   set width  [WidthCanvas $w]
   #set width  [$w cget -width]
   set pymin  $scaling($w,pymin)

   $w create text [expr {$width/2}] 3 -text $title \
       -tags title -font $config($w,title,font) \
       -fill $config($w,title,textcolor) -anchor $config($w,title,anchor)
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
   # Check for missing values
   #
   if { $xcrd == "" || $ycrd == "" } {
       unset data_series($w,$series,x)
       return
   }

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
   set filled "no"
   if { [info exists data_series($w,$series,-filled)] } {
      set filled $data_series($w,$series,-filled)
   }
   set fillcolour white
   if { [info exists data_series($w,$series,-fillcolour)] } {
      set fillcolour $data_series($w,$series,-fillcolour)
   }

   foreach {pxcrd pycrd} [coordsToPixel $w $xcrd $ycrd] {break}

   if { [info exists data_series($w,$series,x)] } {
       set xold $data_series($w,$series,x)
       set yold $data_series($w,$series,y)
       foreach {pxold pyold} [coordsToPixel $w $xold $yold] {break}

       if { $filled ne "no" } {
           if { $filled eq "down" } {
               set pym $scaling($w,pymax)
           } else {
               set pym $scaling($w,pymin)
           }
           $w create polygon $pxold $pym $pxold $pyold $pxcrd $pycrd $pxcrd $pym \
               -fill $fillcolour -outline {} -tag data
       }

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
      DrawSymbolPixel $w $series $pxcrd $pycrd $symbol $colour "data"
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

   #
   # Check for missing values
   #
   if { $xcrd == "" || $ycrd == "" } {
       unset data_series($w,$series,x)
       return
   }

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

# DrawLogData --
#    Draw the data in an X-logY-plot
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
proc ::Plotchart::DrawLogData { w series xcrd ycrd } {

    DrawData $w $series $xcrd [expr {log10($ycrd)}]
}

# DrawInterval --
#    Draw the data as an error interval in an XY-plot
# Arguments:
#    w           Name of the canvas
#    series      Data series
#    xcrd        X coordinate
#    ymin        Minimum y coordinate
#    ymax        Maximum y coordinate
#    ycentr      Central y coordinate (optional)
# Result:
#    None
# Side effects:
#    New interval drawn in canvas
#
proc ::Plotchart::DrawInterval { w series xcrd ymin ymax {ycentr {}} } {
   variable data_series
   variable scaling

   #
   # Check for missing values
   #
   if { $xcrd == "" || $ymin == "" || $ymax == "" } {
       return
   }

   #
   # Draw the line piece
   #
   set colour "black"
   if { [info exists data_series($w,$series,-colour)] } {
      set colour $data_series($w,$series,-colour)
   }

   foreach {pxcrd pymin} [coordsToPixel $w $xcrd $ymin] {break}
   foreach {pxcrd pymax} [coordsToPixel $w $xcrd $ymax] {break}
   if { $ycentr != "" } {
       foreach {pxcrd pycentr} [coordsToPixel $w $xcrd $ycentr] {break}
   }

   #
   # Draw the I-shape (note the asymmetry!)
   #
   $w create line $pxcrd $pymin $pxcrd $pymax \
                        -fill $colour -tag data
   $w create line [expr {$pxcrd-3}] $pymin [expr {$pxcrd+4}] $pymin \
                        -fill $colour -tag data
   $w create line [expr {$pxcrd-3}] $pymax [expr {$pxcrd+4}] $pymax \
                        -fill $colour -tag data

   if { $ycentr != "" } {
      set symbol "dot"
      if { [info exists data_series($w,$series,-symbol)] } {
         set symbol $data_series($w,$series,-symbol)
      }
      DrawSymbolPixel $w $series $pxcrd $pycentr $symbol $colour "data"
   }

   $w lower data
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
#    tag         What tag to use
# Result:
#    None
# Side effects:
#    New symbol drawn in canvas
#
proc ::Plotchart::DrawSymbolPixel { w series pxcrd pycrd symbol colour tag } {
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
                               -fill $colour -tag $tag \
                               -capstyle projecting
              }
   "cross"    { $w create line $pxmin $pymin $pxmax $pymax \
                               $pxcrd $pycrd $pxmax $pymin \
                               $pxmin $pymax \
                               -fill $colour -tag $tag \
                               -capstyle projecting
              }
   "circle"   { $w create oval $pxmin $pymin $pxmax $pymax \
                               -outline $colour -tag $tag
              }
   "dot"      { $w create oval $pxmin $pymin $pxmax $pymax \
                               -outline $colour -fill $colour -tag $tag
              }
   "up"       { $w create polygon $pxmin $pymax $pxmax $pymax \
                               $pxcrd $pymin \
                               -outline $colour -fill {} -tag $tag
              }
   "upfilled" { $w create polygon $pxmin $pymax $pxmax $pymax \
                              $pxcrd $pymin \
                              -outline $colour -fill $colour -tag $tag
              }
   "down"     { $w create polygon $pxmin $pymin $pxmax $pymin \
                              $pxcrd $pymax \
                              -outline $colour -fill {} -tag $tag
              }
   "downfilled" { $w create polygon $pxmin $pymin $pxmax $pymin \
                              $pxcrd $pymax \
                              -outline $colour -fill $colour -tag $tag
              }
   }
}

# DrawTimeData --
#    Draw the data in an TX-plot
# Arguments:
#    w           Name of the canvas
#    series      Data series
#    time        Next date/time value
#    xcrd        Next x coordinate (vertical axis)
# Result:
#    None
# Side effects:
#    New data drawn in canvas
#
proc ::Plotchart::DrawTimeData { w series time xcrd } {
    DrawData $w $series [clock scan $time] $xcrd
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

   set colours $scaling(${w},colours)

   if {[llength ${data}] == 2} {
       # use canvas create oval as arc does not fill with colour for a full circle
       set colour [lindex ${colours} 0]
       ${w} create oval ${pxmin} ${pymin} ${pxmax} ${pymax} -fill ${colour}
       # text looks nicer at 45 degree
       set rad [expr {45.0 * 3.1415926 / 180.0}]
       set xtext [expr {(${pxmin}+${pxmax}+cos(${rad})*(${pxmax}-${pxmin}+20))/2}]
       set ytext [expr {(${pymin}+${pymax}-sin(${rad})*(${pymax}-${pymin}+20))/2}]
       foreach {label value} ${data} {
           break
       }
       ${w} create text ${xtext} ${ytext} -text ${label} -anchor w
       set scaling($w,angles) {0 360}
   } else {
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

       set idx 0

       array unset scaling ${w},angles
       set colours [CycleColours ${colours} [expr {[llength ${data}] / 2}]]

       foreach {label value} $data {
          set colour [lindex $colours $idx]
          incr idx

          if { $value == "" } {
              break
          }

          set angle_bgn [expr {$sum   * $factor}]
          set angle_ext [expr {$value * $factor}]
          lappend scaling(${w},angles) [expr {int(${angle_bgn})}]

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
#    dir         Direction if graded colours are used (see DrawGradientBackground)
# Result:
#    None
# Side effects:
#    Data bars drawn in canvas
#
proc ::Plotchart::DrawVertBarData { w series ydata {colour black} {dir {}} } {
   variable data_series
   variable scaling
   variable legend

   #
   # Draw the bars
   #
   set x $scaling($w,xbase)

   #
   # set the colours
   #
   if {[llength ${colour}]} {
       set colours ${colour}
   } elseif {[info exists scaling(${w},colours)]} {
       set colours $scaling(${w},colours)
   } else {
       set colours {}
   }
   set colours [CycleColours ${colours} [llength ${ydata}]]

   #
   # Legend information
   #
   set legendcol [lindex $colours 0]
   set data_series($w,$series,-colour) $legendcol
   set data_series($w,$series,-type)   rectangle
   if { [info exists legend($w,canvas)] } {
       set legendw $legend($w,canvas)
       $legendw itemconfigure $series -fill $legendcol
   }

   set newbase {}

   set idx 0
   foreach yvalue $ydata ybase $scaling($w,ybase) {
      set colour [lindex ${colours} ${idx}]
      incr idx

      if { $yvalue == "" } {
          set yvalue 0.0
      }

      set xnext [expr {$x+$scaling($w,barwidth)}]
      set y     [expr {$yvalue+$ybase}]
      foreach {px1 py1} [coordsToPixel $w $x     $ybase] {break}
      foreach {px2 py2} [coordsToPixel $w $xnext $y    ] {break}

      if { $dir == {} } {
          $w create rectangle $px1 $py1 $px2 $py2 \
                         -fill $colour -tag data
      } else {
          DrawGradientBackground $w $colour $dir [list $px1 $py1 $px2 $py2]
      }
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
#    dir         Direction if graded colours are used (see DrawGradientBackground)
# Result:
#    None
# Side effects:
#    Data bars drawn in canvas
#
proc ::Plotchart::DrawHorizBarData { w series xdata {colour black} {dir {}} } {
   variable data_series
   variable scaling

   #
   # Draw the bars
   #
   set y $scaling($w,ybase)

   #
   # set the colours
   #
   if {[llength ${colour}]} {
       set colours ${colour}
   } elseif {[info exists scaling(${w},colours)]} {
       set colours $scaling(${w},colours)
   } else {
       set colours {}
   }
   set colours [CycleColours ${colours} [llength ${xdata}]]

   #
   # Legend information
   #
   set legendcol [lindex $colours 0]
   set data_series($w,$series,-colour) $legendcol
   if { [info exists legend($w,canvas)] } {
       set legendw $legend($w,canvas)
       $legendw itemconfigure $series -fill $legendcol
   }

   set newbase {}

   set idx 0
   foreach xvalue $xdata xbase $scaling($w,xbase) {
      set colour [lindex ${colours} ${idx}]
      incr idx

      if { $xvalue == "" } {
          set xvalue 0.0
      }

      set ynext [expr {$y+$scaling($w,barwidth)}]
      set x     [expr {$xvalue+$xbase}]
      foreach {px1 py1} [coordsToPixel $w $xbase $y    ] {break}
      foreach {px2 py2} [coordsToPixel $w $x     $ynext] {break}

      if { $dir == {} } {
          $w create rectangle $px1 $py1 $px2 $py2 \
                         -fill $colour -tag data
      } else {
          DrawGradientBackground $w $colour $dir [list $px1 $py1 $px2 $py2]
      }

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

# DrawHistogramData --
#    Draw the vertical bars for a histogram
# Arguments:
#    w           Name of the canvas
#    series      Data series
#    xcrd        X coordinate (for the righthand side of the bar)
#    ycrd        Y coordinate
# Result:
#    None
# Side effects:
#    Data bars drawn in canvas
#
proc ::Plotchart::DrawHistogramData { w series xcrd ycrd } {
   variable data_series
   variable scaling

   #
   # Check for missing values (only y-value can be missing!)
   #
   if { $ycrd == "" } {
       set data_series($w,$series,x) $xcrd
       return
   }

   #
   # Draw the bar
   #
   set colour "black"
   if { [info exists data_series($w,$series,-colour)] } {
      set colour $data_series($w,$series,-colour)
   }

   foreach {pxcrd pycrd} [coordsToPixel $w $xcrd $ycrd] {break}

   if { [info exists data_series($w,$series,x)] } {
      set xold $data_series($w,$series,x)
   } else {
      set xold $scaling($w,xmin)
   }
   set yold $scaling($w,ymin)
   foreach {pxold pyold} [coordsToPixel $w $xold $yold] {break}

   $w create rectangle $pxold $pyold $pxcrd $pycrd \
                         -fill $colour -outline $colour -tag data
   $w lower data

   set data_series($w,$series,x) $xcrd
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

# BackgroundColour --
#    Set the background colour or other aspects of the background
# Arguments:
#    w           Name of the canvas
#    part        Which part: axes or plot
#    colour      Colour to use (or if part is "image", name of the image)
#    dir         Direction of increasing whiteness (optional, for "gradient"
#
# Result:
#    None
# Side effect:
#    Colour of the relevant part is changed
#
proc ::Plotchart::BackgroundColour { w part colour {dir {}} } {
    if { $part == "axes" } {
        $w configure -highlightthickness 0
        $w itemconfigure mask -fill $colour -outline $colour
    }
    if { $part == "plot" } {
        $w configure -highlightthickness 0
        $w configure -background $colour
    }
    if { $part == "gradient" } {
        DrawGradientBackground $w $colour $dir
    }
    if { $part == "image" } {
        DrawImageBackground $w $colour
    }
}

# DrawRadialSpokes --
#    Draw the spokes of the radial chart
# Arguments:
#    w           Name of the canvas
#    names       Names of the spokes
# Result:
#    None
# Side effects:
#    Radial chart filled in
#
proc ::Plotchart::DrawRadialSpokes { w names } {
   variable settings
   variable scaling

   set pxmin $scaling($w,pxmin)
   set pymin $scaling($w,pymin)
   set pxmax $scaling($w,pxmax)
   set pymax $scaling($w,pymax)

   $w create oval $pxmin $pymin $pxmax $pymax -outline black

   set dangle [expr {2.0 * 3.1415926 / [llength $names]}]
   set angle  0.0
   set xcentr [expr {($pxmin+$pxmax)/2.0}]
   set ycentr [expr {($pymin+$pymax)/2.0}]

   foreach name $names {
       set xtext  [expr {$xcentr+cos($angle)*($pxmax-$pxmin+20)/2}]
       set ytext  [expr {$ycentr-sin($angle)*($pymax-$pymin+20)/2}]
       set xspoke [expr {$xcentr+cos($angle)*($pxmax-$pxmin)/2}]
       set yspoke [expr {$ycentr-sin($angle)*($pymax-$pymin)/2}]

       if { cos($angle) >= 0.0 } {
           set anchor w
       } else {
           set anchor e
       }

       if { abs($xspoke-$xcentr) < 2 } {
           set xspoke $xcentr
       }
       if { abs($yspoke-$ycentr) < 2 } {
           set yspoke $ycentr
       }

       $w create text $xtext $ytext -text $name -anchor $anchor
       $w create line $xcentr $ycentr $xspoke $yspoke -fill black

       set angle [expr {$angle+$dangle}]
   }
}

# DrawRadial --
#    Draw the data for the radial chart
# Arguments:
#    w           Name of the canvas
#    values      Values for each spoke
#    colour      Colour of the line
#    thickness   Thickness of the line (optional)
# Result:
#    None
# Side effects:
#    New line drawn
#
proc ::Plotchart::DrawRadial { w values colour {thickness 1} } {
   variable data_series
   variable settings
   variable scaling

   if { [llength $values] != $settings($w,number) } {
       return -code error "Incorrect number of data given - should be $settings($w,number)"
   }

   set pxmin $scaling($w,pxmin)
   set pymin $scaling($w,pymin)
   set pxmax $scaling($w,pxmax)
   set pymax $scaling($w,pymax)

   set dangle [expr {2.0 * 3.1415926 / [llength $values]}]
   set angle  0.0
   set xcentr [expr {($pxmin+$pxmax)/2.0}]
   set ycentr [expr {($pymin+$pymax)/2.0}]

   set coords {}

   if { ! [info exists data_series($w,base)] } {
       set data_series($w,base) {}
       foreach value $values {
           lappend data_series($w,base) 0.0
       }
   }

   set newbase {}
   foreach value $values base $data_series($w,base) {
       if { $settings($w,style) != "lines" } {
           set value [expr {$value+$base}]
       }
       set factor [expr {$value/$settings($w,scale)}]
       set xspoke [expr {$xcentr+$factor*cos($angle)*($pxmax-$pxmin)/2}]
       set yspoke [expr {$ycentr-$factor*sin($angle)*($pymax-$pymin)/2}]

       if { abs($xspoke-$xcentr) < 2 } {
           set xspoke $xcentr
       }
       if { abs($yspoke-$ycentr) < 2 } {
           set yspoke $ycentr
       }

       lappend coords $xspoke $yspoke
       lappend newbase $value
       set angle [expr {$angle+$dangle}]
   }

   set data_series($w,base) $newbase

   if { $settings($w,style) == "filled" } {
       set fillcolour $colour
   } else {
       set fillcolour ""
   }

   set id [$w create polygon $coords -outline $colour -width $thickness -fill $fillcolour -tags data]
   $w lower $id
}

# DrawTrendLine --
#    Draw a trend line based on the given data in an XY-plot
# Arguments:
#    w           Name of the canvas
#    series      Data series
#    xcrd        Next x coordinate
#    ycrd        Next y coordinate
# Result:
#    None
# Side effects:
#    New/updated trend line drawn in canvas
#
proc ::Plotchart::DrawTrendLine { w series xcrd ycrd } {
    variable data_series
    variable scaling

    #
    # Check for missing values
    #
    if { $xcrd == "" || $ycrd == "" } {
        return
    }

    #
    # Compute the coefficients of the line
    #
    if { [info exists data_series($w,$series,xsum)] } {
        set nsum  [expr {$data_series($w,$series,nsum)  + 1.0}]
        set xsum  [expr {$data_series($w,$series,xsum)  + $xcrd}]
        set x2sum [expr {$data_series($w,$series,x2sum) + $xcrd*$xcrd}]
        set ysum  [expr {$data_series($w,$series,ysum)  + $ycrd}]
        set xysum [expr {$data_series($w,$series,xysum) + $ycrd*$xcrd}]
    } else {
        set nsum  [expr {1.0}]
        set xsum  [expr {$xcrd}]
        set x2sum [expr {$xcrd*$xcrd}]
        set ysum  [expr {$ycrd}]
        set xysum [expr {$ycrd*$xcrd}]
    }

    if { $nsum*$x2sum != $xsum*$xsum } {
        set a [expr {($nsum*$xysum-$xsum*$ysum)/($nsum*$x2sum - $xsum*$xsum)}]
    } else {
        set a 0.0
    }
    set b [expr {($ysum-$a*$xsum)/$nsum}]

    set xmin $scaling($w,xmin)
    set xmax $scaling($w,xmax)

    foreach {pxmin pymin} [coordsToPixel $w $xmin [expr {$a*$xmin+$b}]] {break}
    foreach {pxmax pymax} [coordsToPixel $w $xmax [expr {$a*$xmax+$b}]] {break}

    #
    # Draw the actual line
    #
    set colour "black"
    if { [info exists data_series($w,$series,-colour)] } {
        set colour $data_series($w,$series,-colour)
    }

    if { [info exists data_series($w,$series,trend)] } {
        $w coords $data_series($w,$series,trend) $pxmin $pymin $pxmax $pymax
    } else {
        set data_series($w,$series,trend) \
            [$w create line $pxmin $pymin $pxmax $pymax -fill $colour -tag data]
    }

    $w lower data

    set data_series($w,$series,nsum)  $nsum
    set data_series($w,$series,xsum)  $xsum
    set data_series($w,$series,x2sum) $x2sum
    set data_series($w,$series,ysum)  $ysum
    set data_series($w,$series,xysum) $xysum
}

# VectorConfigure --
#    Set configuration options for vectors
# Arguments:
#    w           Name of the canvas
#    series      Data series (identifier for vectors)
#    args        Pairs of configuration options:
#                -scale|-colour|-centred|-type {cartesian|polar|nautical}
# Result:
#    None
# Side effects:
#    Configuration options are stored
#
proc ::Plotchart::VectorConfigure { w series args } {
    variable data_series
    variable scaling

    foreach {option value} $args {
        switch -- $option {
            "-scale" {
                if { $value > 0.0 } {
                    set scaling($w,$series,vectorscale) $value
                } else {
                    return -code error "Scale factor must be positive"
                }
            }
            "-colour" - "-color" {
                set data_series($w,$series,vectorcolour) $value
            }
            "-centered" - "-centred" {
                set data_series($w,$series,vectorcentred) $value
            }
            "-type" {
                if { [lsearch {cartesian polar nautical} $value] >= 0 } {
                    set data_series($w,$series,vectortype) $value
                } else {
                    return -code error "Unknown vector components option: $value"
                }
            }
            default {
                return -code error "Unknown vector option: $option ($value)"
            }
        }
    }
}

# DrawVector --
#    Draw a vector at the given coordinates with the given components
# Arguments:
#    w           Name of the canvas
#    series      Data series (identifier for the vectors)
#    xcrd        X coordinate of start or centre
#    ycrd        Y coordinate of start or centre
#    ucmp        X component or length
#    vcmp        Y component or angle
# Result:
#    None
# Side effects:
#    New arrow drawn in canvas
#
proc ::Plotchart::DrawVector { w series xcrd ycrd ucmp vcmp } {
    variable data_series
    variable scaling

    #
    # Check for missing values
    #
    if { $xcrd == "" || $ycrd == "" } {
        return
    }
    #
    # Check for missing values
    #
    if { $ucmp == "" || $vcmp == "" } {
        return
    }

    #
    # Get the options
    #
    set scalef  1.0
    set colour  black
    set centred 0
    set type    cartesian
    if { [info exists scaling($w,$series,vectorscale)] } {
        set scalef $scaling($w,$series,vectorscale)
    }
    if { [info exists data_series($w,$series,vectorcolour)] } {
        set colour $data_series($w,$series,vectorcolour)
    }
    if { [info exists data_series($w,$series,vectorcentred)] } {
        set centred $data_series($w,$series,vectorcentred)
    }
    if { [info exists data_series($w,$series,vectortype)] } {
        set type $data_series($w,$series,vectortype)
    }

    #
    # Compute the coordinates of beginning and end of the arrow
    #
    switch -- $type {
        "polar" {
            set x1 [expr {$ucmp * cos( 3.1415926 * $vcmp / 180.0 ) }]
            set y1 [expr {$ucmp * sin( 3.1415926 * $vcmp / 180.0 ) }]
            set ucmp $x1
            set vcmp $y1
        }
        "nautical" {
            set x1 [expr {$ucmp * sin( 3.1415926 * $vcmp / 180.0 ) }]
            set y1 [expr {$ucmp * cos( 3.1415926 * $vcmp / 180.0 ) }]
            set ucmp $x1
            set vcmp $y1
        }
    }

    set u1 [expr {$scalef * $ucmp}]
    set v1 [expr {$scalef * $vcmp}]

    foreach {x1 y1} [coordsToPixel $w $xcrd $ycrd] {break}

    if { $centred } {
        set x1 [expr {$x1 - 0.5 * $u1}]
        set y1 [expr {$y1 + 0.5 * $v1}]
    }

    set x2 [expr {$x1 + $u1}]
    set y2 [expr {$y1 - $v1}]

    #
    # Draw the arrow
    #
    $w create line $x1 $y1 $x2 $y2 -fill $colour -tag data -arrow last
    $w lower data
}

# DotConfigure --
#    Set configuration options for dots
# Arguments:
#    w           Name of the canvas
#    series      Data series (identifier for dots)
#    args        Pairs of configuration options:
#                -radius|-colour|-classes {value colour ...}|-outline|-scalebyvalue|
#                -scale
# Result:
#    None
# Side effects:
#    Configuration options are stored
#
proc ::Plotchart::DotConfigure { w series args } {
    variable data_series
    variable scaling

    foreach {option value} $args {
        switch -- $option {
            "-scale" {
                if { $value > 0.0 } {
                    set scaling($w,$series,dotscale) $value
                } else {
                    return -code error "Scale factor must be positive"
                }
            }
            "-colour" - "-color" {
                set data_series($w,$series,dotcolour) $value
            }
            "-radius" {
                set data_series($w,$series,dotradius) $value
            }
            "-scalebyvalue" {
                set data_series($w,$series,dotscalebyvalue) $value
            }
            "-outline" {
                set data_series($w,$series,dotoutline) $value
            }
            "-classes" {
                set data_series($w,$series,dotclasses) $value
            }
            default {
                return -code error "Unknown dot option: $option ($value)"
            }
        }
    }
}

# DrawDot --
#    Draw a dot at the given coordinates, size and colour based on the given value
# Arguments:
#    w           Name of the canvas
#    series      Data series (identifier for the vectors)
#    xcrd        X coordinate of start or centre
#    ycrd        Y coordinate of start or centre
#    value       Value to be used
# Result:
#    None
# Side effects:
#    New oval drawn in canvas
#
proc ::Plotchart::DrawDot { w series xcrd ycrd value } {
    variable data_series
    variable scaling

    #
    # Check for missing values
    #
    if { $xcrd == "" || $ycrd == "" || $value == "" } {
        return
    }

    #
    # Get the options
    #
    set scalef   1.0
    set colour   black
    set usevalue 1
    set outline  black
    set radius   3
    set classes  {}
    if { [info exists scaling($w,$series,dotscale)] } {
        set scalef $scaling($w,$series,dotscale)
    }
    if { [info exists data_series($w,$series,dotcolour)] } {
        set colour $data_series($w,$series,dotcolour)
    }
    if { [info exists data_series($w,$series,dotoutline)] } {
        set outline {}
        if { $data_series($w,$series,dotoutline) } {
            set outline black
        }
    }
    if { [info exists data_series($w,$series,dotradius)] } {
        set radius $data_series($w,$series,dotradius)
    }
    if { [info exists data_series($w,$series,dotclasses)] } {
        set classes $data_series($w,$series,dotclasses)
    }
    if { [info exists data_series($w,$series,dotscalebyvalue)] } {
        set usevalue $data_series($w,$series,dotscalebyvalue)
    }

    #
    # Compute the radius and the colour
    #
    if { $usevalue } {
        set radius [expr {$scalef * $value}]
    }
    if { $classes != {} } {
        foreach {limit col} $classes {
            if { $value < $limit } {
                set colour $col
                break
            }
        }
    }

    foreach {x y} [coordsToPixel $w $xcrd $ycrd] {break}

    set x1 [expr {$x - $radius}]
    set y1 [expr {$y - $radius}]
    set x2 [expr {$x + $radius}]
    set y2 [expr {$y + $radius}]

    #
    # Draw the oval
    #
    $w create oval $x1 $y1 $x2 $y2 -fill $colour -tag data -outline $outline
    $w lower data
}

# DrawRchart --
#    Draw data together with two horizontal lines representing the
#    expected range
# Arguments:
#    w           Name of the canvas
#    series      Data series
#    xcrd        X coordinate of the data point
#    ycrd        Y coordinate of the data point
# Result:
#    None
# Side effects:
#    New data point drawn, lines updated
#
proc ::Plotchart::DrawRchart { w series xcrd ycrd } {
    variable data_series
    variable scaling

    #
    # Check for missing values
    #
    if { $xcrd == "" || $ycrd == "" } {
        return
    }

    #
    # In any case, draw the data point
    #
    DrawData $w $series $xcrd $ycrd

    #
    # Compute the expected range
    #
    if { ! [info exists data_series($w,$series,rchart)] } {
        set data_series($w,$series,rchart) $ycrd
    } else {
        lappend data_series($w,$series,rchart) $ycrd

        if { [llength $data_series($w,$series,rchart)] < 2 } {
            return
        }

        set filtered $data_series($w,$series,rchart)
        set outside  1
        while { $outside } {
            set data     $filtered
            foreach {ymin ymax} [RchartValues $data] {break}
            set filtered {}
            set outside  0
            foreach y $data {
                if { $y < $ymin || $y > $ymax } {
                    set outside 1
                } else {
                    lappend filtered $y
                }
            }
        }

        #
        # Draw the limit lines
        #
        if { [info exists data_series($w,$series,rchartlimits)] } {
            eval $w delete $data_series($w,$series,rchartlimits)
        }

        set colour "black"
        if { [info exists data_series($w,$series,-colour)] } {
            set colour $data_series($w,$series,-colour)
        }

        set xmin $scaling($w,xmin)
        set xmax $scaling($w,xmax)

        foreach {pxmin pymin} [coordsToPixel $w $xmin $ymin] {break}
        foreach {pxmax pymax} [coordsToPixel $w $xmax $ymax] {break}


        set data_series($w,$series,rchartlimits) [list \
            [$w create line $pxmin $pymin $pxmax $pymin -fill $colour -tag data] \
            [$w create line $pxmin $pymax $pxmax $pymax -fill $colour -tag data]]
    }
}

# RchartValues --
#    Compute the expected range for a series of data
# Arguments:
#    data        Data to be examined
# Result:
#    Expected minimum and maximum
#
proc ::Plotchart::RchartValues { data } {
    set sum   0.0
    set sum2  0.0
    set ndata [llength $data]

    if { $ndata <= 1 } {
        return [list $data $data]
    }

    foreach v $data {
        set sum   [expr {$sum  + $v}]
        set sum2  [expr {$sum2 + $v*$v}]
    }

    if { $ndata < 2 } {
       return [list $v $v]
    }

    set variance [expr {($sum2 - $sum*$sum/double($ndata))/($ndata-1.0)}]
    if { $variance < 0.0 } {
        set variance 0.0
    }

    set vmean [expr {$sum/$ndata}]
    set stdev [expr {sqrt($variance)}]
    set vmin  [expr {$vmean - 3.0 * $stdev}]
    set vmax  [expr {$vmean + 3.0 * $stdev}]

    return [list $vmin $vmax]
}
