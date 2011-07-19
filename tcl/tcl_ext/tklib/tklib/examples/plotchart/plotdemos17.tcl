# plotdemos17.tcl --
#     Demonstrate how to configure the axes
#
#
source ../../plotchart.tcl
package require Plotchart


#
# Set up three plots
#
pack [canvas .c1 -bg white -width 400 -height 300]
pack [canvas .c2 -bg white -width 400 -height 300]
pack [canvas .c3 -bg white -width 400 -height 200]

set p1 [::Plotchart::createXYPlot .c1 {0 10 2} {-1 1 0.25}]
$p1 ytext "(V)"
$p1 xconfig -ticklength 20 -format %.3f
$p1 yconfig -ticklength 20 -minorticks 4 -labeloffset 10

set p2 [::Plotchart::createXYPlot .c2 {0 10 2} {-10 10 5}]
$p2 ytext "(mA)"

$p2 dataconfig current -colour red
$p2 xconfig -ticklength -6 -minorticks 1

#
# Fill in some data
#
for {set i 0} {$i < 100} {incr i} {
    set phase [expr {2.0*3.1415926*$i/50.0}]

    $p1 plot voltage $phase [expr {0.9*cos($phase)}]
    $p2 plot current $phase [expr {7*sin($phase)}]
}


#
# The third plot uses -xlabes and -ylabels to control
# the labelling
#
set p3 [::Plotchart::createXYPlot .c3 {0 10} {-10 10} -xlabels {1 4 6} -ylabels {-5 0}]
