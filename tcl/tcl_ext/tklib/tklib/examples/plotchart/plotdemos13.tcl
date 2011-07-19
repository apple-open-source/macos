# plotdemos13.tcl --
#     Check the performance of xy-plots with large amounts of data
#
package require Plotchart


#
# Create the XY plot and fill it with several series of data
#
pack [canvas .c -bg white]

set p [::Plotchart::createXYPlot .c {0 100 10} {0 100 10}]

update idletasks

set time0 [clock milliseconds]
for { set i 0} {$i < 1000 } {incr i} {
    set x [expr {100*rand()}]
    set y [expr {100*rand()}]

    $p plot series1 $x $y
}

set time1 [clock milliseconds]

$p dataconfig series2 -type both -symbol cross -colour red
for { set i 0} {$i < 1000 } {incr i} {
    set x [expr {100*rand()}]
    set y [expr {100*rand()}]

    $p plot series2 $x $y
}

set time2 [clock milliseconds]

update idletasks

set time3 [clock milliseconds]

$p dataconfig series3 -type both -symbol cross -colour blue
for { set i 0} {$i < 10000 } {incr i} {
    set x [expr {100*rand()}]
    set y [expr {100*rand()}]

    $p plot series3 $x $y
}

set time4 [clock milliseconds]

catch {
    console show
}

puts "Plotting a line with 1000 points: [expr {$time1-$time0}] ms"
puts "Plotting a line with 1000 symbols (crosses): [expr {$time2-$time1}] ms"
puts "Time to display: [expr {$time3-$time2}] ms"
puts "Plotting a line with 10000 symbols (crosses): [expr {$time4-$time3}] ms"
