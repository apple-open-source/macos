#! /bin/sh
# -*- tcl -*- \
exec tclsh "$0" ${1+"$@"}

package require Tcl 8.3
package require Tk
source ../../modules/plotchart/plotchart.tcl
package require Plotchart

# testplot.tcl --
#    Test program for the Plotchart package
#

#
# Main code
#
canvas .c  -background white -width 400 -height 200
canvas .c2 -background white -width 400 -height 200
canvas .c3 -background white -width 400 -height 200
pack   .c .c2 .c3 -fill both -side top

toplevel .h
canvas .h.c  -background white -width 400 -height 200
canvas .h.c2 -background white -width 400 -height 200
pack   .h.c .h.c2 -fill both -side top

toplevel .v
canvas .v.c  -background white -width 400 -height 200
canvas .v.c2 -background white -width 400 -height 200
canvas .v.c3 -background white -width 400 -height 200
pack   .v.c .v.c2 .v.c3 -fill both -side top

set s [::Plotchart::createXYPlot .c {0.0 100.0 10.0} {0.0 100.0 20.0}]

set xd    5.0
set yd   20.0
set xold  0.0
set yold 50.0

$s dataconfig series1 -colour "red"

for { set i 0 } { $i < 20 } { incr i } {
   set xnew [expr {$xold+$xd}]
   set ynew [expr {$yold+(rand()-0.5)*$yd}]
   set ynew2 [expr {$yold+(rand()-0.5)*2.0*$yd}]
   $s plot series1 $xnew $ynew
   $s plot series2 $xnew $ynew2
   set xold $xnew
   set yold $ynew
}

$s xtext "X-coordinate"
$s ytext "Y-data"
$s title "Aha!"

tkwait visibility .c
$s saveplot "aha.ps"


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


set s [::Plotchart::createBarchart .h.c {A B C D E} {0.0 10.0 2.0} 2]

$s plot series1 {1.0 4.0 6.0 1.0 7.0} red
$s plot series2 {0.0 3.0 7.0 9.3 2.0} green
$s title "Arbitrary data"


set s [::Plotchart::createBarchart .h.c2 {A B C D E} {0.0 20.0 5.0} stacked]

$s plot series1 {1.0 4.0 6.0 1.0 7.0} red
$s plot series2 {0.0 3.0 7.0 9.3 2.0} green
$s title "Stacked diagram"



set s [::Plotchart::createHorizontalBarchart .v.c {0.0 10.0 2.0} {A B C D E} 2]

$s plot series1 {1.0 4.0 6.0 1.0 7.0} red
$s plot series2 {0.0 3.0 7.0 9.3 2.0} green
$s title "Arbitrary data"


set s [::Plotchart::createHorizontalBarchart .v.c2 {0.0 20.0 5.0} {A B C D E} stacked]

$s plot series1 {1.0 4.0 6.0 1.0 7.0} red
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

proc cowboyhat {x y} {
   set x1 [expr {$x/9.0}]
   set y1 [expr {$y/9.0}]

   expr { 3.0 * (1.0-($x1*$x1+$y1*$y1))*(1.0-($x1*$x1+$y1*$y1)) }
}

toplevel .h3
canvas .h3.c  -bg white -width 400 -height 300
canvas .h3.c2 -bg white -width 400 -height 250
pack .h3.c .h3.c2

set s [::Plotchart::create3DPlot .h3.c {0 10 3} {-10 10 10} {0 10 2.5}]
$s title "3D Plot"
$s plotfunc cowboyhat

set s [::Plotchart::create3DPlot .h3.c2 {0 10 3} {-10 10 10} {0 10 2.5}]
$s title "3D Plot - data "
$s colour "green" "black"
$s plotdata { {1.0 2.0 1.0 0.0} {1.1 3.0 1.1 -0.5} {3.0 1.0 4.0 5.0} }
