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
# Note:
# The extremes and the canvas sizes are chosen so that the
# coordinate mapping is isometric!
#
#
canvas .c  -background white -width 400 -height 400
canvas .c2 -background white -width 400 -height 200
pack   .c .c2 -fill both -side top

set s [::Plotchart::createXYPlot .c {0.0 100.0 10.0} {0.0 100.0 20.0}]

$s vectorconfig series1 -colour "red"   -scale 40
$s vectorconfig series2 -colour "blue"  -scale 50 -type nautical -centred 1

#
# Cartesian
#
set data {1.0 0.0 0.0 1.0 0.5 0.5 -2.0 1.0}

set x 30.0
set y 20.0
foreach {u v} $data {
   $s vector series1 $x $y $u $v
}

#
# Nautical
#
set data {1.0 0.0 1.0 45.0 2.0 90.0}

set x 60.0
set y 40.0
foreach {length angle} $data {
   $s vector series2 $x $y $length $angle
}

set s2 [::Plotchart::createXYPlot .c2 {0.0 100.0 10.0} {0.0 100.0 20.0}]

$s2 dotconfig series1 -colour "red" -scalebyvalue 1 -scale 2.5
$s2 dotconfig series2 -colour "magenta" -classes {0 blue 1 green 2 yellow 3 red} \
    -scalebyvalue 0 -outline 0
$s2 dotconfig series3 -colour "magenta" -classes {0 blue 1 green 2 yellow 3 red} \
    -scalebyvalue 1 -scale 2.5

set y1 20
set y2 50
set y3 80
set x  10
foreach value {-1.0 0.5 1.5 2.5 3.5 4.5} {
    $s2 dot series1 $x $y1 $value
    $s2 dot series2 $x $y2 $value
    $s2 dot series3 $x $y3 $value
    set x [expr {$x + 10}]
}

#
# A more interesting vector plot: the forces in a dipole field
#
proc forcesDipole {x y} {
    set xd1 51.0
    set yd1 50.0
    set xd2 49.0
    set yd2 50.0

    set r1p3 [expr {pow(hypot($x-$xd1,$y-$yd1),3.0)}]
    set r2p3 [expr {pow(hypot($x-$xd2,$y-$yd2),3.0)}]

    set fx [expr {($x-$xd1)/$r1p3 - ($x-$xd2)/$r2p3}]
    set fy [expr {($y-$yd1)/$r1p3 - ($y-$yd2)/$r2p3}]

    return [list $fx $fy]
}

toplevel .dipole
canvas .dipole.c -background white -width 500 -height 500
pack   .dipole.c -fill both -side top

set s [::Plotchart::createXYPlot .dipole.c {45.0 55.0 1.0} {45.0 55.0 1.0}]

$s title "Forces in a dipole field"

$s vectorconfig series1 -colour "black" -scale 40 -type polar

$s dotconfig dipole -colour red -scalebyvalue 0 -radius 5
$s dot dipole 49.0 50.0 1.0
$s dot dipole 51.0 50.0 1.0

for {set y 45.25} {$y < 55.0} {set y [expr {$y+0.5}]} {
    for {set x 45.25} {$x < 55.0} {set x [expr {$x+0.5}]} {
        foreach {u v} [forcesDipole $x $y] {break}

        # Scale the vector for better display

        set angle  [expr {180.0*atan2($v,$u)/3.1415926}]
        set length [expr {(0.5+hypot($u,$v))/(1.0+hypot($u,$v))}]

        $s vector series1 $x $y $length $angle
    }
}

#
# Simple demonstration of an R-chart
#
toplevel .rchart
canvas .rchart.c -background white -width 400 -height 200
pack   .rchart.c -fill both -side top

set s [::Plotchart::createXYPlot .rchart.c {0.0 100.0 10.0} {0.0 50.0 10.0}]

$s title "R-chart (arbitrary data)"

$s dataconfig series1 -colour "green"

for {set x 1.0} {$x < 50.0} {set x [expr {$x+3.0}]} {
    set y [expr {20.0 + 3.0*rand()}]
    $s rchart series1 $x $y
}

#
# Now some data outside the expected range
#

$s rchart series1 50.0 41.0
$s rchart series1 52.0 42.0
$s rchart series1 54.0 39.0

#
# And continue with the well-behaved series
#
for {set x 57.0} {$x < 100.0} {set x [expr {$x+3.0}]} {
    set y [expr {20.0 + 3.0*rand()}]
    $s rchart series1 $x $y
}
