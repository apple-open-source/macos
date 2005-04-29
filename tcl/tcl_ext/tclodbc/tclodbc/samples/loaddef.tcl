####################################################################
#
# loaddef.tcl
#
# Loads data definitions from every table of the database from a file
# named db.def. The sql statements are stored to file named db.sql. 
# 
# Uses utility procedure TableDefToSql in file datautil.tcl
#

# Create the db, if not yet created.
set dir [file dirname [info script]]
source $dir/createdb.tcl

####################################################################
#
# Execution starts here
#

# delete tables, if exists
puts "dropping all tables..."
foreach i [db tables] {
    # exclude system tables
    if {![string compare [lindex $i 3] TABLE]} {
	db "drop table [lindex $i 2]"
    }
}
puts "drop completed"

puts "loading..."
tclodbc::LoadSchema db db.def yes
puts "load complete"

