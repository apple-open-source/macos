#
# rosenbrock.demo3.test.tcl --
#   Plot the contours of the Rosenbrock function with
#   Tklib/Plotchart.
#   Test with a small screen.
#   Based on a modified Plotchart version, implementing
#   contourlinesvalues with a helper function for
#   the axis values.
#
# Copyright 2008-2009 Michael Baudin
#

package require Plotchart 1.8
package require cmdline

set width 500
set height 500

proc rosenbrock {x} {
  set x1 [lindex $x 0]
  set x2 [lindex $x 1]
  set y [expr {100.0 * pow($x2 - $x1*$x1,2) + pow(1.0 - $x1,2)}]
  return $y
}

#
# linspace --
#   Generate a list of values in the given range.
# Arguments
#   -min min : the minimum value in the range (default 0.0)
#   -max max : the maximum value in the range (default (1.0)
#   -step step : the step to use (default 0.1)
#   -nb nb : the number of points in the range (default 10)
#   -method method : the method to use to generate the points in the range (default "step")
#     * if method is "step", the step is used to generate
#       the list of values
#     * if method is "nb", the nb parameter is used to compute a step
#
proc linspace {args} {
    set opts {
      {min.arg 0.0 "the minimum value in the range"}
      {max.arg 1.0 "the maximum value in the range"}
      {step.arg 0.1 "the step to use"}
      {nb.arg 10 "the number of points in the range"}
      {method.arg "step" "the method to compute the values"}
    }
    set usage ": plotcontours \[options] ...\noptions:"
    array set params [::cmdline::getoptions args $opts $usage]

    set min $params(min)
    set max $params(max)
    if {$min > $max} then {
      error "The minimum value is $min but the maximum value is $max"
    }
    switch -- $params(method) {
      "nb" {
        set nb $params(nb)
        set step [expr {($max-$min)/($nb-1)}]
      }
      "step" {
        set step $params(step)
      }
      default {
        error "Unknown method $params(method)"
      }
    }
    set result {}
    set current $min
    while {1} {
      lappend result $current
      set current [expr {$current + $step}]
      if {$current > $max} then {
        break
      }
    }
    return $result
}

#
# Direct use of the API
#
#set xvec {-2.0 -1.75 -1.5 -1.25 -1.0 -0.25 -0.5 -0.25 0.0 0.25 0.5 0.75 1.0 1.25 1.5 1.75 2.0}
#set yvec {-1.0 -0.25 -0.5 -0.25 0.0 0.25 0.5 0.75 1.0 1.25 1.5 1.75 2.0}
set xvec [linspace -min -2.0 -max 2.0 -step 0.05]
set yvec [linspace -min -1.0 -max 2.0 -step 0.05]

set fmat {}
foreach y $yvec {
  set frow {}
  foreach x $xvec {
    lappend frow [rosenbrock [list $x $y]]
  }
  lappend fmat $frow
}

set contours { \
  0.0  1.0  3.0  5.0 10  30  \
  50  100  200  500  1000  2000  3000   \
  }

set xlimits {-2.0 2.0 2.0}
set ylimits {-1.0 2.0 1.0}
wm title . "Rosenbrock contours"
::Plotchart::colorMap jet
set c [canvas .c  -background white -width $width -height $height]
pack   $c  -fill both -side top
set chart1 [::Plotchart::createXYPlot $c $xlimits $ylimits]
$chart1 contourlinesfunctionvalues $xvec $yvec $fmat $contours
$chart1 saveplot "rosenbrock.demo3.png" -format png

