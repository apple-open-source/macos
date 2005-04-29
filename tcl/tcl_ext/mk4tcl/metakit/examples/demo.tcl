# Simple demo, can be used to verify proper Mk4tcl installation

load ../builds/Mk4tcl[info sharedlibextension] Mk4tcl
puts "[info script] - Mk4tcl [package require Mk4tcl] - $tcl_platform(os)"

# On first run, output should consist of 5 lines:
#   John Lennon 44
#   Flash Gordon 42
#   Flash Gordon 42
#   John Lennon 44
#   John Lennon 44
# Each following run will generate 5 more lines of output

  # create a file called "demo.db"
mk::file open db demo.db -nocommit
  # define a view in it called "people", containing three fields
mk::view layout db.people {first last shoesize:I}
set vw [mk::view open db.people]

  # let's append two rows to the end of the view
$vw insert end first John last Lennon shoesize 44
$vw insert end first Flash last Gordon shoesize 42

  # commit the structure and data to file
mk::file commit db

  # a simple loop to print out all rows
for {set r 0} {$r < [$vw size]} {incr r} {
  puts [$vw get $r first last shoesize]
}

  # another way to loop, in sorted order
foreach r [$vw select -sort last] {
  puts [$vw get $r first last shoesize]
}

  # this loop iterates over a selection 
foreach r [$vw select first John] {
  puts [$vw get $r first last shoesize]
}
