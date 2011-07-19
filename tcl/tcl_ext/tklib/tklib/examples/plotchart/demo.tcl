#  demo.tcl --
#      Demo program for Plotchart
#
source d:/tcl-programs/plotchart/cvs-dir/plotchart.tcl
package require Plotchart

proc demo {code} {
    .cnv delete all

    .code delete 1.0 end
    .code insert end $code

    eval $code

    vwait next
}

proc nextDemo {} {
    set ::next 1
}

button .next -text "   >   " -command nextDemo
text   .code -width 70 -height 30
canvas .cnv  -width 600 -height 600
grid .code .cnv -sticky news
grid .next -

# DEMO 1

demo {
set s [::Plotchart::createXYPlot .cnv {0.0 100.0 10.0} {0.0 100.0 20.0}]
set r [::Plotchart::createRightAxis .cnv {0.0 0.1 0.01}]

set xd    5.0
set yd   20.0
set xold  0.0
set yold 50.0

$s dataconfig series1 -colour "red"
$s dataconfig series2 -colour "blue"
$s dataconfig series3 -colour "magenta"

for { set i 0 } { $i < 20 } { incr i } {
   set xnew [expr {$xold+$xd}]
   set ynew [expr {$yold+(rand()-0.5)*$yd}]
   set ynew2 [expr {$yold+(rand()-0.5)*2.0*$yd}]
   $s plot series1 $xnew $ynew
   $s plot series2 $xnew $ynew2
   $s trend series3 $xnew $ynew2
   set xold $xnew
   set yold $ynew
}

$s interval series2 50.0 40.0 60.0 52.0
$s interval series2 60.0 40.0 60.0

$s xtext "X-coordinate"
$s ytext "Y-data"
$r ytext "Right axis"
$s title "Aha!"

#
# Some data for the right axis
#
$r dataconfig right -type both -symbol circle -colour green
$r plot right 10.0 0.01
$r plot right 30.0 0.03
$r plot right 40.0 0.02
}

demo {
set p [::Plotchart::createBoxplot .cnv {0 40 5} {A B C D E F}]

$p plot A {0 1 2 5 7 1 4 5 0.6 5 5.5}
$p plot C {2 2 3 6 1.5 3}

$p plot E {2 3 3 4 7 8 9 9 10 10 11 11 11 14 15 17 17 20 24 29}
}

# Histograms and the like
demo {
set s [::Plotchart::createBarchart .cnv {A B C D E} {0.0 10.0 2.0} 2.5]

$s plot series1 {1.0 4.0 6.0 1.0 7.0} red
$s plot series2 {0.0 3.0 7.0 9.3 2.0} green

$s legend series1 "Series 1"
$s legend series2 "Series 2"

$s title "Arbitrary data"
}

demo {
set s [::Plotchart::createPiechart .cnv]

$s plot {"Long names" 10 "Short names" 30 "Average" 40
         "Ultra-short names" 5}

$s explode auto
$s explode 1
$s title "Names - click on a segment!"
}

demo {
set s [::Plotchart::createGanttchart .cnv "1 january 2004" \
        "31 december 2004" 14]

set from [$s task "Spring" "1 march 2004" "1 june 2004" 30]
set to   [$s task "Summer" "1 june 2004" "1 september 2004" 10]
$s summary "First half" $from $to
$s connect $from $to
$s vertline "1 jan" "1 january 2004"
$s vertline "1 apr" "1 april 2004"
$s vertline "1 jul" "1 july 2004"
$s vertline "1 oct" "1 october 2004"
$s milestone "Longest day" "21 july 2004"
$s title "Seasons (northern hemisphere)"
}

demo {
::Plotchart::plotconfig horizbars leftaxis font "Helvetica 10 italic"
::Plotchart::plotconfig horizbars background outercolor steelblue3
::Plotchart::plotconfig horizbars bottomaxis ticklength -5

set s [::Plotchart::createHorizontalBarchart .cnv {0.0 10.0 2.0} \
         {Antarctica Eurasia "The Americas" "Australia and Oceania" Ocean} 2]

$s plot series1 {1.0 4.0 6.0 1.0 7.0} red left-right
$s plot series2 {0.0 3.0 7.0 9.3 2.0} green right-left dark
$s title "Arbitrary data"
}

# DEMO 5
demo {
proc cowboyhat {x y} {
   set x1 [expr {$x/9.0}]
   set y1 [expr {$y/9.0}]

   expr { 3.0 * (1.0-($x1*$x1+$y1*$y1))*(1.0-($x1*$x1+$y1*$y1)) }
}

set xlimits {-10. 10.  10.  }
set ylimits {-10. 10.  10.  }
set zlimits { -5. 10.   5.  }

set zmin   0.0
set zmax   3.0

set nc    51
set dz    [expr {($zmax - $zmin) / ($nc - 1)}]

set contours {}
for {set cnt 1} {$cnt < $nc} {incr cnt} {
    set zval [expr {$zmin + ($dz * ($cnt - 1))}]
    lappend contours $zval
}

set chart6 [::Plotchart::create3DPlot .cnv $xlimits $ylimits $zlimits]
::Plotchart::colorMap jet
$chart6 title "3D Plot"
$chart6 plotfuncont cowboyhat $contours
}

# DEMO 9
demo {
#
# Wind rose diagram
#
set p [::Plotchart::createWindRose .cnv {30 6} 4]

$p plot {5 10 0 3} red
$p plot {10 10 10 3} blue

$p title "Simple wind rose - margins need to be corrected ..."
}

demo {
#
# Bands in two directions
#
set p [::Plotchart::createXYPlot .cnv {0 10 2} {0 40 10}]

$p plot data 1 10
$p plot data 6 20
$p plot data 9 10

$p xband 15 25
$p yband 3 5
}

demo {
#
# Label-dots and vertical text
#

set p [::Plotchart::createXYPlot .cnv {0 10 2} {0 40 10}]

$p labeldot 3 10 "Point 1" w
$p labeldot 6 20 "Point 2" e
$p labeldot 9 10 "Point 3" n
$p labeldot 9 30 "Point 4" s
}

demo {
#
# 3D ribbon chart
#

set p [::Plotchart::create3DRibbonChart .cnv {A B C D} {0.0 1.0 0.25} {0.0 50.0 10.0}]

$p area {0.1 10.0 0.2 10.0 0.5 45.0 0.9 20.0 1.0 30.0} green
$p line {0.2 8.0 0.5 25.0 0.7 10.0 0.9 10.0 1.0 50.0} blue
}

demo {
    set s [::Plotchart::createHistogram .cnv {0.0 100.0 10.0} {0.0 100.0 20.0}]

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
}

exit
