#! /bin/sh
# -*- tcl -*- \
exec tclsh "$0" ${1+"$@"}

package require Tcl 8.4
package require Tk

package require Plotchart

# plotdemos4.tcl --
#     Show a 3D bar chart
#

canvas .c  -width 400 -height 400 -bg white
toplevel .t
canvas .t.c2 -width 300 -height 200 -bg white
canvas .t.c3 -width 300 -height 200 -bg white
canvas .t.c4 -width 300 -height 200 -bg white
pack   .c -fill both
pack   .t.c2 .t.c3 .t.c4 -fill both

#
# 3D barchart
#
set s [::Plotchart::create3DBarchart .c {-200.0 900.0 100.0} 7]

foreach {bar value} {red 765 green 234 blue 345 yellow 321
                     magenta 567 cyan -123 white 400} {

    $s plot $bar $value $bar
}

$s title "3D Bars"

$s balloon 1.2 100 "Arrow pointing\nat second bar" south-east

#
# Three styles of radial charts
#
foreach {style canvas} {lines .t.c2 cumulative .t.c3 filled .t.c4} {
    set s [::Plotchart::createRadialchart $canvas {A B LongerName C D} 10.0 $style]

    $s plot {1 2 3 4 3} green 2
    $s plot {4 5 0 1 4} red   3

    $s title "Sample of a radial chart - style: $style"
}
