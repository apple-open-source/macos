# require tcl 8.0 because of namespaces
if {$tcl_version < "8.0"} {
    return
}

#package provide tclodbc 2.4

namespace eval tclodbc {;
####################################################################
#
# Procedure DumpTable
#
# Dump the contents of a named table to a file.
# The filename may be given explicitly, but if not,
# a default name of the form $tablename.d is used.
#
# The .d file format is the following: 
# 1st line contains the data column names.
# from 2rd line forward, the data in tcl list format.
#
# Parameters:
# db         : database object name
# table      : database table name
# ?filename? : filename, optional
#

namespace export DumpTable
proc DumpTable {db table {filename {}}} {
    # generate filename, if not specified
    if {$filename == {}} {
	set filename $table.d
    }

    # create output file
    set out [open $filename w]

    # statement for table iteration
    $db statement select$table "select * from $table"

    # dump column names
    puts $out [select$table columns name]

    # dump the actual data 
    select$table execute
    while {[set row [select$table fetch]] != {}} {
	puts $out $row
    }  
    select$table drop

    close $out
}
# end proc DumpTable

####################################################################
#
# Procedure LoadTable
#
# Loads the contents of a named table from a file.
# The filename may be given explicitly, but if not,
# a default name of the form $tablename.d is used.
# The file format is assumed to be of the form generated
# by DumpData procedure, described above.
#
# Parameters:
# db         : database object name
# table      : database table name
# ?filename? : filename, optional
#

namespace export LoadTable
proc LoadTable {db table {filename {}}} {
    # generate filename, if not specified
    if {$filename == {}} {
	set filename $table.d
    }

    # open input file
    set in [open $filename]

    # read column names and types
    gets $in colnames

    # generate insert statement
    SqlInsert $db insert$table $table $colnames 

    # read data 
    while {[gets $in row] > -1} {
	insert$table run $row
    }
    insert$table drop

    close $in
}
# end proc LoadTable

proc quote {s {q `}} {
    foreach i $s {
	lappend ret $q$i$q
    }
    return $ret
}

####################################################################
#
# Procedure TableDef
#
# Return the definition of a named database table. 
# The definition consist of a list {tablename coldefs indexdefs}
#
# Parameters:
# db         : database object name
# table      : database table name
#

namespace export TableDef
proc TableDef {db table} {
    # output triplet {tablename coldef indexdef}
    return [list $table [$db columns $table] [$db indexes $table]]
}
# end proc TableDef

####################################################################
#
# Procedure TableDefToSql
#
# Transforms a TableDef as returned by procedure TableDef to a list
# of driver specific sql data definition statements.
#
# Parameters:
# db         : database object name
# tabledef   : database table name
#

namespace export TableDefToSql
proc TableDefToSql {db tabledef} {
    # table name
    set table [lindex $tabledef 0]

    # create column definition sql
    foreach i [lindex $tabledef 1] {
	set colname [lindex $i 3]
	set typeid [lindex $i 4]
	set typeinfo [lindex [$db typeinfo $typeid] 0] 
	if {$typeinfo == {}} {
	    return -code error "invalid type id $typeid, sql conversion failed"
	}

	set coltype [lindex $typeinfo 0]
	set args [split [lindex $typeinfo 5] ,]

	# use simple heuristic based on the count of type arguments
	# this propably does not work in all cases !!
	switch [llength [split [lindex $typeinfo 5] ,]] {
	    0 {# OK, nothing to do}
	    1 {append coltype ([lindex $i 6]) ;# precision}
	    2 {append coltype ([lindex $i 6],[lindex $i 8]) ;# precision, scale}
	    default {return -code error "invalid count of type arguments"}
	}

	lappend coldef "$colname $coltype"
    }
    
    # CREATE TABLE clause
    lappend sql "CREATE TABLE $table ([join $coldef ,]);"

    # indexes
    foreach i [lindex $tabledef 2] {
	set ixname [lindex $i 5]
	
	# Some database (e.g. Access) have a default index without name.
	# Sql cannot be generated for them.
	if {$ixname == {}} continue

	# uniqueness
	if {![lindex $i 3]} {
	    set unique($ixname) "UNIQUE "
	} else {
	    set unique($ixname) {}
	}

	# cumulate column definitions to an array
	set coldef [lindex $i 8]
	if {![string compare [lindex $i 9] D]} {append coldef " DESCENDING"}
	lappend ixcoldef($ixname) $coldef
    }
    
    foreach i [array names ixcoldef] {
	lappend sql "CREATE $unique($i)INDEX $i ON $table\
		([join $ixcoldef($i) ,]);"
    }

    return $sql
}
# end proc TableDefToSql

####################################################################
#
# Procedure LoadSql
#
# Loads sql from a file to a database. 
# The input file should contain one sql statement per line.
#
# Parameters:
# db         : database object name
# filename   : filename
#

namespace export LoadSql
proc LoadSql {db filename} {
    # open input file
    set in [open $filename]

    # execute lines, one by one
    while {[gets $in line] > -1} {
	$db $line
    }

    close $in
}
# end proc LoadSql

####################################################################
#
# Procedure DumpSchema
#
# Dumps whole database schema to a named file.
#
# Parameters:
# db         : database object name
# filename   : filename
#

namespace export DumpSchema
proc DumpSchema {db filename} {
    # create output file 
    set out [open $filename w]

    # dump all tables, excluding system tables
    set tables [$db tables]
    foreach i $tables {
	if {![string compare [lindex $i 3] TABLE]} {
	    set tablename [lindex $i 2]
	    puts $out [TableDef $db $tablename]
	}
    }

    # close file
    close $out
}
# end proc DumpSchema

####################################################################
#
# Procedure LoadSchema
#
# Loads whole database schema from a named file.
#
# Parameters:
# db         : database object name
# filename   : filename
# verbose    : puts lines before executing, useful when debugging sql
#

namespace export LoadSchema
proc LoadSchema {db filename {verbose no}} {
    # open input file
    set in [open $filename]

    # read in and interpret lines
    while {[gets $in line] > -1} {
	foreach i [TableDefToSql $db $line] {
	    if {$verbose} {puts $i}
	    $db $i
	}
    }

    close $in
}
# end proc LoadSchema

}; # end namespace tclodbc
