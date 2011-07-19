# plotdemos12.tcl --
#     Test and demonstrate the interactive facilities of Plotchart
#
package require Plotchart 1.8

#
# Procedure for showing the coordinates
#
proc showCoords {xcoord ycoord} {
    global coords

    set coords "Coordinates: $xcoord, $ycoord"
}

#
# Procedure for showing an annotation
#
proc showAnnotation {xcoord ycoord plot w} {

    $plot balloon $xcoord $ycoord "Data point: [format "%.3f, %.3f" $xcoord $ycoord]" north

    after 2000 [list removeAnnotation $w]
}

#
# Procedure for erase an annotation
#
proc removeAnnotation {w} {

    $w delete BalloonText
    $w delete BalloonFrame
}

#
# Create a simple plot and a label
#
pack [canvas .c -bg white] [label .l -textvariable coords]

set p [::Plotchart::createXYPlot .c {0 1000 200} {0 10 1}]

$p dataconfig series1 -type both -symbol cross

foreach x {1 2 5 10 20 50 100 200 500 1000} {
    $p plot series1 $x [expr {log($x)}]

    $p bindlast series1 <Enter> [list showAnnotation $p %W]
}

$p balloon 20 2 "Text" north

$p title "y = log(x)"

$p bindplot <Motion> showCoords
