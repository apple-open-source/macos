#! /bin/sh
# -*- tcl -*- \
exec tclsh "$0" ${1+"$@"}

package require Tcl 8.3
package require Tk

source ../../modules/plotchart/plotchart.tcl
package require Plotchart

# testplsec.tcl --
#    Second test program for the Plotchart package
#

#
# Main code
#
canvas .c  -background white -width 400 -height 200
canvas .c2 -background white -width 400 -height 200
pack   .c .c2 -fill both -side top

#
# Set up a strip chart
#
set s [::Plotchart::createStripchart .c {0.0 100.0 10.0} {0.0 100.0 20.0}]

proc gendata {slipchart xold xd yold yd} {
   set xnew [expr {$xold+$xd}]
   set ynew [expr {$yold+(rand()-0.5)*$yd}]
   set ynew2 [expr {$yold+(rand()-0.5)*2.0*$yd}]
   $slipchart plot series1 $xnew $ynew
   $slipchart plot series2 $xnew $ynew2

   if { $xnew < 200 } {
   after 500 [list gendata $slipchart $xnew $xd $ynew $yd]
   }
}

after 100 [list gendata $s 0.0 15.0 50.0 30.0]

$s title "Aha!"

#
# Set up an isometric plot
#
set s [::Plotchart::createIsometricPlot .c2 {0.0 100.0} {0.0 200.0} noaxes]
::Plotchart::setZoomPan .c2
$s plot rectangle        10.0 10.0 50.0 50.0 green
$s plot filled-rectangle 20.0 20.0 40.0 40.0 red
$s plot filled-circle    70.0 70.0 40.0 yellow
$s plot circle           70.0 70.0 42.0

#
# Check the symbols
#
toplevel .h
canvas   .h.c -bg white -width 400 -height 200
pack     .h.c -fill both
set s [::Plotchart::createXYPlot .h.c {0.0 100.0 10.0} {0.0 100.0 20.0}]

$s dataconfig series1 -colour red   -type symbol
$s dataconfig series2 -colour green -type both

$s yconfig -format "%12.2e"

set x 5.0
foreach symbol {plus cross circle up down dot upfilled downfilled} {
   $s dataconfig series1 -symbol $symbol
   $s dataconfig series2 -symbol $symbol
   $s plot series1 $x 50.0
   $s plot series2 $x 20
   set x [expr {$x+10}]
}

#
# Second window: XY-plot with background and a Pareto plot
# Note:
# The data series is filled upwards, so that a white polygon
# hides the shading above the line. You need to let the
# series cover the whole axis, otherwise the effect is lost.
#
toplevel .t2
canvas .t2.c  -background white -width 400 -height 200
canvas .t2.c2 -background white -width 400 -height 200
pack .t2.c .t2.c2 -fill both

set s [::Plotchart::createXYPlot .t2.c {0.0 100.0 10.0} {0.0 100.0 20.0}]

$s background gradient green top-down

$s dataconfig series1 -filled up -fillcolour white

$s plot series1  0.0 20.0
$s plot series1 10.0 20.0
$s plot series1 30.0 50.0
$s plot series1 35.0 45.0
$s plot series1 45.0 25.0
$s plot series1 75.0 55.0
$s plot series1 100.0 55.0

$s plaintext 30.0 60.0 "Peak" south

set s2 [::Plotchart::createXYPlot .t2.c2 {0.0 100.0 10.0} {0.0 100.0 20.0}]

set image [image create photo bg -file tcllogo.gif]
$s2 background image $image

#$s2 dataconfig series1 -filled up -fillcolour white

$s2 plot series1  0.0 20.0
$s2 plot series1 10.0 20.0
$s2 plot series1 30.0 50.0
$s2 plot series1 35.0 45.0
$s2 plot series1 45.0 25.0
$s2 plot series1 75.0 55.0
$s2 plot series1 100.0 55.0

$s2 plaintext 30.0 60.0 "Peak" south

#
# Not ready for prime time
if { 0 } {
set s3 [::Plotchart::createBarchart .t2.c3 {{} "Type 1" "Type 2" "Type 3" } \
             {0.0 50.0 10.0} 1]

set s4 [::Plotchart::createRightAxis .t2.c3 {0.0 100.0 20.0}]

set data {0.0 20.0 5.0 30.0}
$s3 plot series1 {0.0 20.0 5.0 30.0} blue
$s4 plot series2  0.5  0.0
$s4 plot series2  1.5 20.0
$s4 plot series2  2.5  5.0
$s4 plot series2  2.5 30.0
}
