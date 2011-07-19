# plotdemos16.tcl --
#     Test performance profiles
#
#     This type of diagrams is used when comparing a set of numerical methods
#     for their performance on a set of solved problems. For the performance
#     measure: the lower the value the better.
#
#     Reference:
#     Desmond Higham and Nicholas Higham
#         Matlab Guide
#         SIAM, 2005, Philadephia
#
#
source ../../plotchart.tcl
package require Plotchart


#
# Performance profile
#
pack [canvas .c1 -bg white]

set p [::Plotchart::createPerformanceProfile .c1 5.0]

#
# Data copied from Higham and Higham
#
$p dataconfig ode23  -symbol circle -colour red   -type both
$p dataconfig ode45  -symbol plus   -colour blue  -type both
$p dataconfig ode113 -symbol cross  -colour green -type both

#       Model results             Measurements to compare them with
$p plot {ode23  {1.26e-2 2.41e-1 3.74e-2 3.37e0 1.44e-1 5.06e-1}
         ode45  {6.20e-3 1.53e-1 5.00e-2 6.45e0 1.56e-1 1.07e0}
         ode113 {1.56e-2 1.97e-1 6.68e-2 7.86e0 3.76e-2 1.50e0}}

$p legend ode23  ode23
$p legend ode45  ode45
$p legend ode113 ode113
