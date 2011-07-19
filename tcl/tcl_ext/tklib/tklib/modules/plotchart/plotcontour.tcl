# plotcontour.tcl --
#     Contour plotting test program for the Plotchart package
#
#  Author: Mark Stucky
#
#  The basic idea behind the method used for contouring within this sample
#  is primarily based on :
#
#    (1) "Contour Plots of Large Data Sets" by Chris Johnston
#        Computer Language, May 1986
#
#  a somewhat similar method was also described in
#
#    (2) "A Contouring Subroutine" by Paul D. Bourke
#        BYTE, June 1987
#        http://astronomy.swin.edu.au/~pbourke/projection/conrec/
#
#  In (1) it is assumed that you have a N x M grid of data that you need
#  to process.  In order to generate a contour, each cell of the grid
#  is handled without regard to it's neighbors.  This is unlike many other
#  contouring algorithms that follow the current contour line into
#  neighboring cells in an attempt to produce "smoother" contours.
#
#  In general the method described is:
#
#     1) for each four cornered cell of the grid,
#        calculate the center of the cell (average of the four corners)
#
#           data(i  ,j)   : Point (1)
#           data(i+1,j)   : Point (2)
#           data(i+1,j+1) : Point (3)
#           data(i  ,j+1) : Point (4)
#           center        : Point (5)
#
#               (4)-------------(3)
#                | \           / |
#                |  \         /  |
#                |   \       /   |
#                |    \     /    |
#                |     \   /     |
#                |      (5)      |
#                |     /   \     |
#                |    /     \    |
#                |   /       \   |
#           ^    |  /         \  |
#           |    | /           \ |
#           J   (1)-------------(2)
#
#                I ->
#
#        This divides the cell into four triangles.
#
#     2) Each of the five points in the cell can be assigned a sign (+ or -)
#        depending upon whether the point is above (+) the current contour
#        or below (-).
#
#        A contour will cross an edge whenever the points on the boundary of
#        the edge are of an opposite sign.
#
#        A few examples :
#
#           (-)     (-)        (-)  |  (+)       (-)     (-)        (+)  |  (-)
#                                    \                _                   \
#                                     \              /  \                  \
#               (-)  -             (-) |          _ /(+) |           -  (+)  -
#                  /                  /                 /              \
#                 /                 /                  /                \
#           (-)  |  (+)        (-)  |  (+)       (+)  |  (-)        (-)  |  (+)
#
#
#        (Hopefully the "rough" character diagrams above give you the
#        general idea)
#
#        It turns out that there are 32 possibles combinations of + and -
#        and therefore 32 basic paths through the cell.  And if you swap
#        the (+) and (-) in the diagram above, the "same" basic path is
#        generated:
#
#           (+)     (+)        (+)  |  (-)       (+)     (+)        (-)  |  (+)
#                                    \                _                   \
#                                     \              /  \                  \
#               (+)  -             (+) |          _ /(-) |           -  (-)  -
#                  /                  /                 /              \
#                 /                 /                  /                \
#           (+)  |  (-)        (+)  |  (-)       (-)  |  (+)        (+)  |  (-)
#
#
#        So, it turns out that there are 16 basic paths through the cell.
#
###############################################################################
#
#  The original article/code worked on all four triangles together and
#  generated one of the 16 paths.
#
#  For this version of the code, I split the cell into the four triangles
#  and handle each triangle individually.
#
#  Doing it this way is slower than the above method for calculating the
#  contour lines.  But since it "simplifies" the code when doing "color filled"
#  contours, I opted for the longer calculation times.
#
#
# AM:
# Introduce the following methods in createXYPlot:
# - grid            Draw the grid (x,y needed)
# - contourlines    Draw isolines (x,y,z needed)
# - contourfill     Draw shades (x,y,z needed)
# - contourbox      Draw uniformly coloured cells (x,y,z needed)
#
# This needs still to be done:
# - colourmap       Set colours to be used (possibly interpolated)
#
# Note:
# To get the RGB values of a named colour:
# winfo rgb . color (divide by 256)
#
# The problem:
# What interface do we use?
#
# Changes:
# - Capitalised several proc names (to indicate they are private to
#   the Plotchart package)
# - Changed the data structure from an array to a list of lists.
#   This means:
#   - No confusion about the start of indices
#   - Lists can be passed as ordinary arguments
#   - In principle they are faster, but that does not really
#     matter here
# To do:
# - Absorb all global arrays into the Plotchart system of private data
# - Get rid of the bug in the shades algorithm ;)
#

# DrawGrid --
#     Draw the grid as contained in the lists of coordinates
# Arguments:
#     w           Canvas to draw in
#     x           X-coordinates of grid points (list of lists)
#     y           Y-coordinates of grid points (list of lists)
# Result:
#     None
# Side effect:
#     Grid drawn as lines between the vertices
# Note:
#     STILL TO DO
#     A cell is only drawn if there are four well-defined
#     corners. If the x or y coordinate is missing, the cell is
#     skipped.
#
proc ::Plotchart::DrawGrid {w x y} {

    set maxrow [llength $x]
    set maxcol [llength [lindex $x 0]]

    for {set i 0} {$i < $maxrow} {incr i} {
        set xylist {}
        for {set j 0} {$j < $maxcol} {incr j} {
            lappend xylist [lindex $x $i $j] [lindex $y $i $j]
        }
        C_line $w $xylist black
    }

    for {set j 0} {$j < $maxcol} {incr j} {
        set xylist {}
        for {set i 0} {$i < $maxrow} {incr i} {
            lappend xylist [lindex $x $i $j] [lindex $y $i $j]
        }
        C_line $w $xylist black
    }
}

# DrawIsolines --
#     Draw isolines in the given grid
# Arguments:
#     canv        Canvas to draw in
#     x           X-coordinates of grid points (list of lists)
#     y           Y-coordinates of grid points (list of lists)
#     f           Values of the parameter on the grid cell corners
#     cont        List of contour classes (or empty to indicate
#                 automatic scaling
# Result:
#     None
# Side effect:
#     Isolines drawn
# Note:
#     A cell is only drawn if there are four well-defined
#     corners. If the x or y coordinate is missing or the value is
#     missing, the cell is skipped.
#
proc ::Plotchart::DrawIsolines {canv x y f {cont {}} } {
    variable contour_options

    set contour_options(simple_box_contour) 0
    set contour_options(filled_contour) 0

#   DrawContour $canv $x $y $f 0.0 100.0 20.0
    DrawContour $canv $x $y $f $cont
}

# DrawShades --
#     Draw filled contours in the given grid
# Arguments:
#     canv        Canvas to draw in
#     x           X-coordinates of grid points (list of lists)
#     y           Y-coordinates of grid points (list of lists)
#     f           Values of the parameter on the grid cell corners
#     cont        List of contour classes (or empty to indicate
#                 automatic scaling
# Result:
#     None
# Side effect:
#     Shades (filled contours) drawn
# Note:
#     A cell is only drawn if there are four well-defined
#     corners. If the x or y coordinate is missing or the value is
#     missing, the cell is skipped.
#
proc ::Plotchart::DrawShades {canv x y f {cont {}} } {
    variable contour_options

    set contour_options(simple_box_contour) 0
    set contour_options(filled_contour) 1

#   DrawContour $canv $x $y $f 0.0 100.0 20.0
    DrawContour $canv $x $y $f $cont
}

