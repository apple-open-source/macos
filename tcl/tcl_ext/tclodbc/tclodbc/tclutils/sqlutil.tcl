# require tcl 8.0 because of namespaces
if {$tcl_version < "8.0"} {
    return
}

#package provide tclodbc 2.4

namespace eval tclodbc {;
####################################################################
#
# Procedure SqlInsert
# 
# Return command for creating a insert statement for given table
# and columns. Mode determines the action for the command. Useful
# modes are e.g. eval, return and puts.
# 
# Parameters:
# db         : database object
# stmt       : statement name
# table      : table name
# columns    : column names
# ?mode?     : mode, default is eval       

namespace export SqlInsert
proc SqlInsert {db stmt table columns {mode eval}} {
    set sql "INSERT INTO $table ([join $columns ,]) VALUES ([SqlParams $columns , no])"
    set coltypes [ColTypes $db $table $columns]
    $mode "$db statement $stmt \{$sql\} \{$coltypes\}"
}
# end proc SqlInsert

####################################################################
#
# Procedure SqlUpdate
# 
# Return command for creating a update statement for given table
# and columns. Mode determines the action for the command. Useful
# modes are e.g. eval, return and puts. 
# 
# Parameters:
# db         : database object
# stmt       : statement name
# table      : table name
# datacols   : data column names
# keycols    : key column names
# ?mode?     : mode, default is eval       

namespace export SqlUpdate
proc SqlUpdate {db stmt table datacols keycols {mode eval}} {
    set sql "UPDATE $table SET [SqlParams $datacols ,] WHERE [SqlParams $keycols]" 
    set coltypes [ColTypes $db $table [concat $datacols $keycols]]
    $mode "$db statement $stmt \{$sql\} \{$coltypes\}"
}
# end proc SqlUpdate

####################################################################
#
# Procedure SqlSelect
# 
# Return command for creating a select statement for given table
# and columns. Mode determines the action for the command. Useful
# modes are e.g. eval, return and puts.
# 
# Parameters:
# db         : database object
# stmt       : statement name
# table      : table name
# datacols   : data column names
# keycols    : key column names
# ?mode?     : mode, default is eval       

namespace export SqlSelect
proc SqlSelect {db stmt table datacols keycols {mode eval}} {
    set sql "SELECT [join $datacols ,] FROM $table WHERE [SqlParams $keycols]" 
    set coltypes [ColTypes $db $table $keycols]
    $mode "$db statement $stmt \{$sql\} \{$coltypes\}"
}
# end proc SqlSelect

####################################################################
#
# Procedure SqlDelete
# 
# Create command for creating a delete statement for given table
# and columns. 
# 
# Parameters:
# db         : database object
# stmt       : statement name
# table      : table name
# keycols    : key column names
# ?mode?     : mode, default is eval       

namespace export SqlDelete
proc SqlDelete {db stmt table keycols {mode eval}} {
    set sql "DELETE FROM $table WHERE [SqlParams $keycols]" 
    set coltypes [ColTypes $db $table $keycols]
    $mode "$db statement $stmt \{$sql\} \{$coltypes\}"
}
# end proc SqlDelete

####################################################################
#
# Procedure ColTypes
# 
# Return list of column types for given columns. This list can
# be used as statement argument specification.
# 
# Parameters:
# db         : database object
# table      : table name
# columns    : column names

namespace export ColTypes
proc ColTypes {db table columns} {
    set coldefs [$db columns $table]
    foreach i $coldefs {
	set colname [string tolower [lindex $i 3]]
	set coltype($colname) [list [lindex $i 4] [lindex $i 6]]

	# precision may be empty for certain datatypes
	if {[lindex $i 8] != {}} {
	    lappend coltype($colname) [lindex $i 8]
	}
    }

    foreach i $columns {
	lappend coltypes $coltype([string tolower $i])
    }

    return $coltypes
}
# end proc ColTypes

####################################################################
#
# Private utility procedures

proc SqlParams {columns {separator { AND }} {assign yes}} {
    foreach i $columns {
	if {$assign} {
	    lappend marks ${i}=?
	} else {
	    lappend marks ?
	}
    }
    return [join $marks $separator]
}

}; # end namespace tclodbc
