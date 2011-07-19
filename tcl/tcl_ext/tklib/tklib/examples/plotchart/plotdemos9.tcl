# plotdemos9.tcl --
#     Test wind roses, bands in xy-plots, vertical text and label-dots
#
package require Plotchart


#
# Wind rose diagram
#
pack [canvas .c1 -bg white]

set p [::Plotchart::createWindRose .c1 {30 6} 4]

$p plot {5 10 0 3} red
$p plot {10 10 10 3} blue

$p title "Simple wind rose - margins need to be corrected ..."

#
# Bands in two directions
#
pack [canvas .c2 -bg white] -side top

set p [::Plotchart::createXYPlot .c2 {0 10 2} {0 40 10}]

$p plot data 1 10
$p plot data 6 20
$p plot data 9 10

$p xband 15 25
$p yband 3 5

#
# Label-dots and vertical text
#

pack [canvas .c3 -bg white] -side top

set p [::Plotchart::createXYPlot .c3 {0 10 2} {0 40 10}]

$p labeldot 3 10 "Point 1" w
$p labeldot 6 20 "Point 2" e
$p labeldot 9 10 "Point 3" n
$p labeldot 9 30 "Point 4" s

if { [package require Tk 8.6] } {
    $p vtext "Vertical axis label"
}