# DrawBox --
#     Draw filled cells in the given grid (colour chosen according
#     to the _average_ of the four corner values)
# Arguments:
#     canv        Canvas to draw in
#     x           X-coordinates of grid points (list of lists)
#     y           Y-coordinates of grid points (list of lists)
#     f           Values of the parameter on the grid cell corners
#     cont        List of contour classes (or empty to indicate
#                 automatic scaling
# Result:
#     None
# Side effect:
#     Filled cells (quadrangles) drawn
# Note:
#     A cell is only drawn if there are four well-defined
#     corners. If the x or y coordinate is missing or the value is
#     missing, the cell is skipped.
#
proc ::Plotchart::DrawBox {canv x y f {cont {}} } {
    variable contour_options

    set contour_options(simple_box_contour) 1
    set contour_options(filled_contour) 0

#   DrawContour $canv $x $y $f 0.0 100.0 20.0
    DrawContour $canv $x $y $f $cont
}

# Draw3DFunctionContour --
#    Plot a function of x and y with a color filled contour
# Arguments:
#    w           Name of the canvas
#    function    Name of a procedure implementing the function
#    cont        contour levels
# Result:
#    None
# Side effect:
#    The plot of the function - given the grid
#
proc ::Plotchart::Draw3DFunctionContour { w function {cont {}} } {
    variable scaling
    variable contour_options

    set contour_options(simple_box_contour) 0
    set contour_options(filled_contour) 1
    set noTrans 0

    ::Plotchart::setColormapColors  [llength $cont]

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

            set xb [list $px11 $px21 $px22 $px12]
            set yb [list $py11 $py21 $py22 $py12]
            set fb [list $z11  $z21  $z22  $z12 ]

            Box_contour $w $xb $yb $fb $cont $noTrans

            $w create line $px11 $py11 $px21 $py21 $px22 $py22 \
                           $px12 $py12 $px11 $py11 \
                           -fill $border
      }
   }
}


# DrawContour --
#     Routine that loops over the grid and delegates the actual drawing
# Arguments:
#     canv        Canvas to draw in
#     x           X-coordinates of grid points (list of lists)
#     y           Y-coordinates of grid points (list of lists)
#     f           Values of the parameter on the grid cell corners
#     cont        List of contour classes (or empty to indicate
#                 automatic scaling)
# Result:
#     None
# Side effect:
#     Isolines, shades or boxes drawn
# Note:
#     A cell is only drawn if there are four well-defined
#     corners. If the x or y coordinate is missing or the value is
#     missing, the cell is skipped.
#
proc ::Plotchart::DrawContour {canv x y f cont} {
    variable contour_options
    variable colorMap

    #
    # Construct the class-colour list
    #
    set cont [MakeContourClasses $f $cont]

    set fmin  [lindex $cont 0 0]
    set fmax  [lindex $cont end 0]
    set ncont [llength $cont]

    # Now that we know how many entries (ncont), create
    # the colormap colors
    #
    # I moved this into MakeContourClasses...
    #    ::Plotchart::setColormapColors  $ncont

    set maxrow [llength $x]
    set maxcol [llength [lindex $x 0]]

    for {set i 0} {$i < $maxrow-1} {incr i} {
        set i1 [expr {$i + 1}]
        for {set j 0} {$j < $maxcol-1} {incr j} {
            set j1 [expr {$j + 1}]

            set x1 [lindex $x $i1 $j]
            set x2 [lindex $x $i $j]
            set x3 [lindex $x $i $j1]
            set x4 [lindex $x $i1 $j1]

            set y1 [lindex $y $i1 $j]
            set y2 [lindex $y $i $j]
            set y3 [lindex $y $i $j1]
            set y4 [lindex $y $i1 $j1]

            set f1 [lindex $f $i1 $j]
            set f2 [lindex $f $i $j]
            set f3 [lindex $f $i $j1]
            set f4 [lindex $f $i1 $j1]

            set xb [list $x1 $x2 $x3 $x4]
            set yb [list $y1 $y2 $y3 $y4]
            set fb [list $f1 $f2 $f3 $f4]

            if { [lsearch $fb {}] >= 0 ||
                 [lsearch $xb {}] >= 0 ||
                 [lsearch $yb {}] >= 0    } {
                continue
            }

            Box_contour $canv $xb $yb $fb $cont
        }
    }
}

# Box_contour --
#     Draw a filled box
# Arguments:
#     canv        Canvas to draw in
#     xb          X-coordinates of the four corners
#     yb          Y-coordinates of the four corners
#     fb          Values of the parameter on the four corners
#     cont        List of contour classes and colours
# Result:
#     None
# Side effect:
#     Box drawn for a single cell
#
proc ::Plotchart::Box_contour {canv xb yb fb cont {doTrans 1}} {
    variable colorMap
    variable contour_options

    foreach {x1 x2 x3 x4} $xb {}
    foreach {y1 y2 y3 y4} $yb {}
    foreach {f1 f2 f3 f4} $fb {}

    set xc [expr {($x1 + $x2 + $x3 + $x4) * 0.25}]
    set yc [expr {($y1 + $y2 + $y3 + $y4) * 0.25}]
    set fc [expr {($f1 + $f2 + $f3 + $f4) * 0.25}]

    if {$contour_options(simple_box_contour)} {

        set fmin  [lindex $cont 0]
        set fmax  [lindex $cont end]
        set ncont [llength $cont]

        set ic 0
        for {set i 0} {$i < $ncont} {incr i} {
            set ff [lindex $cont $i 0]
            if {$ff <= $fc} {
                set ic $i
            }
        }

        set xylist [list $x1 $y1 $x2 $y2 $x3 $y3 $x4 $y4]

        # canvasPlot::polygon $win $xylist -fill $cont($ic,color)
        ###        C_polygon $canv $xylist $cont($ic,color)
        C_polygon $canv $xylist [lindex $cont $ic 1]

    } else {

#debug#        puts "Tri_contour 1)"
        Tri_contour $canv $x1 $y1 $f1 $x2 $y2 $f2 $xc $yc $fc $cont $doTrans

#debug#        puts "Tri_contour 2)"
        Tri_contour $canv $x2 $y2 $f2 $x3 $y3 $f3 $xc $yc $fc $cont $doTrans

#debug#        puts "Tri_contour 3)"
        Tri_contour $canv $x3 $y3 $f3 $x4 $y4 $f4 $xc $yc $fc $cont $doTrans

#debug#        puts "Tri_contour 4)"
        Tri_contour $canv $x4 $y4 $f4 $x1 $y1 $f1 $xc $yc $fc $cont $doTrans

    }

}

