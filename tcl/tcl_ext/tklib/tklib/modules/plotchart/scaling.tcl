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
#    inverted  Whether to return values for an inverted axis (1) or not (0)
#              Defaults to 0.
# Result:
#    A list of three values, a nice minimum and maximum
#    and stepsize
# Note:
#    xmin is assumed to be smaller or equal xmax
#
proc ::Plotchart::determineScale { xmin xmax {inverted 0} } {
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
      set nicemax [expr {$nicemax+$step*$factor}]
   }
   if { [tgt $nicemin $xmin] } {
      set nicemin [expr {$nicemin-$step*$factor}]
   }

   if { !$inverted } {
       return [list $nicemin $nicemax [expr {$step*$factor}]]
   } else {
       return [list $nicemax $nicemin [expr {-$step*$factor}]]
   }
}

# determineTimeScale --
#    Determine nice date/time values for an axis from the given extremes
#
# Arguments:
#    tmin      Minimum date/time
#    tmax      Maximum date/time
# Result:
#    A list of three values, a nice minimum and maximum
#    and stepsize
# Note:
#    tmin is assumed to be smaller or equal tmax
#
proc ::Plotchart::determineTimeScale { tmin tmax } {
    set ttmin [clock scan $tmin]
    set ttmax [clock scan $tmax]

    set dt [expr {abs($ttmax-$ttmin)}]

    if { $dt == 0.0 } {
        set dt 86400.0
        set ttmin [expr {$ttmin-$dt}]
        set ttmax [expr {$ttmin+$dt}]
    }

    foreach {limit step} {2.0 0.5 5.0 1.0 10.0 2.0 50.0 7.0 300.0 30.0 1.0e10 365.0} {
        if { $dt/86400.0 < $limit } {
            break
        }
    }

    set nicemin [expr {$step*floor($ttmin/$step)}]
    set nicemax [expr {$step*floor($ttmax/$step)}]

    if { $nicemax < $ttmax } {
        set nicemax [expr {$nicemax+$step}]
    }
    if { $nicemin > $ttmin } {
        set nicemin [expr {$nicemin-$step}]
    }

    set nicemin [expr {int($nicemin)}]
    set nicemax [expr {int($nicemax)}]

    return [list [clock format $nicemin -format "%Y-%m-%d %H:%M:%S"] \
                 [clock format $nicemax -format "%Y-%m-%d %H:%M:%S"] \
                 $step]
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
if 0 {
    puts [::Plotchart::determineTimeScale "2007-01-15" "2007-01-16"]
    puts [::Plotchart::determineTimeScale "2007-03-15" "2007-06-16"]
}
