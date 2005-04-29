####################################################################
#
# loaddata.tcl
#
# Loads data from every table of the database from a file. 
# 
# Uses utility procedure LoadTable in file datautil.tcl
#

# Create the db, if not yet created.
set dir [file dirname [info script]]
source $dir/createdb.tcl

####################################################################
#
# Execution starts here
#

set tables [db tables]

foreach i $tables {
    # exclude system tables
    if {![string compare [lindex $i 3] TABLE]} {
	# delete old data
	db "DELETE FROM Table1"

	# load data from file
	tclodbc::LoadTable db [lindex $i 2]
    }
}