# Tri_contour --
#     Draw isolines or shades in a triangle
# Arguments:
#     canv        Canvas to draw in
#     x1,x2,x3    X-coordinate  of the three corners
#     y1,y2,y3    Y-coordinates of the three corners
#     f1,f2,f3    Values of the parameter on the three corners
#     cont        List of contour classes and colours
# Result:
#     None
# Side effect:
#     Isolines/shades drawn for a single triangle
#
proc ::Plotchart::Tri_contour { canv x1 y1 f1 x2 y2 f2 x3 y3 f3 cont {doTrans 1} } {
    variable contour_options
    variable colorMap

    set ncont [llength $cont]


    # Find the min/max function values for this triangle
    #
    set tfmin  [min $f1 $f2 $f3]
    set tfmax  [max $f1 $f2 $f3]

    # Based on the above min/max, figure out which
    # contour levels/colors that bracket this interval
    #
    set imin 0
    set imax 0   ;#mbs#
    for {set i 0} {$i < $ncont} {incr i} {
        set ff [lindex $cont $i]           ; ### set ff $cont($i,fval)
        if {$ff <= $tfmin} {
            set imin $i
            set imax $i
        }
        if { $ff <= $tfmax} {
            set imax $i
        }
    }

    set vertlist {}

    # Loop over all contour levels of interest for this triangle
    #
    for {set ic $imin} {$ic <= $imax} {incr ic} {

        # Get the value for this contour level
        #
        set ff [lindex $cont $ic 0]         ;###  set ff $cont($ic,fval)

        set xylist   {}
        set pxylist  {}

        # Classify the triangle based on whether the functional values, f1,f2,f3
        # are above (+), below (-), or equal (=) to the current contour level ff
        #
        set s1 [::Plotchart::setFLevel $f1 $ff]
        set s2 [::Plotchart::setFLevel $f2 $ff]
        set s3 [::Plotchart::setFLevel $f3 $ff]

        set class "$s1$s2$s3"

        # Describe class here...

        # ( - - - )   : Case A,
        # ( - - = )   : Case B, color a point, do nothing
        # ( - - + )   : Case C, contour between {23}-{31}
        # ( - = - )   : Case D, color a point, do nothing
        # ( - = = )   : Case E, contour line between 2-3
        # ( - = + )   : Case F, contour between 2-{31}
        # ( - + - )   : Case G, contour between {12}-{23}
        # ( - + = )   : Case H, contour between {12}-3
        # ( - + + )   : Case I, contour between {12}-{31}
        # ( = - - )   : Case J, color a point, do nothing
        # ( = - = )   : Case K, contour line between 1-3
        # ( = - + )   : Case L, contour between 1-{23}
        # ( = = - )   : Case M, contour line between 1-2
        # ( = = = )   : Case N, fill full triangle, return
        # ( = = + )   : Case M,
        # ( = + - )   : Case L,
        # ( = + = )   : Case K,
        # ( = + + )   : Case J,
        # ( + - - )   : Case I,
        # ( + - = )   : Case H,
        # ( + - + )   : Case G,
        # ( + = - )   : Case F,
        # ( + = = )   : Case E,
        # ( + = + )   : Case D,
        # ( + + - )   : Case C,
        # ( + + = )   : Case B,
        # ( + + + )   : Case A,


        switch -- $class {

            "---" {
                ############### Case A ###############

#debug#                puts "class A = $class , $ic , $ff"
                if {$contour_options(filled_contour)} {
                    set pxylist [list $x1 $y1 $x2 $y2 $x3 $y3]
                    C_polygon $canv $pxylist [lindex $colorMap $ic] $doTrans
                }
                return
            }

            "+++" {
#debug#                puts "class A = $class , $ic , $ff"
                if {$contour_options(filled_contour)} {
                    if {$ic == $imax} {
                        set pxylist [list $x1 $y1 $x2 $y2 $x3 $y3]
                        set ictmp [expr {$ic + 1}]
                        C_polygon $canv $pxylist [lindex $colorMap $ictmp] $doTrans
                        return
                    }
                }
            }

            "===" {
                ############### Case N ###############

#debug#                puts "class N = $class , $ic , $ff"
                if {$contour_options(filled_contour)} {
                    set pxylist [list $x1 $y1 $x2 $y2 $x3 $y3]
                    C_polygon $canv $pxylist [lindex $colorMap $ic] $doTrans
                }
                return
            }

            "--=" {
                ############### Case B ###############

#debug#                puts "class B = $class , $ic , $ff"
                if {$contour_options(filled_contour)} {
                    set pxylist [list $x1 $y1 $x2 $y2 $x3 $y3]
                    C_polygon $canv $pxylist [lindex $colorMap $ic] $doTrans
                }
                return
            }

            "++=" {
#debug#                puts "class B= $class , $ic , $ff , do nothing unless ic == imax"
                if {$ic == $imax} {
                    if {$contour_options(filled_contour)} {
                        set pxylist [list $x1 $y1 $x2 $y2 $x3 $y3]
                        set ictmp [expr {$ic + 1}]
                        C_polygon $canv $pxylist [lindex $colorMap $ictmp] $doTrans
                        return
                    }
                }
            }

            "-=-" {
                ############### Case D ###############

#debug#                puts "class D = $class , $ic , $ff"
                if {$contour_options(filled_contour)} {
                    set pxylist [list $x1 $y1 $x2 $y2 $x3 $y3]
                    C_polygon $canv $pxylist [lindex $colorMap $ic] $doTrans
                }
                return
            }

            "+=+" {
#debug#                puts "class D = $class , $ic , $ff , do nothing unless ic == imax"
                if {$ic == $imax} {
                    if {$contour_options(filled_contour)} {
                        set pxylist [list $x1 $y1 $x2 $y2 $x3 $y3]
                        set ictmp [expr {$ic + 1}]
                        C_polygon $canv $pxylist [lindex $colorMap $ictmp] $doTrans
                        return
                    }
                }
            }

            "=--" {
                ############### Case J ###############

#debug#                puts "class J = $class , $ic , $ff"
                if {$contour_options(filled_contour)} {
                    set pxylist [list $x1 $y1 $x2 $y2 $x3 $y3]
                    C_polygon $canv $pxylist [lindex $colorMap $ic] $doTrans
                }
                return
            }

            "=++" {
#debug#                puts "class J = $class , $ic , $ff , do nothing unless ic == imax"
                if {$ic == $imax} {
                    if {$contour_options(filled_contour)} {
                        set pxylist [list $x1 $y1 $x2 $y2 $x3 $y3]
                        set ictmp [expr {$ic + 1}]
                        C_polygon $canv $pxylist [lindex $colorMap $ictmp] $doTrans
                        return
                    }
                }
            }

            "=-=" {
                ############### Case K ###############

#debug#                puts "class K = $class , $ic , $ff"
                set xylist [list $x1 $y1 $x3 $y3]
                if {$contour_options(filled_contour)} {
                    set pxylist [list $x1 $y1 $x2 $y2 $x3 $y3]
                    C_polygon $canv $pxylist [lindex $colorMap $ic] $doTrans
                }
                C_line $canv $xylist [lindex $colorMap $ic] $doTrans
                return

            }

            "=+=" {
#debug#                puts "class K = $class , $ic , $ff"
                set xylist [list $x1 $y1 $x3 $y3]
                if {$ic == $imax} {
                    if {$contour_options(filled_contour)} {
                        set pxylist [list $x1 $y1 $x2 $y2 $x3 $y3]
                        set ictmp [expr {$ic + 1}]
                        C_polygon $canv $pxylist [lindex $colorMap $ictmp] $doTrans
                        return
                    }
                    C_line $canv $xylist [lindex $colorMap $ic] $doTrans

                } else {
                    C_line $canv $xylist [lindex $colorMap $ic] $doTrans
                }
            }

            "-==" {
                ############### Case E ###############

#debug#                puts "class E = $class , $ic , $ff"
                set xylist [list $x2 $y2 $x3 $y3]
                if {$contour_options(filled_contour)} {
                    set pxylist [list $x1 $y1 $x2 $y2 $x3 $y3]
                    C_polygon $canv $pxylist [lindex $colorMap $ic] $doTrans
                }
                C_line $canv $xylist [lindex $colorMap $ic] $doTrans
                return
            }

            "+==" {
#debug#                puts "class E = $class , $ic , $ff"
                set xylist [list $x2 $y2 $x3 $y3]
                if {$ic == $imax} {
                    if {$contour_options(filled_contour)} {
                        set pxylist [list $x1 $y1 $x2 $y2 $x3 $y3]
                        set ictmp [expr {$ic + 1}]
                        C_polygon $canv $pxylist [lindex $colorMap $ictmp] $doTrans
                        return
                    }
                    C_line $canv $xylist [lindex $colorMap $ic] $doTrans

                } else {
                    C_line $canv $xylist [lindex $colorMap $ic] $doTrans
                }
            }

            "==-" {
                ############### Case M ###############

#debug#                puts "class M = $class , $ic , $ff"
                set xylist [list $x1 $y1 $x2 $y2]
                if {$contour_options(filled_contour)} {
                    set pxylist [list $x1 $y1 $x2 $y2 $x3 $y3]
                    C_polygon $canv $pxylist [lindex $colorMap $ic] $doTrans
                }
                C_line $canv $xylist [lindex $colorMap $ic] $doTrans
                return
            }

            "==+" {
#debug#                puts "class M = $class , $ic , $ff"
                set xylist [list $x1 $y1 $x2 $y2]
                if {$ic == $imax} {
                    if {$contour_options(filled_contour)} {
                        set pxylist [list $x1 $y1 $x2 $y2 $x3 $y3]
                        set ictmp [expr {$ic + 1}]
                        C_polygon $canv $pxylist [lindex $colorMap $ictmp] $doTrans
                        return
                    }
                    C_line $canv $xylist [lindex $colorMap $ic] $doTrans

                } else {
                    C_line $canv $xylist [lindex $colorMap $ic] $doTrans
                }

            }

            "-=+" {
                ############### Case F ###############

#debug#                puts "class F = $class , $ic , $ff"
                set xylist [list $x2 $y2]
                set xyf2  [fintpl $x3 $y3 $f3 $x1 $y1 $f1 $ff]
                foreach {xx yy} $xyf2 {}
                lappend xylist $xx $yy

                if {$contour_options(filled_contour)} {
                        set pxylist $xylist
                        lappend pxylist $x1 $y1
                        C_polygon $canv $pxylist [lindex $colorMap $ic] $doTrans
                }
                C_line $canv $xylist [lindex $colorMap $ic] $doTrans

                set x1 $xx; set y1 $yy; set f1 $ff

                if {$ic == $imax} {
                    if {$contour_options(filled_contour)} {
                        set pxylist [list $x1 $y1 $x2 $y2 $x3 $y3]
                        set ictmp [expr {$ic + 1}]
                        C_polygon $canv $pxylist [lindex $colorMap $ictmp] $doTrans
                        return
                    }
                }

            }

            "+=-" {
#debug#                puts "class F = $class , $ic , $ff"
                set xylist [list $x2 $y2]
                set xyf2  [fintpl $x3 $y3 $f3 $x1 $y1 $f1 $ff]
                foreach {xx yy} $xyf2 {}
                lappend xylist $xx $yy

                if {$contour_options(filled_contour)} {
                        set pxylist $xylist
                        lappend pxylist $x3 $y3
                        C_polygon $canv $pxylist [lindex $colorMap $ic] $doTrans
                }
                C_line $canv $xylist [lindex $colorMap $ic] $doTrans

                set x3 $xx; set y3 $yy; set f3 $ff

                if {$ic == $imax} {
                    if {$contour_options(filled_contour)} {
                        set pxylist [list $x1 $y1 $x2 $y2 $x3 $y3]
                        set ictmp [expr {$ic + 1}]
                        C_polygon $canv $pxylist [lindex $colorMap $ictmp] $doTrans
                        return
                    }
                }

            }

            "-+=" {
                ############### Case H ###############

#debug#                puts "class H = $class , $ic , $ff"
                set xylist [fintpl $x1 $y1 $f1 $x2 $y2 $f2 $ff]
                foreach {xx yy} $xylist {}
                lappend xylist $x3 $y3

                if {$contour_options(filled_contour)} {
                        set pxylist $xylist
                        lappend pxylist $x1 $y1
                        C_polygon $canv $pxylist [lindex $colorMap $ic] $doTrans
                }
                C_line $canv $xylist [lindex $colorMap $ic] $doTrans

                set x1 $xx; set y1 $yy; set f1 $ff

                if {$ic == $imax} {
                    if {$contour_options(filled_contour)} {
                        set pxylist [list $x1 $y1 $x2 $y2 $x3 $y3]
                        set ictmp [expr {$ic + 1}]
                        C_polygon $canv $pxylist [lindex $colorMap $ictmp] $doTrans
                        return
                    }
                }

            }

            "+-=" {
#debug#                puts "class H = $class , $ic , $ff"
                set xylist [fintpl $x1 $y1 $f1 $x2 $y2 $f2 $ff]
                foreach {xx yy} $xylist {}
                lappend xylist $x3 $y3
                C_line $canv $xylist [lindex $colorMap $ic] $doTrans

                if {$contour_options(filled_contour)} {
                        set pxylist $xylist
                        lappend pxylist $x2 $y2
                        C_polygon $canv $pxylist [lindex $colorMap $ic] $doTrans
                }
                C_line $canv $xylist [lindex $colorMap $ic] $doTrans

                set x2 $xx; set y2 $yy; set f2 $ff

                if {$ic == $imax} {
                    if {$contour_options(filled_contour)} {
                        set pxylist [list $x1 $y1 $x2 $y2 $x3 $y3]
                        set ictmp [expr {$ic + 1}]
                        C_polygon $canv $pxylist [lindex $colorMap $ictmp] $doTrans
                        return
                    }
                }

            }

            "=-+" {
                ############### Case L ###############

#debug#                puts "class L = $class , $ic , $ff"
                set xylist [fintpl $x2 $y2 $f2 $x3 $y3 $f3 $ff]
                foreach {xx yy} $xylist {}
                lappend xylist $x1 $y1
                C_line $canv $xylist [lindex $colorMap $ic] $doTrans

                if {$contour_options(filled_contour)} {
                        set pxylist $xylist
                        lappend pxylist $x2 $y2
                        C_polygon $canv $pxylist [lindex $colorMap $ic] $doTrans
                }
                C_line $canv $xylist [lindex $colorMap $ic] $doTrans

                set x2 $xx; set y2 $yy; set f2 $ff

                if {$ic == $imax} {
                    if {$contour_options(filled_contour)} {
                        set pxylist [list $x1 $y1 $x2 $y2 $x3 $y3]
                        set ictmp [expr {$ic + 1}]
                        C_polygon $canv $pxylist [lindex $colorMap $ictmp] $doTrans
                        return
                    }
                }

            }

            "=+-" {
#debug#                puts "class L = $class , $ic , $ff"
                set xylist [fintpl $x2 $y2 $f2 $x3 $y3 $f3 $ff]
                foreach {xx yy} $xylist {}
                lappend xylist $x1 $y1
                C_line $canv $xylist [lindex $colorMap $ic] $doTrans

                if {$contour_options(filled_contour)} {
                        set pxylist $xylist
                        lappend pxylist $x3 $y3
                        C_polygon $canv $pxylist [lindex $colorMap $ic] $doTrans
                }
                C_line $canv $xylist [lindex $colorMap $ic] $doTrans

                set x3 $xx; set y3 $yy; set f3 $ff

                if {$ic == $imax} {
                    if {$contour_options(filled_contour)} {
                        set pxylist [list $x1 $y1 $x2 $y2 $x3 $y3]
                        set ictmp [expr {$ic + 1}]
                        C_polygon $canv $pxylist [lindex $colorMap $ictmp] $doTrans
                        return
                    }
                }

            }

            "--+" {
                ############### Case C ###############

#debug#                puts "class C = $class , $ic , $ff"
                set xyf1  [fintpl $x2 $y2 $f2 $x3 $y3 $f3 $ff]
                set xyf2  [fintpl $x3 $y3 $f3 $x1 $y1 $f1 $ff]
                set xylist $xyf1
                foreach {xx1 yy1} $xyf1 {}
                foreach {xx2 yy2} $xyf2 {}
                lappend xylist $xx2 $yy2
                if {$contour_options(filled_contour)} {
                    set pxylist $xylist
                    lappend pxylist $x1 $y1 $x2 $y2
                    C_polygon $canv $pxylist [lindex $colorMap $ic] $doTrans
                    if {$ic == $imax} {
                        set pxylist $xylist
                        lappend pxylist $x3 $y3
                        C_polygon $canv $pxylist [lindex $colorMap $ic] $doTrans
                    }
                }
                C_line $canv $xylist [lindex $colorMap $ic] $doTrans
                set oldlist {}
                set x1 $xx1; set y1 $yy1; set f1 $ff
                set x2 $xx2; set y2 $yy2; set f2 $ff
                if {$ic == $imax} {
                    if {$contour_options(filled_contour)} {
                        set pxylist [list $x1 $y1 $x2 $y2 $x3 $y3]
                        set ictmp [expr {$ic + 1}]
                        C_polygon $canv $pxylist [lindex $colorMap $ictmp] $doTrans
                        return
                    }
                }
            }

            "++-" {
#debug#                puts "class C = $class , $ic , $ff"
                set xyf1  [fintpl $x2 $y2 $f2 $x3 $y3 $f3 $ff]
                set xyf2  [fintpl $x3 $y3 $f3 $x1 $y1 $f1 $ff]
                set xylist $xyf1
                foreach {xx1 yy1} $xyf1 {}
                foreach {xx2 yy2} $xyf2 {}
                lappend xylist $xx2 $yy2
                if {$contour_options(filled_contour)} {
                    set pxylist $xylist
                    lappend pxylist $x3 $y3
                    C_polygon $canv $pxylist [lindex $colorMap $ic] $doTrans
                }

                if {$ic == $imax} {
                    if {$contour_options(filled_contour)} {
                        set pxylist $xylist
                        lappend pxylist $x1 $y1 $x2 $y2
                        set ictmp [expr {$ic + 1}]
                        C_polygon $canv $pxylist [lindex $colorMap $ictmp] $doTrans
                    }

                } else {

#debug#                    puts "call Tri_contour : 1) $class"
#debug#                    puts "   : $xx1 $yy1 $ff $xx2 $yy2 $ff $x1 $y1 $f1"
                    Tri_contour $canv $xx1 $yy1 $ff $xx2 $yy2 $ff $x1 $y1 $f1 $cont $doTrans

#debug#                    puts "call Tri_contour : 2) $class"
#debug#                    puts "   : $xx1 $yy1 $ff $x1 $y1 $f1 $x2 $y2 $f2"
                    Tri_contour $canv $xx1 $yy1 $ff $x1 $y1 $f1 $x2 $y2 $f2 $cont $doTrans

                }
                C_line $canv $xylist [lindex $colorMap $ic] $doTrans
                return

            }

            "-+-" {
                ############### Case G ###############

#debug#                puts "class G = $class , $ic , $ff"
                set xyf1  [fintpl $x1 $y1 $f1 $x2 $y2 $f2 $ff]
                set xyf2  [fintpl $x2 $y2 $f2 $x3 $y3 $f3 $ff]
                set xylist $xyf1
                foreach {xx1 yy1} $xyf1 {}
                foreach {xx2 yy2} $xyf2 {}
                lappend xylist $xx2 $yy2
                if {$contour_options(filled_contour)} {
                    set pxylist $xylist
                    lappend pxylist $x3 $y3 $x1 $y1
                    C_polygon $canv $pxylist [lindex $colorMap $ic] $doTrans
                    if {$ic == $imax} {
                        set pxylist $xylist
                        lappend pxylist $x2 $y2
                        C_polygon $canv $pxylist [lindex $colorMap $ic] $doTrans
                    }
                }
                C_line $canv $xylist [lindex $colorMap $ic] $doTrans
                set oldlist {}
                set x1 $xx1; set y1 $yy1; set f1 $ff
                set x3 $xx2; set y3 $yy2; set f3 $ff

                if {$ic == $imax} {
                    if {$contour_options(filled_contour)} {
                        set pxylist [list $x1 $y1 $x2 $y2 $x3 $y3]
                        set ictmp [expr {$ic + 1}]
                        C_polygon $canv $pxylist [lindex $colorMap $ictmp] $doTrans
                        return
                    }
                }

            }

            "+-+" {
#debug#                puts "class G = $class , $ic , $ff"
                set xyf1  [fintpl $x1 $y1 $f1 $x2 $y2 $f2 $ff]
                set xyf2  [fintpl $x2 $y2 $f2 $x3 $y3 $f3 $ff]
                foreach {xx1 yy1} $xyf1 {}
                foreach {xx2 yy2} $xyf2 {}
                set xylist $xyf1
                lappend xylist $xx2 $yy2
                if {$contour_options(filled_contour)} {
                    set pxylist $xylist
                    lappend pxylist $x2 $y2
                    C_polygon $canv $pxylist [lindex $colorMap $ic] $doTrans
                }

                if {$ic == $imax} {
                    if {$contour_options(filled_contour)} {
                        set pxylist $xylist
                        lappend pxylist $x3 $y3 $x1 $y1
                        set ictmp [expr {$ic + 1}]
                        C_polygon $canv $pxylist [lindex $colorMap $ictmp] $doTrans
                    }

                } else {

#debug#                    puts "call Tri_contour : 1) $class"
#debug#                    puts "   : $xx1 $yy1 $ff $xx2 $yy2 $ff $x3 $y3 $f3"
                    Tri_contour $canv $xx1 $yy1 $ff $xx2 $yy2 $ff $x3 $y3 $f3 $cont $doTrans

#debug#                    puts "call Tri_contour : 2) $class"
#debug#                    puts "   : $xx1 $yy1 $ff $x3 $y3 $f3 $x1 $y1 $f1"
                    Tri_contour $canv $xx1 $yy1 $ff $x3 $y3 $f3 $x1 $y1 $f1 $cont $doTrans
                }
                C_line $canv $xylist [lindex $colorMap $ic] $doTrans
                return

            }

            "+--" {
                ############### Case I ###############

#debug#                puts "class I = $class , $ic , $ff"
                set xyf1  [fintpl $x1 $y1 $f1 $x2 $y2 $f2 $ff]
                set xyf2  [fintpl $x3 $y3 $f3 $x1 $y1 $f1 $ff]
                set xylist $xyf1
                foreach {xx1 yy1} $xyf1 {}
                foreach {xx2 yy2} $xyf2 {}
                lappend xylist $xx2 $yy2
                if {$contour_options(filled_contour)} {
                    set pxylist $xylist
                    lappend pxylist $x3 $y3 $x2 $y2
                    C_polygon $canv $pxylist [lindex $colorMap $ic] $doTrans
                    if {$ic == $imax} {
                        set pxylist $xylist
                        lappend pxylist $x1 $y1
                        C_polygon $canv $pxylist [lindex $colorMap $ic] $doTrans
                    }
                }
                C_line $canv $xylist [lindex $colorMap $ic] $doTrans
                set oldlist {}
                set x2 $xx1; set y2 $yy1; set f2 $ff
                set x3 $xx2; set y3 $yy2; set f3 $ff

                if {$ic == $imax} {
                    if {$contour_options(filled_contour)} {
                        set pxylist [list $x1 $y1 $x2 $y2 $x3 $y3]
                        set ictmp [expr {$ic + 1}]
                        C_polygon $canv $pxylist [lindex $colorMap $ictmp] $doTrans
                        return
                    }
                }

            }

            "-++" {
#debug#                puts "class I = $class , $ic , $ff"
                set xyf1  [fintpl $x1 $y1 $f1 $x2 $y2 $f2 $ff]
                set xyf2  [fintpl $x3 $y3 $f3 $x1 $y1 $f1 $ff]
                foreach {xx1 yy1} $xyf1 {}
                foreach {xx2 yy2} $xyf2 {}
                set xylist $xyf1
                lappend xylist $xx2 $yy2
                if {$contour_options(filled_contour)} {
                    set pxylist $xylist
                    lappend pxylist $x1 $y1
                    C_polygon $canv $pxylist [lindex $colorMap $ic] $doTrans
                }

                if {$ic == $imax} {
                    if {$contour_options(filled_contour)} {
                        set pxylist $xylist
                        lappend pxylist $x3 $y3 $x2 $y2
                        set ictmp [expr {$ic + 1}]
                        C_polygon $canv $pxylist [lindex $colorMap $ictmp] $doTrans
                    }

                } else {

#debug#                    puts "call Tri_contour : 1) $class"
#debug#                    puts "   : $xx1 $yy1 $ff $xx2 $yy2 $ff $x3 $y3 $f3"
                    Tri_contour $canv $xx1 $yy1 $ff $xx2 $yy2 $ff $x3 $y3 $f3 $cont $doTrans

#debug#                    puts "call Tri_contour : 2) $class"
#debug#                    puts "   : $xx1 $yy1 $ff $x3 $y3 $f3 $x2 $y2 $f2"
                    Tri_contour $canv $xx1 $yy1 $ff $x3 $y3 $f3 $x2 $y2 $f2 $cont $doTrans
                }
                C_line $canv $xylist [lindex $colorMap $ic] $doTrans
                return

            }

        }

    }
}

