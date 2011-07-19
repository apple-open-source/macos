# plotdemos11.tcl --
#     Test and demonstrate log X-Y plots and log-log plots
#
package require Plotchart 1.8

#
# Log X plot of y = log(x)
#
pack [canvas .c1 -bg white]

set p [::Plotchart::createLogXYPlot .c1 {1 1000} {0 5 1}]

foreach x {1 2 5 10 20 50 100 200 500 1000} {
    $p plot series1 $x [expr {log($x)}]
}

$p title "y = log(x)"

#
# Log-log plot - use y = x**n
#
pack [canvas .c2 -bg white] -side top

set p [::Plotchart::createLogXLogYPlot .c2 {1 1000} {1 1e6}]

$p dataconfig series1 -colour green
$p dataconfig series2 -colour blue
$p dataconfig series3 -colour red

foreach x {1 2 5 10 20 50 100 200 500 1000} {
    $p plot series1 $x [expr {sqrt($x)}]
    $p plot series2 $x [expr {$x*$x}]
    $p plot series3 $x [expr {$x*sqrt($x)}]
}

$p title "y = x**n, n = 1/2, 2, 3/2"

$p xticklines grey

