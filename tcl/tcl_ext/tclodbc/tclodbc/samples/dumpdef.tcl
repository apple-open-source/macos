####################################################################
#
# dumpdef.tcl
#
# Dump data definitions from every table of the database to a file. 
# 
# Uses utility procedure DumpSchema in file datautil.tcl
#

# Create the db, if not yet created.
set dir [file dirname [info script]]
source $dir/createdb.tcl

####################################################################
#
# Execution starts here
#

tclodbc::DumpSchema db db.def