# setFLevel --
#     Auxiliary function: used to classify one functional value to another
# Arguments:
#     f1          Second break point and value
#     f2          Value to find
# Result:
#     +    f1 > f2
#     =    f1 = f2
#     -    f1 < f2
#
proc ::Plotchart::setFLevel {f1 f2} {
    if {$f1 > $f2} {
        return "+"
    } else {
        if {$f1 < $f2} {
            return "-"
        } else {
            return "="
        }
    }
}

# fintpl --
#     Auxiliary function: inverse interpolation
# Arguments:
#     x1,y1,f1    First break point and value
#     x2,y2,f2    Second break point and value
#     ff          Value to find
# Result:
#     x,y coordinates of point with that value
#
proc ::Plotchart::fintpl {x1 y1 f1 x2 y2 f2 ff} {

    if {[expr {$f2 - $f1}] != 0.0} {
        set xx  [expr {$x1 + (($ff - $f1)*($x2 - $x1)/($f2 - $f1))}]
        set yy  [expr {$y1 + (($ff - $f1)*($y2 - $y1)/($f2 - $f1))}]
    } else {

        # If the logic was handled correctly above, this point
        # should never be reached.
        #
        # puts "FINTPL : f1 == f2 : x1,y1 : $x1 , $y1 : x2,y2 : $x2 , $y2"
        set xx $x1
        set yy $y1
    }

    set xmin [min $x1 $x2]
    set xmax [max $x1 $x2]
    set ymin [min $y1 $y2]
    set ymax [max $y1 $y2]

    if {$xx < $xmin} { set xx $xmin }
    if {$xx > $xmax} { set xx $xmax }
    if {$yy < $ymin} { set yy $ymin }
    if {$yy > $ymax} { set yy $ymax }

    return [list $xx $yy]
}

