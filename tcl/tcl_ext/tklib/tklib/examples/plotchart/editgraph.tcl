# editgraph.tcl --
#     Widget to edit data series graphically
#
#     Note:
#     Not as elegant as could be with bindings via Plotchart itself
#
#package require Plotchart
source plotchart.tcl

namespace eval ::Editgraph {

    variable graph
    variable endedit
}

# editSeries --
#     Procedure to edit a series of data graphically
#
# Arguments:
#     xrange          Range for the x values
#     yrange          Range for the y values
#     number          Number of data points
#
# Result:
#     List of x,y values representing the data series
#
# Note:
#     The widget allows setting new y values for given x values only
#
proc ::Editgraph::editSeries {xrange yrange number} {

    variable graph
    variable endedit

    toplevel .graph
    wm title .graph "Editing series"
    canvas   .graph.c -width 400 -height 300
    button   .graph.ok     -text OK     -width 10 -command {::Editgraph::SaveSeries}
    button   .graph.cancel -text Cancel -width 10 -command {::Editgraph::CancelSeries}
    grid     .graph.c -
    grid     .graph.ok .graph.cancel

    #
    # Initialise the series
    #
    foreach {xmin xmax} $xrange {break}
    foreach {ymin ymax} $yrange {break}

    if { $ymin <= 0.0 && $ymax >= 0.0 } {
        set y 0.0
    } else {
        set y $ymin
    }

    set delx [expr {($xmax-$xmin)/double($number-1)}]

    set graph(xy) {}
    set x         $xmin
    while { $x < $xmax+0.5*$delx } {
        lappend graph(xy) $x $y
        set x [expr {$x + $delx}]
        if { abs($x) < 0.5 *$delx } {
            set x 0.0
        }
    }
    set graph(xmin)   $xmin
    set graph(xmax)   $xmax
    set graph(delx)   $delx
    set graph(number) $number

    set graph(plot) [::Plotchart::createXYPlot .graph.c \
                        [::Plotchart::determineScale $xmin $xmax] \
                        [::Plotchart::determineScale $ymin $ymax]]

    $graph(plot) dataconfig data -colour blue
    set graph(widget) .graph.c

    foreach {x y} $graph(xy) {
        $graph(plot) plot data $x $y
    }

    #
    # Set the bindings
    #
    bind .graph.c <Button-1> {::Editgraph::EditPoint %W %x %y}

    vwait ::Editgraph::endedit
    puts "$endedit"

    if { $endedit == 1 } {
        return $graph(xy)
    } else {
        return {}
    }
}

# SaveSeries, CancelSeries --
#     Callback procedure for the widget
#
# Arguments:
#     None
#
# Result:
#     None
#
# Side effects:
#     The widget is closed
#
proc ::Editgraph::SaveSeries {} {
    set ::Editgraph::endedit 1
    destroy .graph
}

proc ::Editgraph::CancelSeries {} {
    set ::Editgraph::endedit 0
    destroy .graph
}

# EditPoint --
#     Callback procedure for setting the new data point
#
# Arguments:
#     w             Widget
#     x             X-coordinate of selected point (pixel)
#     y             Y-coordinate of selected point (pixel)
#
# Result:
#     None
#
# Side effects:
#     The data points are updated and the graph redrawn
#

proc ::Editgraph::EditPoint {w x y} {
    variable graph

    foreach {xc yc} [::Plotchart::pixelToCoords $graph(widget) $x $y] {break}

    set xp [expr {int(0.5+($xc-$graph(xmin))/$graph(delx))}]

    if { $xp < 0 } { set xp 0 }
    if { $xp > $graph(number) } { set xp $graph(number) }

    lset graph(xy) [expr {2*$xp+1}] $yc

    #
    # Reset the plotted data
    #
    $graph(widget) delete data
    $graph(plot) plot data "" ""

    #
    # Redraw the data
    #
    foreach {x y} $graph(xy) {
        $graph(plot) plot data $x $y
    }

}

# main --
#     Make it all work

catch {
    console show
}
set xyseries [::Editgraph::editSeries {0 100} {-50 50} 10]

puts "Series:"
foreach {x y} $xyseries {
    puts [format "%10.4f %10.4f" $x $y]
}
