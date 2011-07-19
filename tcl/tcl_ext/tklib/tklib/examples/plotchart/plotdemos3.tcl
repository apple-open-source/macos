#! /bin/sh
# -*- tcl -*- \
exec tclsh "$0" ${1+"$@"}

package require Tcl 8.4
package require Tk

package require Plotchart

# plotdemos3.tcl --
#     Show a Gantt chart
#

canvas .c -width 500 -height 200 -bg white
pack   .c -fill both
.c delete all

set s [::Plotchart::createGanttchart .c "1 january 2004" \
        "31 december 2004" 4]

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

#
# Copy the thing:
# Should result in this configuration:
#  = =
#  =
toplevel .t
canvas   .t.c -width 700 -height 500
pack .t.c
::Plotchart::plotpack .t.c top $s $s
::Plotchart::plotpack .t.c left $s