# min --
#     Auxiliary function: find the minimum of the arguments
# Arguments:
#     val         First value
#     args        All others
# Result:
#     Minimum over the arguments
#
proc ::Plotchart::min { val args } {
    set min $val
    foreach val $args {
        if { $val < $min } {
            set min $val
        }
    }
    return $min
}

# max --
#     Auxiliary function: find the maximum of the arguments
# Arguments:
#     val         First value
#     args        All others
# Result:
#     Maximum over the arguments
#
proc ::Plotchart::max { val args } {
    set max $val
    foreach val $args {
        if { $val > $max } {
            set max $val
        }
    }
    return $max
}

# C_line --
#     Draw a line
# Arguments:
#     canv        Canvas to draw in
#     xylist      List of raw coordinates
#     color       Chosen colour
#     args        Any additional arguments (for line style and such)
# Result:
#     None
#
proc ::Plotchart::C_line {canv xylist color {doTrans 1} } {

    if {$doTrans} {
        set wxylist {}
        foreach {xx yy} $xylist {
            foreach {pxcrd pycrd} [::Plotchart::coordsToPixel $canv $xx $yy] {break}
            lappend wxylist $pxcrd $pycrd
        }
        eval "$canv create line $wxylist -fill $color"

    } else {
        $canv create line $xylist -fill $color
    }
}

