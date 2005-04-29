####################################################################
#
# dumpdata.tcl
#
# Dump data from every table of the database to a file. 
# 
# Uses utility procedure DumpTable in file datautil.tcl
#

# Create the db, if not yet created.
set dir [file dirname [info script]]
source $dir/createdb.tcl

# Populate some data to tables
if {[db "select count(*) from table1"] == 0} {source $dir/populate.tcl}

####################################################################
#
# Execution starts here
#

set tables [db tables]

foreach i $tables {
    # exclude system tables
    if {![string compare [lindex $i 3] TABLE]} {
	tclodbc::DumpTable db [lindex $i 2]
    }
}

