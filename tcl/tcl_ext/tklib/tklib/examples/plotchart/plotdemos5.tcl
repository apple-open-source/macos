#! /bin/sh
# the next line restarts with tclsh \
exec tclsh "$0" ${1+"$@"}

package require Tcl 8.4
package require Tk
package require Plotchart

# plotdemos5.tcl --
#     Contour and isoline plots
#
proc cowboyhat {x y} {
   set x1 [expr {$x/9.0}]
   set y1 [expr {$y/9.0}]

   expr { 3.0 * (1.0-($x1*$x1+$y1*$y1))*(1.0-($x1*$x1+$y1*$y1)) }
}


#
# Main code
#
set choice 1

if {$choice == 0} {

set x { {0.0 1.0 2.0 3.0}
        {0.0 1.0 2.0 3.0}
        {0.0 1.0 2.0 3.0}
        {0.0 1.0 2.0 3.0} }

set y { {0.0 0.0 0.0 0.0}
        {1.0 1.0 1.0 1.0}
        {2.0 2.0 2.0 2.0}
        {3.0 3.0 3.0 3.0} }


set f { {0.0 0.0 2.0 3.0}
        {0.0 0.0 2.0 3.0}
        {2.0 2.0 3.0 4.0}
        {3.0 3.0 4.0 5.0} }

set contours [list 1.0 2.0 3.0 4.0 5.0 ]

# set contours [list 1.0 1.3 1.6 2.0 2.3 2.6 3.0 3.3 3.6 4.0 4.3 4.6 5.0 5.3 ]

set xlimits {0 3.5 0.5}
set ylimits {0 3.5 0.5}

}


if {$choice == 1} {

set x { {0.0 100.0 200.0}
        {0.0 100.0 200.0}
        {0.0 100.0 200.0}
        {0.0 100.0 200.0}}
set y { {0.0   0.0   0.0}
       {30.0  30.0  30.0}
       {60.0  60.0  60.0}
       {90.0  90.0  90.0}}
set f { {0.0   1.0  10.0}
       { 0.0  30.0  30.0}
       {10.0  60.0  60.0}
       {30.0  90.0  90.0}}

set contours [list \
     0.0             \
     5.2631578947    \
     10.5263157895   \
     15.7894736842   \
     21.0526315789   \
     26.3157894737   \
     31.5789473684   \
     36.8421052632   \
     42.1052631579   \
     47.3684210526   \
     52.6315789474   \
     57.8947368421   \
     63.1578947368   \
     68.4210526316   \
     73.6842105263   \
     78.9473684211   \
     84.2105263158   \
     89.4736842105   \
     94.7368421053   \
     100.0           \
     105.263157895   \
              ]

 set xlimits {0 200 50}
 set ylimits {0 100 20}

}

########################################################################

wm title . "Contour Demo : shade (jet colormap)"

set c [canvas .c  -background white \
          -width 500 -height 500]

pack   $c  -fill both -side top

set chart [::Plotchart::createXYPlot $c $xlimits $ylimits]

::Plotchart::colorMap jet

#$chart contourlines $x $y $f $contours
$chart contourfill $x $y $f $contours
#$chart contourbox $x $y $f $contours
$chart grid $x $y

set t [toplevel .contourlines]
lappend windows $t
wm title $t "Contour Demo : contourlines (default colormap)"
set c [canvas $t.c  -background white \
          -width 500 -height 500]
pack   $c  -fill both -side top

set chart1 [::Plotchart::createXYPlot $c $xlimits $ylimits]
$chart1 grid $x $y
$chart1 contourlines $x $y $f $contours


set t [toplevel .hot]
lappend windows $t
wm title $t "Contour Demo : contourlines (hot colormap)"
set c [canvas $t.c  -background white \
          -width 500 -height 500]
pack   $c  -fill both -side top

set chart2 [::Plotchart::createXYPlot $c $xlimits $ylimits]
::Plotchart::colorMap hot
$chart2 contourfill $x $y $f $contours
$chart2 grid $x $y


set t [toplevel .gray]
lappend windows $t
wm title $t "Contour Demo : gray contourfill , jet contourlines"
set c [canvas $t.c  -background white \
          -width 500 -height 500]
pack   $c  -fill both -side top

set chart3 [::Plotchart::createXYPlot $c $xlimits $ylimits]
::Plotchart::colorMap gray
$chart3 contourfill $x $y $f $contours

::Plotchart::colorMap jet
$chart3 contourlines $x $y $f $contours
$chart3 grid $x $y


set t [toplevel .cool]
lappend windows $t
wm title $t "Contour Demo : contourlines (cool colormap)"
set c [canvas $t.c  -background white \
          -width 500 -height 500]
pack   $c  -fill both -side top

set chart4 [::Plotchart::createXYPlot $c $xlimits $ylimits]
::Plotchart::colorMap cool
$chart4 contourfill $x $y $f $contours
$chart4 grid $x $y



set t [toplevel .defcont]
lappend windows $t
wm title $t "Contour Demo : default contours (jet colormap)"
set c [canvas $t.c  -background white \
          -width 500 -height 500]
pack   $c  -fill both -side top

set chart5 [::Plotchart::createXYPlot $c $xlimits $ylimits]
::Plotchart::colorMap jet
$chart5 contourfill $x $y $f
$chart5 grid $x $y



set t [toplevel .3dcontour]
lappend windows $t
wm title $t "Contour Demo : contours on a 3DPlot"
set c [canvas $t.c  -background white \
          -width 500 -height 500]
pack   $c  -fill both -side top

set xlimits {-10. 10.  10.  }
set ylimits {-10. 10.  10.  }
set zlimits { -5. 10.   5.  }

set zmin   0.0
set zmax   3.0

set nc    51
set dz    [expr {($zmax - $zmin) / ($nc - 1)}]

set contours {}
for {set cnt 1} {$cnt < $nc} {incr cnt} {
    set zval [expr {$zmin + ($dz * ($cnt - 1))}]
    lappend contours $zval
}

set chart6 [::Plotchart::create3DPlot $c $xlimits $ylimits $zlimits]
::Plotchart::colorMap jet
$chart6 title "3D Plot"
$chart6 plotfuncont cowboyhat $contours