# C_polygon --
#     Draw a polygon
# Arguments:
#     canv        Canvas to draw in
#     xylist      List of raw coordinates
#     color       Chosen colour
#     args        Any additional arguments (for line style and such)
# Result:
#     None
#
proc ::Plotchart::C_polygon {canv xylist color {doTrans 1}} {

    if {$doTrans} {
        set wxylist {}
        foreach {xx yy} $xylist {
            foreach {pxcrd pycrd} [::Plotchart::coordsToPixel $canv $xx $yy] {break}
            lappend wxylist $pxcrd $pycrd
        }
        eval "$canv create polygon $wxylist -fill $color"

    } else {
        $canv create polygon $xylist -fill $color
    }
}

# MakeContourClasses --
#     Return contour classes and colours
# Arguments:
#     values      List of values
#     classes     Given list of classes or class/colours
# Result:
#     List of pairs of class limits and colours
# Note:
#     This should become more sophisticated!
#
proc ::Plotchart::MakeContourClasses {values classes} {
    variable contour_options
    variable colorMap

    if { [llength $classes] == 0 } {
        set min {}
        set max {}
        foreach row $values {
            foreach v $row {
                if { $v == {} } {continue}

                if { $min == {} || $min > $v } {
                    set min $v
                }

                if { $max == {} || $max < $v } {
                    set max $v
                }
            }
        }

        foreach {xmin xmax xstep} [determineScale $min $max] {break}

        #
        # The contour classes must encompass all values
        # There might be a problem with border cases
        #
        set classes {}
        set x $xmin

        while { $x < $xmax+0.5*$xstep } {
            #mbs# lappend classes [list $x]
            set  x [expr {$x+$xstep}]
            lappend classes [list $x]
        }

        # Now that we know how many entries (ncont), create
        # the colormap colors
        #
        ::Plotchart::setColormapColors  [expr {[llength $classes] + 1}]

    } elseif { [llength [lindex $classes 0]] == 1 } {
        #mbs#  Changed the above line from " == 2 " to " == 1 "
        ::Plotchart::setColormapColors  [llength $classes]
        return $classes
    }

    #
    # Add the colours
    #
#####    set cont {}
#####    set c 0
#####
#####    foreach x $classes {
#####        set col [lindex $contour_options(colourmap) $c]
#####        if { $col == {} } {
#####            set c 0
#####            set col [lindex $contour_options(colourmap) $c]
#####        }
#####        lappend cont [list $x $col]
#####        incr c
#####    }
#####
#####    return $cont

#debug#    puts "classes (cont) : $classes"

    return $classes
}


