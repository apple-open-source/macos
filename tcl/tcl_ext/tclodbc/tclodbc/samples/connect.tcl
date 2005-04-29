####################################################################
#
# connect.tcl
#
# Demonstrate connecting to an existing database using both
# dsn and connection string
#

# Create the db, if not yet created, close connection immediately.
set dir [file dirname [info script]]
source $dir/createdb.tcl
db disconnect

####################################################################
#
# subprocedures
#

proc output_db_data db {
  set tables [$db tables]
  puts "db:\t[lindex [lindex $tables 0] 0]"
  puts -nonewline "tables:"
  foreach i $tables {
    puts \t[lindex $i 2]
  }
}

####################################################################
#
# Connect to an Access database using a connection string.
# Uses global variables $driver and $dbfile set in createdb.tcl.
#
# Note that connection string syntax is odbc driver dependent, and
# usually documented in driver's help file or other documentation.
# However, this is the only way to connect to a local database
# using its real file name instead of datasource name.
#

database db "DRIVER=$driver;DBQ=$dbfile" 

output_db_data db

db disconnect

####################################################################
#
# Connect to a database using datasource name.
# Uses global variable $dsn set in createdb.tcl.
#
# This is the preferred method of connecting to a database,
# but requires datasource name to be set.
#

database db $dsn 

output_db_data db

db disconnect
