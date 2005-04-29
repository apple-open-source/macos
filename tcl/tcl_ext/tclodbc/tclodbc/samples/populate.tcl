####################################################################
#
# populate.tcl
#
# This procedure populates some tables in current db with arbitrary
# values. 
#

# Create the db, if not yet created, and open connection.
set dir [file dirname [info script]]
source $dir/createdb.tcl

####################################################################
#
# subprocedures
#

# generate pseudorandom data for table1, unique index given as parameter

proc Table1Row IntData {
    # create some hexadecimal string to use as string data
    set CharData [format %X [expr {$IntData * 123456}]]

    # some datedata, using utility procedure TclTimeToSqlTime in utilproc.tcl
    set DateData [lindex [tclodbc::TclTimeToSqlTime \
	    [expr {[clock seconds] + ($IntData * 86400)}]] 0]

    return [list $IntData $CharData $DateData]
}

####################################################################
#
# Delete data from all tables to avoid duplicates in indices
#

db "DELETE FROM Table1"

####################################################################
#
# Table1 using plain sql
#

puts "Generating data to tables..."

set starttime [clock clicks]
set rowcount 0

# insert 500 rows
for {set i 1} {$i <= 500} {incr i} {
    # generate some data
    set row [Table1Row $i]
    set CharData [lindex $row 1]
    set DateData [lindex $row 2]

    # insert data, note odbc extended escape sequence for date value
    db "INSERT INTO TABLE1 (IntData, CharData, DateData) VALUES ($i, '$CharData', \{d '$DateData'\})" 

    incr rowcount
}

# output statistics
set usedtime [expr {[clock clicks] - $starttime}]
puts "Table1: $rowcount rows in $usedtime clicks, [format %.2f [expr $usedtime.0 / $rowcount]] clicks per row using plain sql"

####################################################################
#
# Table1 using statement object
#

# Create a statement object. 
# Note the argument type definitions after the sql clause.
db statement s1 "INSERT INTO TABLE1 (IntData, CharData, DateData) VALUES (?,?,?)" [list {INTEGER 0 10} {CHAR 20}]

set starttime [clock clicks]
set rowcount 0

# insert 500 rows more
for {} {$i <= 1000} {incr i} {
    # generate some data
    set row [Table1Row $i]

    # execute insert statement 
    s1 $row

    incr rowcount
}

# output statistics
set usedtime [expr {[clock clicks] - $starttime}]
puts "Table1: $rowcount rows in $usedtime clicks, [format %.2f [expr $usedtime.0 / $rowcount]] clicks per row using statement object"

# drop statement when no longer used
s1 drop