# setColormapColors --
#     Auxiliary function: Based on the current colormap type
#     create a colormap with requested number of entries
# Arguments:
#     ncont       Number of colors in the colormap
# Result:
#     List of colours
#
proc ::Plotchart::setColormapColors  {ncont} {
    variable colorMapType
    variable colorMap

#debug#    puts "SetColormapColors : ncont = $ncont"

    # Note : The default colormap is "jet"

    switch -- $colorMapType {

        custom {
            return
        }

        hsv {
            set hueStart     0.0
            set hueEnd     240.0
            set colorMap   {}

            for {set i 0} {$i <= $ncont} {incr i} {
                set dh [expr {($hueStart - $hueEnd) / ($ncont - 1)}]
                set hue  [expr {$hueStart - ($i * $dh)}]
                if {$hue < 0.0} {
                    set hue  [expr {360.0 + $hue}]
                }
                set rgbList [Hsv2rgb $hue 1.0 1.0]
                set r    [expr {int([lindex $rgbList 0] * 65535)}]
                set g    [expr {int([lindex $rgbList 1] * 65535)}]
                set b    [expr {int([lindex $rgbList 2] * 65535)}]

                set color  [format "#%.4x%.4x%.4x" $r $g $b]
                lappend colorMap $color
            }
        }

        hot {
            set colorMap {}
            set nc1          [expr {int($ncont * 0.33)}]
            set nc2          [expr {int($ncont * 0.67)}]

            for {set i 0} {$i <= $ncont} {incr i} {

                if {$i <= $nc1} {
                    set fval  [expr { double($i) / (double($nc1)) } ]
                    set r     [expr {int($fval * 65535)}]
                    set g     0
                    set b     0
                } else {
                    if {$i <= $nc2} {
                        set fval  [expr { double($i-$nc1) / (double($nc2-$nc1)) } ]
                        set r     65535
                        set g     [expr {int($fval * 65535)}]
                        set b     0
                    } else {
                        set fval  [expr { double($i-$nc2) / (double($ncont-$nc2)) } ]
                        set r     65535
                        set g     65535
                        set b     [expr {int($fval * 65535)}]
                    }
                }
                set color  [format "#%.4x%.4x%.4x" $r $g $b]
                lappend colorMap $color
            }
        }

        cool {
            set colorMap {}

            for {set i 0} {$i <= $ncont} {incr i} {

                set fval  [expr { double($i) / (double($ncont)-1) } ]
                set val   [expr { 1.0 - $fval }]

                set r    [expr {int($fval * 65535)}]
                set g    [expr {int($val * 65535)}]
                set b    65535

                set color  [format "#%.4x%.4x%.4x" $r $g $b]
                lappend colorMap $color
            }
        }

        grey -
        gray {
            set colorMap {}

            for {set i 0} {$i <= $ncont} {incr i} {

                set fval  [expr { double($i) / (double($ncont)-1) } ]
                set val  [expr {0.4 + (0.5 * $fval) }]

                set r    [expr {int($val * 65535)}]
                set g    [expr {int($val * 65535)}]
                set b    [expr {int($val * 65535)}]

                set color  [format "#%.4x%.4x%.4x" $r $g $b]
                lappend colorMap $color
            }
        }

        jet -
        default {
            set hueStart   240.0
            set hueEnd       0.0
            set colorMap   {}

            for {set i 0} {$i <= $ncont} {incr i} {
                set dh [expr {($hueStart - $hueEnd) / ($ncont - 1)}]
                set hue  [expr {$hueStart - ($i * $dh)}]
                if {$hue < 0.0} {
                    set hue  [expr {360.0 + $hue}]
                }
                set rgbList [Hsv2rgb $hue 1.0 1.0]
                set r    [expr {int([lindex $rgbList 0] * 65535)}]
                set g    [expr {int([lindex $rgbList 1] * 65535)}]
                set b    [expr {int([lindex $rgbList 2] * 65535)}]

                set color  [format "#%.4x%.4x%.4x" $r $g $b]
                lappend colorMap $color
            }
        }

    }
}

# colorMap --
#     Define the current colormap type
# Arguments:
#     cmap        Type of colormap
# Result:
#     Updated the internal variable "colorMapType"
# Note:
#     Possibly handle "custom" colormaps differently
#     At present, if the user passes in a list (length > 1)
#     rather than a string, then it is assumes that (s)he
#     passed in a list of colors.
#
proc ::Plotchart::colorMap {cmap} {
    variable colorMapType
    variable colorMap

    switch -- $cmap {

        "grey" -
        "gray" { set colorMapType $cmap }

        "jet"  { set colorMapType $cmap }

        "hot"  { set colorMapType $cmap }

        "cool" { set colorMapType $cmap }

        "hsv"  { set colorMapType $cmap }

        default {
            if {[string is alpha $cmap] == 1} {
                puts "Colormap : Unknown colorMapType, $cmap.  Using JET"
                set colorMapType jet

            } else {
                if {[llength $cmap] > 1} {
                    set colorMapType "custom"
                    set colorMap     $cmap
                }
            }
        }
    }
}



########################################################################
#  The following two routines were borrowed from :
#
#        http://mini.net/cgi-bin/wikit/666.html
########################################################################

