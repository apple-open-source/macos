# this script runs all tests in the current directory

source defs.tcl

set files [glob *.test]
puts stdout "Processing [llength $files] scripts..."

S

unset F

foreach f [lsort $files] {
  M "  [N $f]"
  if {[catch {source $f} err]} {
    E $err
  }
}

Q
