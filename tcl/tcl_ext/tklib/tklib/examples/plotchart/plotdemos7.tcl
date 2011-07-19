#! /bin/sh
# -*- tcl -*- \
exec tclsh "$0" ${1+"$@"}

# plotdemos7.tcl --
#     This test/demo script focuses on customising the plots
#

package require Tcl 8.4
package require Tk
package require Plotchart

# plotdemos7.tcl --
#    Test/demo program 7 for the Plotchart package
#

#
# Main code
#
canvas .c  -background white -width 400 -height 200
canvas .c2 -background white -width 400 -height 200
canvas .c3 -background white -width 400 -height 200
canvas .c4 -background white -width 400 -height 200
pack   .c .c2 .c3 .c4 -fill both -side top

toplevel .h
canvas .h.c  -background white -width 400 -height 200
canvas .h.c2 -background white -width 400 -height 200
pack   .h.c .h.c2 -fill both -side top

toplevel .v
canvas .v.c  -background white -width 400 -height 200
canvas .v.c2 -background white -width 400 -height 200
canvas .v.c3 -background white -width 400 -height 200
pack   .v.c .v.c2 .v.c3 -fill both -side top

::Plotchart::plotconfig xyplot title font "Times 14"
::Plotchart::plotconfig xyplot title textcolor "red"
::Plotchart::plotconfig xyplot leftaxis font "Helvetica 10 italic"
::Plotchart::plotconfig xyplot leftaxis thickness 2
::Plotchart::plotconfig xyplot leftaxis ticklength -5
::Plotchart::plotconfig xyplot rightaxis font "Times 10 bold"
::Plotchart::plotconfig xyplot rightaxis color green
::Plotchart::plotconfig xyplot rightaxis thickness 2
::Plotchart::plotconfig xyplot margin right 100

set s [::Plotchart::createXYPlot .c {0.0 100.0 10.0} {0.0 100.0 20.0}]
set r [::Plotchart::createRightAxis .c {0.0 0.1 0.01}]

set xd    5.0
set yd   20.0
set xold  0.0
set yold 50.0

$s dataconfig series1 -colour "red"
$s dataconfig series2 -colour "blue"
$s dataconfig series3 -colour "magenta"

for { set i 0 } { $i < 20 } { incr i } {
   set xnew [expr {$xold+$xd}]
   set ynew [expr {$yold+(rand()-0.5)*$yd}]
   set ynew2 [expr {$yold+(rand()-0.5)*2.0*$yd}]
   $s plot series1 $xnew $ynew
   $s plot series2 $xnew $ynew2
   $s trend series3 $xnew $ynew2
   set xold $xnew
   set yold $ynew
}

$s interval series2 50.0 40.0 60.0 52.0
$s interval series2 60.0 40.0 60.0

$s xtext "X-coordinate"
$s ytext "Y-data"
$r ytext "Right axis"
$s title "Aha!"

#
# Some data for the right axis
#
$r dataconfig right -type both -symbol circle -colour green
$r plot right 10.0 0.01
$r plot right 30.0 0.03
$r plot right 40.0 0.02

tkwait visibility .c
#$s saveplot "aha.ps"


set s [::Plotchart::createPiechart .c2]

$s plot {"Long names" 10 "Short names" 30 "Average" 40
         "Ultra-short names" 5}
#
# Note: title should be shifted up
#       - distinguish a separate title area
#
$s title "Okay - this works"



set s [::Plotchart::createPolarplot .c3 {3.0 1.0}]

for { set angle 0 } { $angle < 360.0 } { set angle [expr {$angle+10.0}] } {
   set rad [expr {1.0+cos($angle*$::Plotchart::torad)}]
   $s plot "cardioid" $rad $angle
}

$s title "Cardioid"


set s [::Plotchart::createBarchart .h.c {A B C D E} {0.0 10.0 2.0} 2.5]

$s legend series1 "Series 1"
$s legend series2 "Series 2"

$s plot series1 {1.0 4.0 6.0 1.0 7.0} red
$s plot series2 {0.0 3.0 7.0 9.3 2.0} green
$s title "Arbitrary data"


set s [::Plotchart::createBarchart .h.c2 {A B C D E} {0.0 20.0 5.0} stacked]

$s plot series1 {1.0 4.0 6.0 1.0 7.0} red
$s plot series2 {0.0 3.0 7.0 9.3 2.0} green
$s title "Stacked diagram"



::Plotchart::plotconfig horizbars leftaxis font "Helvetica 10 italic"
::Plotchart::plotconfig horizbars background outercolor steelblue3
::Plotchart::plotconfig horizbars bottomaxis ticklength -5

set s [::Plotchart::createHorizontalBarchart .v.c {0.0 10.0 2.0} \
         {Antarctica Eurasia "The Americas" "Australia and Oceania" Ocean} 2]

$s plot series1 {1.0 4.0 6.0 1.0 7.0} red left-right
$s plot series2 {0.0 3.0 7.0 9.3 2.0} green right-left
$s title "Arbitrary data"


set s [::Plotchart::createHorizontalBarchart .v.c2 {0.0 20.0 5.0} {A B C D E} stacked]

$s plot series1 {1.0 4.0 6.0 1.0 7.0} red left-right
$s plot series2 {0.0 3.0 7.0 9.3 2.0} green
$s title "Stacked diagram"


set s [::Plotchart::createTimechart .v.c3 "1 january 2004" \
                                          "31 december 2004" 4]

$s period "Spring" "1 march 2004" "1 june 2004" green
$s period "Summer" "1 june 2004" "1 september 2004" yellow
$s vertline "1 jan" "1 january 2004"
$s vertline "1 apr" "1 april 2004"
$s vertline "1 jul" "1 july 2004"
$s vertline "1 oct" "1 october 2004"
$s milestone "Longest day" "21 july 2004"
$s title "Seasons (northern hemisphere)"

