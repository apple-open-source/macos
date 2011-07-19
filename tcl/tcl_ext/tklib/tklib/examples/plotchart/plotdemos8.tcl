#! /bin/sh
# -*- tcl -*- \
exec tclsh "$0" ${1+"$@"}

package require Tcl 8.4
package require Tk

package require Plotchart


# plotdemos8.tcl --
#     Demonstration of a boxplot
#
pack [canvas .c] -fill both -side top

set p [::Plotchart::createBoxplot .c {0 40 5} {A B C D E F}]

$p plot A {0 1 2 5 7 1 4 5 0.6 5 5.5}
$p plot C {2 2 3 6 1.5 3}

$p plot E {2 3 3 4 7 8 9 9 10 10 11 11 11 14 15 17 17 20 24 29}

#
# Demonstration of selected x labels - for version 1.6.2
#
if {0} {
set s [::Plotchart::createXYPlot .c2 {1990 2050 {}} {0.0 100.0 20.0} \
    -xlabels {1990 2020 2030 2050}]

$s xconfig -format "%.0f"

foreach {x y} {1990 32.0 2025 50.0 2030 60.0 2050 11.0 } {
    $s plot series1 $x $y
}

$s title "Data series"
}
