# plot3d.tcl --
#    Facilities to draw simple 3D plots in a dedicated canvas
#
# Note:
#    This source file contains the private functions for 3D plotting.
#    It is the companion of "plotchart.tcl"
#

# Draw3DAxes --
#    Draw the axes in a 3D plot
# Arguments:
#    w           Name of the canvas
#    xmin        Minimum x coordinate
#    xmax        Maximum x coordinate
#    xstep       Step size
#    ymin        Minimum y coordinate
#    ymax        Maximum y coordinate
#    ystep       Step size
#    zmin        Minimum z coordinate
#    zmax        Maximum z coordinate
#    zstep       Step size
#    names       List of labels for the x-axis (optional)
# Result:
#    None
# Note:
#    To keep the axes in positive orientation, the x-axis appears
#    on the right-hand side and the y-axis appears in front.
#    This may not be the most "intuitive" presentation though.
#
#    If the step for the x-axis is zero or negative, it is not
#    drawn - adopted from Keith Vetter's extension.
#
# Side effects:
#    Axes drawn in canvas
#
proc ::Plotchart::Draw3DAxes { w xmin  ymin  zmin
                                 xmax  ymax  zmax
                                 xstep ystep zstep
                                 {names {}}        } {
   variable scaling

   $w delete axis3d

   #
   # Create the support lines first
   #
   foreach {pxxmin pyxmin} [coords3DToPixel $w $scaling($w,xmin) $scaling($w,ymin) $scaling($w,zmin)] {break}
   foreach {pxxmax pyxmax} [coords3DToPixel $w $scaling($w,xmax) $scaling($w,ymin) $scaling($w,zmin)] {break}
   foreach {pxymax pyymax} [coords3DToPixel $w $scaling($w,xmax) $scaling($w,ymax) $scaling($w,zmin)] {break}
   foreach {pxzmax pyzmax} [coords3DToPixel $w $scaling($w,xmax) $scaling($w,ymin) $scaling($w,zmax)] {break}
   foreach {pxzmx2 pyzmx2} [coords3DToPixel $w $scaling($w,xmin) $scaling($w,ymin) $scaling($w,zmax)] {break}
   foreach {pxymx2 pyymx2} [coords3DToPixel $w $scaling($w,xmin) $scaling($w,ymax) $scaling($w,zmin)] {break}
   foreach {pxzymx pyzymx} [coords3DToPixel $w $scaling($w,xmax) $scaling($w,ymax) $scaling($w,zmax)] {break}

   if { $xstep > 0 } {
       $w create line $pxxmax $pyxmax $pxxmin $pyxmin -fill black -tag axis3d
       $w create line $pxxmax $pyxmax $pxymax $pyymax -fill black -tag axis3d
       $w create line $pxymax $pyymax $pxymx2 $pyymx2 -fill black -tag axis3d
       $w create line $pxzmax $pyzmax $pxzymx $pyzymx -fill black -tag axis3d
       $w create line $pxxmax $pyxmax $pxzmax $pyzmax -fill black -tag axis3d
       $w create line $pxzmax $pyzmax $pxzmx2 $pyzmx2 -fill black -tag axis3d
       $w create line $pxymax $pyymax $pxzymx $pyzymx -fill black -tag axis3d
   }
   $w create line $pxxmin $pyxmin $pxymx2 $pyymx2 -fill black -tag axis3d
   $w create line $pxxmin $pyxmin $pxzmx2 $pyzmx2 -fill black -tag axis3d

   #
   # Numbers to the z-axis
   #
   set z $zmin
   while { $z < $zmax+0.5*$zstep } {
      foreach {xcrd ycrd} [coords3DToPixel $w $xmin $ymin $z] {break}
      set xcrd2 [expr {$xcrd-3}]
      set xcrd3 [expr {$xcrd-5}]

      $w create line $xcrd2 $ycrd $xcrd $ycrd -tag axis3d
      $w create text $xcrd3 $ycrd -text $z -tag axis3d -anchor e
      set z [expr {$z+$zstep}]
   }

   #
   # Numbers or labels to the x-axis (shown on the right!)
   #
   if { $xstep > 0 } {
       if { $names eq "" } {
           set x $xmin
           while { $x < $xmax+0.5*$xstep } {
               foreach {xcrd ycrd} [coords3DToPixel $w $x $ymax $zmin] {break}
               set xcrd2 [expr {$xcrd+4}]
               set xcrd3 [expr {$xcrd+6}]

               $w create line $xcrd2 $ycrd $xcrd $ycrd -tag axis3d
               $w create text $xcrd3 $ycrd -text $x -tag axis3d -anchor w
               set x [expr {$x+$xstep}]
           }
       } else {
           set x [expr {$xmin+0.5*$xstep}]
           foreach label $names {
               foreach {xcrd ycrd} [coords3DToPixel $w $x $ymax $zmin] {break}
               set xcrd2 [expr {$xcrd+6}]

               $w create text $xcrd2 $ycrd -text $label -tag axis3d -anchor w
               set x [expr {$x+$xstep}]
           }
       }
   }

   #
   # Numbers to the y-axis (shown in front!)
   #
   set y $ymin
   while { $y < $ymax+0.5*$ystep } {
      foreach {xcrd ycrd} [coords3DToPixel $w $xmin $y $zmin] {break}
      set ycrd2 [expr {$ycrd+3}]
      set ycrd3 [expr {$ycrd+5}]

      $w create line $xcrd $ycrd2 $xcrd $ycrd -tag axis3d
      $w create text $xcrd $ycrd3 -text $y -tag axis3d -anchor n
      set y [expr {$y+$ystep}]
   }

   set scaling($w,xstep) $xstep
   set scaling($w,ystep) $ystep
   set scaling($w,zstep) $zstep

   #
   # Set the default grid size
   #
   GridSize3D $w 10 10
}