# Rgb2hsv --
#
#       Convert a color value from the RGB model to HSV model.
#
# Arguments:
#       r g b  the red, green, and blue components of the color
#               value.  The procedure expects, but does not
#               ascertain, them to be in the range 0 to 1.
#
# Results:
#       The result is a list of three real number values.  The
#       first value is the Hue component, which is in the range
#       0.0 to 360.0, or -1 if the Saturation component is 0.
#       The following to values are Saturation and Value,
#       respectively.  They are in the range 0.0 to 1.0.
#
# Credits:
#       This routine is based on the Pascal source code for an
#       RGB/HSV converter in the book "Computer Graphics", by
#       Baker, Hearn, 1986, ISBN 0-13-165598-1, page 304.
#
proc ::Plotchart::Rgb2hsv {r g b} {
    set h [set s [set v 0.0]]
    set sorted [lsort -real [list $r $g $b]]
    set v [expr {double([lindex $sorted end])}]
    set m [lindex $sorted 0]

    set dist [expr {double($v-$m)}]
    if {$v} {
        set s [expr {$dist/$v}]
    }
    if {$s} {
        set r' [expr {($v-$r)/$dist}] ;# distance of color from red
        set g' [expr {($v-$g)/$dist}] ;# distance of color from green
        set b' [expr {($v-$b)/$dist}] ;# distance of color from blue
        if {$v==$r} {
            if {$m==$g} {
                set h [expr {5+${b'}}]
            } else {
                set h [expr {1-${g'}}]
            }
        } elseif {$v==$g} {
            if {$m==$b} {
                set h [expr {1+${r'}}]
            } else {
                set h [expr {3-${b'}}]
            }
        } else {
            if {$m==$r} {
                set h [expr {3+${g'}}]
            } else {
                set h [expr {5-${r'}}]
            }
        }
        set h [expr {$h*60}]          ;# convert to degrees
    } else {
        # hue is undefined if s == 0
        set h -1
    }
    return [list $h $s $v]
}

# Hsv2rgb --
#
#       Convert a color value from the HSV model to RGB model.
#
# Arguments:
#       h s v  the hue, saturation, and value components of
#               the color value.  The procedure expects, but
#               does not ascertain, h to be in the range 0.0 to
#               360.0 and s, v to be in the range 0.0 to 1.0.
#
# Results:
#       The result is a list of three real number values,
#       corresponding to the red, green, and blue components
#       of a color value.  They are in the range 0.0 to 1.0.
#
# Credits:
#       This routine is based on the Pascal source code for an
#       HSV/RGB converter in the book "Computer Graphics", by
#       Baker, Hearn, 1986, ISBN 0-13-165598-1, page 304.
#
proc ::Plotchart::Hsv2rgb {h s v} {
    set v [expr {double($v)}]
    set r [set g [set b 0.0]]
    if {$h == 360} { set h 0 }
    # if you feed the output of rgb2hsv back into this
    # converter, h could have the value -1 for
    # grayscale colors.  Set it to any value in the
    # valid range.
    if {$h == -1} { set h 0 }
    set h [expr {$h/60}]
    set i [expr {int(floor($h))}]
    set f [expr {$h - $i}]
    set p1 [expr {$v*(1-$s)}]
    set p2 [expr {$v*(1-($s*$f))}]
    set p3 [expr {$v*(1-($s*(1-$f)))}]
    switch -- $i {
        0 { set r $v  ; set g $p3 ; set b $p1 }
        1 { set r $p2 ; set g $v  ; set b $p1 }
        2 { set r $p1 ; set g $v  ; set b $p3 }
        3 { set r $p1 ; set g $p2 ; set b $v  }
        4 { set r $p3 ; set g $p1 ; set b $v  }
        5 { set r $v  ; set g $p1 ; set b $p2 }
    }
    return [list $r $g $b]
}
# DrawIsolinesFunctionValues --
#     Draw isolines in the given grid with given function values.
# Arguments:
#     canv : Canvas to draw in
#     xvec : List of points in the x axis
#     yvec : List of points in the y axis
#     fmat : Matrix of function values at the points defined by x and y.
#       The matrix is given as a list of rows, where each row
#       stores the function values with a fixed y and a varying x.
#     cont        List of contour classes (or empty to indicate
#                 automatic scaling
# Result:
#     None
# Side effect:
#     Isolines drawn
# Note:
#     A cell is only drawn if there are four well-defined
#     corners. If the x or y coordinate is missing or the value is
#     missing, the cell is skipped.
# Author : Michael Baudin
#
proc ::Plotchart::DrawIsolinesFunctionValues {canv xvec yvec fmat {cont {}}} {
  variable scaling
  set nx [llength $xvec]
  set ny [llength $xvec]
  if {$nx!=$ny} then {
    error "The number of values in xvec (nx=$nx) is different from the number of values in yvec (ny=$ny)"
  }
  #
  # Check the given values of xvec and yvec against the scaling of the plot,
  # which was given at the creation of the plot.
  #
  set index 0
  foreach xval $xvec {
    if {$xval > $scaling($canv,xmax) || $xval < $scaling($canv,xmin)} then {
      error "The given x value $xval at index $index of xvec is not in the x-axis range \[$scaling($canv,xmin),$scaling($canv,xmax)\]"
    }
    incr index
  }
  set index 0
  foreach yval $yvec {
    if {$yval > $scaling($canv,ymax) || $yval < $scaling($canv,ymin)} then {
      error "The given y value $yval at index $index of yvec is not in the y-axis range \[$scaling($canv,ymin),$scaling($canv,ymax)\]"
    }
    incr index
  }
  #
  # Form the xmat and ymat matrices based on the given x and y.
  #
  set xmat {}
  for {set iy 0} {$iy < $ny} {incr iy} {
    set xi {}
    for {set ix 0} {$ix < $nx} {incr ix} {
      lappend xi [lindex $xvec $ix]
    }
    lappend xmat $xi
  }
  set ymat {}
  for {set iy 0} {$iy < $ny} {incr iy} {
    set yi {}
    set yiy [lindex $yvec $iy]
    for {set ix 0} {$ix < $nx} {incr ix} {
      lappend yi $yiy
    }
    lappend ymat $yi
  }
  DrawIsolines $canv $xmat $ymat $fmat $cont
}

#
# Define default colour maps
#
namespace eval ::Plotchart {
     set contour_options(colourmap,rainbow) \
        {darkblue blue cyan green yellow orange red magenta}
     set contour_options(colourmap,white-blue) \
        {white paleblue cyan blue darkblue}

     set contour_options(colourmap,detailed) {
#00000000ffff
#000035e4ffff
#00006bc9ffff
#0000a1aeffff
#0000d793ffff
#0000fffff285
#0000ffffbca0
#0000ffff86bc
#0000ffff50d7
#0000ffff1af2
#1af2ffff0000
#50d7ffff0000
#86bcffff0000
#bca0ffff0000
#f285ffff0000
#ffffd7930000
#ffffa1ae0000
#ffff6bc90000
#ffff35e40000
#ffff00000000
#ffff00000000
}
    set contour_options(colourmap) $contour_options(colourmap,detailed)
}
# End of plotcontour.tcl
