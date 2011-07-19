#! /bin/sh
# -*- tcl -*- \
exec tclsh "$0" ${1+"$@"}

package require Tcl 8.4
package require Tk
package require Plotchart

# plothist.test --
#     Testing histograms
#
    canvas .c -width 600 -height 400 -bg white
    pack   .c -fill both
    .c delete all

    set s [::Plotchart::createHistogram .c {0.0 100.0 10.0} {0.0 100.0 20.0}]

    $s dataconfig series1 -colour green

    set xd    5.0
    set yd   20.0
    set xold  0.0
    set yold 50.0

    for { set i 0 } { $i < 20 } { incr i } {
	set xnew [expr {$xold+$xd}]
	set ynew [expr {$yold+(rand()-0.5)*$yd}]
	$s plot series1 $xnew $ynew
	set xold $xnew
	set yold $ynew
    }

    $s balloonconfig -background green -rimwidth 3 -arrowsize 10 -font "Times 14"
    $s balloon 50 50 "Here it is!"  south-east

    $s balloonconfig -background red
    $s balloonconfig -margin 10
    $s balloon 50 100 "No, here!"  north-east

    $s title "Aha!"