# GridSize3D --
#    Set the grid size for a 3D function plot
# Arguments:
#    w           Name of the canvas
#    nxcells     Number of cells in x-direction
#    nycells     Number of cells in y-direction
# Result:
#    None
# Side effect:
#    Store the grid sizes in the private array
#
proc ::Plotchart::GridSize3D { w nxcells nycells } {
   variable scaling

   set scaling($w,nxcells) $nxcells
   set scaling($w,nycells) $nycells
}

# Draw3DFunction --
#    Plot a function of x and y
# Arguments:
#    w           Name of the canvas
#    function    Name of a procedure implementing the function
# Result:
#    None
# Side effect:
#    The plot of the function - given the grid
#
proc ::Plotchart::Draw3DFunction { w function } {
   variable scaling

   set nxcells $scaling($w,nxcells)
   set nycells $scaling($w,nycells)
   set xmin    $scaling($w,xmin)
   set xmax    $scaling($w,xmax)
   set ymin    $scaling($w,ymin)
   set ymax    $scaling($w,ymax)
   set dx      [expr {($xmax-$xmin)/double($nxcells)}]
   set dy      [expr {($ymax-$ymin)/double($nycells)}]

   foreach {fill border} $scaling($w,colours) {break}

   #
   # Draw the quadrangles making up the plot in the right order:
   # first y from minimum to maximum
   # then x from maximum to minimum
   #
   for { set j 0 } { $j < $nycells } { incr j } {
      set y1 [expr {$ymin + $dy*$j}]
      set y2 [expr {$y1   + $dy}]
      for { set i $nxcells } { $i > 0 } { incr i -1 } {
         set x2 [expr {$xmin + $dx*$i}]
         set x1 [expr {$x2   - $dx}]

         set z11 [$function $x1 $y1]
         set z12 [$function $x1 $y2]
         set z21 [$function $x2 $y1]
         set z22 [$function $x2 $y2]

         foreach {px11 py11} [coords3DToPixel $w $x1 $y1 $z11] {break}
         foreach {px12 py12} [coords3DToPixel $w $x1 $y2 $z12] {break}
         foreach {px21 py21} [coords3DToPixel $w $x2 $y1 $z21] {break}
         foreach {px22 py22} [coords3DToPixel $w $x2 $y2 $z22] {break}

         $w create polygon $px11 $py11 $px21 $py21 $px22 $py22 \
                           $px12 $py12 $px11 $py11 \
                           -fill $fill -outline $border
      }
   }
}

