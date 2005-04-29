set __dir__ $dir 
foreach index [glob -nocomplain [file join $dir * pkgIndex.tcl]] {
  set dir [file dirname $index]
  source $index
} 
set dir $__dir__ 
unset __dir__ 

