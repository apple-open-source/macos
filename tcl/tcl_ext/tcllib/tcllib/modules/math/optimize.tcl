# optimize.tcl --
#    Package to implement optimization of a function or expression
#
# Author: Arjen Markus (arjen.markus@wldelft.nl)
#

# math::optimize --
#    Namespace for the commands
#
namespace eval ::math::optimize {
   namespace export minimum  maximum solveLinearProgram

   # Possible extension: minimumExpr, maximumExpr
}

# minimum --
#    Minimize a given function over a given interval
#
# Arguments:
#    begin       Start of the interval
#    end         End of the interval
#    func        Name of the function to be minimized (takes one
#                argument)
#    maxerr      Maximum relative error (defaults to 1.0e-4)
# Return value:
#    Computed value for which the function is minimal
# Notes:
#    The function needs not to be differentiable, but it is supposed
#    to be continuous. There is no provision for sub-intervals where
#    the function is constant (this might happen when the maximum
#    error is very small, < 1.0e-15)
#
proc ::math::optimize::minimum { begin end func {maxerr 1.0e-4} } {

   set nosteps  [expr {3+int(-log($maxerr)/log(2.0))}]
   set delta    [expr {0.5*($end-$begin)*$maxerr}]

   for { set step 0 } { $step < $nosteps } { incr step } {
      set x1 [expr {($end+$begin)/2.0}]
      set x2 [expr {$x1+$delta}]

      set fx1 [uplevel 1 $func $x1]
      set fx2 [uplevel 1 $func $x2]

      if {$fx1 < $fx2} {
         set end   $x1
      } else {
         set begin $x1
      }
   }
   return $x1
}

# maximum --
#    Maximize a given function over a given interval
#
# Arguments:
#    begin       Start of the interval
#    end         End of the interval
#    func        Name of the function to be maximized (takes one
#                argument)
#    maxerr      Maximum relative error (defaults to 1.0e-4)
# Return value:
#    Computed value for which the function is maximal
# Notes:
#    The function needs not to be differentiable, but it is supposed
#    to be continuous. There is no provision for sub-intervals where
#    the function is constant (this might happen when the maximum
#    error is very small, < 1.0e-15)
#
proc ::math::optimize::maximum { begin end func {maxerr 1.0e-4} } {

   set nosteps  [expr {3+int(-log($maxerr)/log(2.0))}]
   set delta    [expr {0.5*($end-$begin)*$maxerr}]

   for { set step 0 } { $step < $nosteps } { incr step } {
      set x1 [expr {($end+$begin)/2.0}]
      set x2 [expr {$x1+$delta}]

      set fx1 [uplevel 1 $func $x1]
      set fx2 [uplevel 1 $func $x2]

      if {$fx1 > $fx2} {
         set end   $x1
      } else {
         set begin $x1
      }
   }
   return $x1
}

# Now we can announce our presence
package provide math::optimize 0.1

#
# Some simple tests
#
if {[file tail $::argv0] == [info script]} {
   namespace import ::math::optimize::*
   proc f1 { x } { expr {$x*$x} }
   proc f2 { x } { expr {cos($x)} }
   proc f3 { x } { expr {sin($x)} }
   proc f4 { x } { expr {$x*(1.0-$x)} }

   puts "Minimize f(x) = x*x:"
   puts "Between 0 and 1:  [minimum 0.0 1.0  f1] (expected: 0)"
   puts "Between -1 and 3: [minimum -1.0 3.0 f1] (expected: 0)"
   puts "Between  1 and 3: [minimum 1.0 3.0  f1] (expected: 1)"

   puts "Minimize f(x) = cos(x):"
   puts "Between 0 and 1:  [minimum 0.0 1.0  f2] (expected: 1)"
   puts "Between -1 and 3: [minimum -1.0 3.0 f2] (expected: 3)"
   puts "Between  1 and 6: [minimum 1.0 6.0  f2] (expected: pi)"

   puts "Minimize f(x) = sin(x):"
   puts "Between 0 and 1:   [minimum  0.0 1.0  f3 ] (expected: 0)"
   puts "Between -1 and 3:  [minimum -1.0 3.0  f3 ] (expected: -1)"
   puts "Between  1 and 6:  [minimum  1.0 6.0  f3 ] (expected: 1.5pi)"
   puts "Between  0 and 60: [minimum  0.0 60.0 f3 ] (expected: ???)"
   puts "Between  0 and 6:  [minimum  0.0 6.0  f3 1.0e-7] (expected: 1.5pi)"

   puts "Maximize f(x) = x*(1-x):"
   puts "Between 0 and 1:  [maximum 0.0 1.0  f4 ] (expected: 0.5)"
   puts "Between -1 and 3: [maximum -1.0 3.0 f4 ] (expected: 0.5)"
   puts "Between  1 and 3: [maximum 1.0 3.0  f4 ] (expected: 1)"

}
