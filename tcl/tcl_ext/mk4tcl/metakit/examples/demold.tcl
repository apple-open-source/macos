# Simple demo, can be used to verify proper Mk4tcl/TclKit installation
#
# On first run, output should consist of 5 lines:
#   John Lennon 44
#   Flash Gordon 42
#   first Flash last Gordon shoesize 42
#   first John last Lennon shoesize 44
#   first John last Lennon shoesize 44
#
# Every following run will generate 5 more lines of output

package require Mk4tcl

  # create a file called "demo.db"
mk::file open db demo.db
  # define a view in it called "people", containing three fields
set vw [mk::view layout db.people {first last shoesize:I}]

  # let's append two rows to the end of the view
mk::row append $vw first John last Lennon shoesize 44
mk::row append $vw first Flash last Gordon shoesize 42

  # commit the structure and data to file
mk::file commit db

  # a simple loop to print out all rows
mk::loop c $vw {
  puts [mk::get $c first last shoesize]
}

  # another way to loop, in sorted order
foreach r [mk::select $vw -sort last] {
  puts [mk::get $vw!$r]
}

  # this loop iterates over a selection 
foreach r [mk::select $vw first John] {
  puts [mk::get $vw!$r]
}
