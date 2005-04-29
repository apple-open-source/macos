# scaling.tcl --
#    Make a nice scale for the axes in the Plotchart package
#

namespace eval ::Plotchart {
   namespace export determineScale

   #
   # Try and load the math::fuzzy package for better
   # comparisons
   #
   if { [catch {
            package require math::fuzzy
            namespace import ::math::fuzzy::tlt
            namespace import ::math::fuzzy::tgt
         }] } {
      proc tlt {a b} {
         expr {$a < $b }
      }
      proc tgt {a b} {
         expr {$a > $b }
      }
   }
}

# determineScale --
#    Determine nice values for an axis from the given extremes
#
# Arguments:
#    xmin      Minimum value
#    xmax      Maximum value
# Result:
#    A list of three values, a nice minimum and maximum
#    and stepsize
# Note:
#    xmin is assumed to be smaller or equal xmax
#
proc ::Plotchart::determineScale { xmin xmax } {
   set dx [expr {abs($xmax-$xmin)}]

   if { $dx == 0.0 } {
      if { $xmin == 0.0 } {
         return [list -0.1 0.1 0.1]
      } else {
         set dx [expr {0.2*abs($xmax)}]
         set xmin [expr {$xmin-0.5*$dx}]
         set xmax [expr {$xmin+0.5*$dx}]
      }
   }

   #
   # Determine the factor of 10 so that dx falls within the range 1-10
   #
   set expon  [expr {int(log10($dx))}]
   set factor [expr {pow(10.0,$expon)}]

   set dx     [expr {$dx/$factor}]

   foreach {limit step} {1.4 0.2 2.0 0.5 5.0 1.0 10.0 2.0} {
      if { $dx < $limit } {
         break
      }
   }

   set nicemin [expr {$step*$factor*int($xmin/$factor/$step)}]
   set nicemax [expr {$step*$factor*int($xmax/$factor/$step)}]
   if { [tlt $nicemax $xmax] } {
      set nicemax [expr {$nicemax+$step}]
   }
   if { [tgt $nicemin $xmin] } {
      set nicemin [expr {$nicemin-$step}]
   }

   return [list $nicemin $nicemax [expr {$step*$factor}]]
}

if 0 {
    #
    # Some simple test cases
    #
    namespace import ::Plotchart::determineScale
    puts [determineScale 0.1 1.0]
    puts [determineScale 0.001 0.01]
    puts [determineScale -0.2 0.9]
    puts [determineScale -0.25 0.85]
    puts [determineScale -0.25 0.7999]
    puts [determineScale 10001 10010]
    puts [determineScale 10001 10015]
}