# Draw3DData --
#    Plot a matrix of data as a function of x and y
# Arguments:
#    w           Name of the canvas
#    data        Nested list of data in the form of a matrix
# Result:
#    None
# Side effect:
#    The plot of the data
#
proc ::Plotchart::Draw3DData { w data } {
   variable scaling

   set  nxcells [llength [lindex $data 0]]
   set  nycells [llength $data]
   incr nxcells -1
   incr nycells -1

   set xmin    $scaling($w,xmin)
   set xmax    $scaling($w,xmax)
   set ymin    $scaling($w,ymin)
   set ymax    $scaling($w,ymax)
   set dx      [expr {($xmax-$xmin)/double($nxcells)}]
   set dy      [expr {($ymax-$ymin)/double($nycells)}]

   foreach {fill border} $scaling($w,colours) {break}

   #
   # Draw the quadrangles making up the data in the right order:
   # first y from minimum to maximum
   # then x from maximum to minimum
   #
   for { set j 0 } { $j < $nycells } { incr j } {
      set z1data [lindex $data $j]
      set z2data [lindex $data [expr {$j+1}]]
      set y1 [expr {$ymin + $dy*$j}]
      set y2 [expr {$y1   + $dy}]
      for { set i $nxcells } { $i > 0 } { incr i -1 } {
         set x2 [expr {$xmin + $dx*$i}]
         set x1 [expr {$x2   - $dx}]

         set z11 [lindex $z1data [expr {$i-1}]]
         set z21 [lindex $z1data $i           ]
         set z12 [lindex $z2data [expr {$i-1}]]
         set z22 [lindex $z2data $i           ]

         foreach {px11 py11} [coords3DToPixel $w $x1 $y1 $z11] {break}
         foreach {px12 py12} [coords3DToPixel $w $x1 $y2 $z12] {break}
         foreach {px21 py21} [coords3DToPixel $w $x2 $y1 $z21] {break}
         foreach {px22 py22} [coords3DToPixel $w $x2 $y2 $z22] {break}

         $w create polygon $px11 $py11 $px21 $py21 $px22 $py22 \
                           $px12 $py12 $px11 $py11 \
                           -fill $fill -outline $border
      }
   }
}

# Draw3DRibbon --
#     Plot yz-data as a 3D ribbon
#
# Arguments:
#     w               Widget to draw in
#     yzData          List of duples, each of which is y,z pair
#                     (y is left-to-right, z is up-and-down, x is front-to-back).
#
# Note:
#     Contributed by Keith Vetter (see the Wiki)
#
proc ::Plotchart::Draw3DRibbon { w yzData } { variable scaling

    set  nxcells 1
    set  nycells [llength $yzData]
    incr nxcells -1
    incr nycells -1

    set x1    $scaling($w,xmin)
    set x2    [expr {($scaling($w,xmax) - $x1)/10.0}]

    foreach {fill border} $scaling($w,colours) {break}

    #
    # Draw the quadrangles making up the data in the right order:
    # first y from minimum to maximum
    # then x from maximum to minimum
    #
    for { set j 0 } { $j < $nycells } { incr j } {
        set jj [expr {$j+1}]
        set y1 [lindex $yzData $j 0]
        set y2 [lindex $yzData $jj 0]
        set z1 [lindex $yzData $j 1]
        set z2 [lindex $yzData $jj 1]

        foreach {px11 py11} [::Plotchart::coords3DToPixel $w $x1 $y1 $z1] break
        foreach {px12 py12} [::Plotchart::coords3DToPixel $w $x1 $y2 $z2] break
        foreach {px21 py21} [::Plotchart::coords3DToPixel $w $x2 $y1 $z1] break
        foreach {px22 py22} [::Plotchart::coords3DToPixel $w $x2 $y2 $z2] break
        $w create polygon $px11 $py11 $px21 $py21 $px22 $py22 \
            $px12 $py12 $px11 $py11 \
            -fill $fill -outline $border
    }
}

# Draw3DLineFrom3Dcoordinates --
#    Plot a line in the three-dimensional axis system
# Arguments:
#    w           Name of the canvas
#    data        List of xyz-coordinates
#    colour      The colour to use
# Result:
#    None
# Side effect:
#    The projected line
#
proc ::Plotchart::Draw3DLineFrom3Dcoordinates { w data colour } {
   variable scaling

   set xmin    $scaling($w,xmin)
   set xmax    $scaling($w,xmax)
   set xprev   {}

   set coords  {}
   set colours {}
   foreach {x y z} $data {
       foreach {px py} [coords3DToPixel $w $x $y $z] {break}

       lappend coords $px $py

       if { $xprev == {} } {
           set xprev $x
       }
       set factor [expr {0.5*(2.0*$xmax-$xprev-$x)/($xmax-$xmin)}]

       lappend colours [GreyColour $colour $factor]
       set xprev $x
   }

   foreach {xb yb} [lrange $coords 0 end-2] {xe ye} [lrange $coords 2 end] c [lrange $colours 0 end-1] {
       $w create line $xb $yb $xe $ye -fill $c
   }
}

