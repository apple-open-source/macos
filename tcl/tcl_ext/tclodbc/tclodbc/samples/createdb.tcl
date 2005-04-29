####################################################################
#
# createdb.tcl
#
# This procedure creates a new Access database to current directory, 
# sets it as user datasource named testdb, 
# and populates it with some tables.
#

package require tclodbc

####################################################################
#
# set global variables 
#

set driver "Microsoft Access Driver (*.mdb)"
set dbfile testdb.mdb
set dsn TESTDB

####################################################################
#
# subprocedures
#

proc create_datasource {driver dbfile dsn} {
    if {![file exists $dbfile]} {
	database configure config_dsn $driver \
		[list "CREATE_DB=\"$dbfile\" General"]
    }

    database configure add_dsn $driver [list "DSN=$dsn" "DBQ=$dbfile"]
}

proc remove_datasource {driver dsn} {
    database configure remove_dsn $driver "DSN=$dsn"
}

proc create_tables db {
    $db "CREATE TABLE Table1 (
    IntData INTEGER, 
    CharData CHAR (20),
    DateData DATE)"
    $db "CREATE UNIQUE INDEX Table1_Ix1 ON Table1 (IntData)"
    $db "CREATE INDEX Table1_Ix2 ON Table1 (CharData, DateData)"
}

####################################################################
#
# Execution starts here
#
# First we try to open connection to the datasource.
# On error we try to determine what went wrong and act
# accordingly.
#

if {[catch {database db $dsn} err] == 1} {
    if {![string compare [lindex $err 0] IM002]} {
	# datasource not found, create new
	create_datasource $driver $dbfile $dsn
	database db $dsn
	create_tables db
    } elseif {![string compare [lindex $err 0] S1000]} {
	# database file deleted, but datasource not, create new
	remove_datasource $driver $dsn
	create_datasource $driver $dbfile $dsn
	database db $dsn
	create_tables db
    } else {
	# other error  
	error [lindex $err 2]
    }
}

# At this point we should have a datasource named $dsn
# in the system with some test tables created.

# For proper cleanup we should call db disconnect here to
# close connection to the datasource. However, now we leave
# the db connected, because other test programs source
# this file to open the connection, and assume that a database
# object db is available after a succesfull call.

# Commented out on purpose:
# db disconnect
