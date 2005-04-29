# require tcl 8.0 because of namespaces
if {$tcl_version < "8.0"} {
    return
}

#package provide tclodbc 2.4

namespace eval tclodbc {;
####################################################################
#
# Procedure Justification
#
# Returns default justification of a column, right, left or center
#
# Parameters:
# coltype    : column type description, as returned by
#		"stmt columns type precision scale displaysize"
#

namespace export Justification
proc Justification {coltype} {
    switch [lindex $coltype 0] {
	2 - 3 - 4 - 5 - 6 - 7 - 8 - {-5} - {-6} {
	    # numerical datatypes
	    return right
	}
	1 - 12 - {-1} {
	    # string types
	    return left
	}
	default {
	    # logical, date and time types
	    return center
	}
    }
}
# end proc Justification

####################################################################
#
# Procedure SqlShell
#
# Very simple sql shell. Execute given statements and return data
# in columnar format. 
#
# Parameters:
# db          : db connection object
#

namespace export SqlShell
proc SqlShell {db} {
    set pagesize 25
    
    while {[puts -nonewline stdout "> "; flush stdout; gets stdin line] > -1} {
	if {$line == {}} continue
	if {$line == {exit}} break
	if {[catch {
	    # accept get, set and transaction handling commands
	    if {
		[string match {[sg]et*} $line]
		|| [string match {commit} $line]
		|| [string match {rollback} $line]
	    } {
		puts [eval $db $line]
		continue
	    }

	    $db statement stmt $line

	    stmt execute
	    set fmt [stmt columns type displaysize]

	    if {[llength $fmt] == 0} {
		set rowcount [stmt rowcount]
		puts stdout "$rowcount rows OK"
		continue
	    }

	    set row 0
	    while {[set data [stmt fetch]] != {}} {
		if {![expr $row % $pagesize]} {
		    puts [tclodbc::FieldFormat $fmt [stmt columns label]]
		    puts [tclodbc::FieldFormat $fmt]
		    incr row 2
		}
		puts [tclodbc::FieldFormat $fmt $data]
		incr row

		if {![expr ($row + 1) % $pagesize]} {
		    puts -nonewline stdout "Continue? (press A to abort) "
		    flush stdout
		    gets stdin line
		    if {[string toupper $line] == {A}} break
		    puts stdout ""
		}
	    }
	} err]} {
	    puts stderr $err
	}
    }
}
# end proc Justification

####################################################################
#
# Procedure DefaultFormat
#
# Returs default string format for a given column type
#
# Parameters:
# fielddef    : field defininition, {coltype ?fieldlenght?}
#
namespace export DefaultFormat
proc DefaultFormat {fieldtype} {
    set coltype [lindex $fieldtype 0]
    set flen [lindex $fieldtype 1]
    set fjust {}
    switch $coltype {
	1 - CHAR - 12 - VARCHAR {
	    set ftype s
	    set fjust -
	    if {$flen == {}} {set flen 15}
	    set fprec $flen
	}
	2 - NUMERIC - 3 - DECIMAL - 6 - FLOAT - 7 - REAL - 8 - DOUBLE {
	    set ftype f
	    if {$flen == {}} {set flen 14}
	    # 2 decimals by default
	    set fprec 2
	}
	4 - INTEGER {
	    set ftype d
	    if {$flen == {}} {set flen 10}
	    set fprec 1
	}
	5 - SMALLINT {
	    set ftype d
	    if {$flen == {}} {set flen 5}
	    set fprec 1
	}
	9 - DATE {
	    set ftype s
	    set fjust -
	    if {$flen == {}} {set flen 10}
	    set fprec $flen
	}
	10 - TIME {
	    set ftype s
	    set fjust -
	    if {$flen == {}} {set flen 8}
	    set fprec $flen
	}
	11 - DATETIME {
	    set ftype s
	    set fjust -
	    if {$flen == {}} {set flen 19}
	    set fprec $flen
	}
	-7 - BIT {
	    set ftype d
	    if {$flen == {}} {set flen 1}
	    set fprec 1
	}
	default {error "Invalid column type: $coltype"}
    }
    
    return %${fjust}${flen}.${fprec}${ftype}
}

####################################################################
#
# Procedure FieldFormat
#
# Formats given list of fields for output
#
# Parameters:
# fielddef    : field defininition list, {coltype ?fieldlenght?}
# ?data?      : field data, if empty, return a delimiter row
#

namespace export FieldFormat
proc FieldFormat {fielddef {data {}}} {
    set col 0
    foreach i $fielddef {
	set coltype [lindex $i 0]
	set flen [lindex $i 1]
	set fjust {}
	switch $coltype {
	    1 - CHAR - 12 - VARCHAR {
		set ftype s
		set fjust -
		if {$flen == {}} {set flen 15}
		set fprec $flen
	    }
	    2 - NUMERIC - 3 - DECIMAL - 6 - FLOAT - 7 - REAL - 8 - DOUBLE {
		set ftype f
		if {$flen == {}} {set flen 14}
		# 2 decimals by default
		set fprec 2
	    }
	    4 - INTEGER {
		set ftype d
		if {$flen == {}} {set flen 10}
		set fprec 1
	    }
	    5 - SMALLINT {
		set ftype d
		if {$flen == {}} {set flen 5}
		set fprec 1
	    }
	    9 - DATE {
		set ftype s
		set fjust -
		if {$flen == {}} {set flen 10}
		set fprec $flen
	    }
	    10 - TIME {
		set ftype s
		set fjust -
		if {$flen == {}} {set flen 8}
		set fprec $flen
	    }
	    11 - DATETIME {
		set ftype s
		set fjust -
		if {$flen == {}} {set flen 19}
		set fprec $flen
	    }
	    -7 - BIT {
		set ftype d
		if {$flen == {}} {set flen 1}
		set fprec 1
	    }
	    default {error "Invalid column type: $coltype"}
	}
	if {$data == {}} {
	    # empty data, just output delimiter row
	    for {set j 0} {$j < $flen} {incr j} {append output =}
	    append output { }
	    continue
	} else {
	    #puts "%${fjust}${flen}.${fprec}${ftype} [lindex $data $col]"
	    if {[catch {append output \
		    [format "%${fjust}${flen}.${fprec}${ftype} " \
		    [lindex $data $col]]}]} {
		# if the format returned error, the data does not match the
		# given type, or it is empty just append the data in string
		# form to fill up the field
		append output [format "%${fjust}${flen}.${flen}s " \
			[lindex $data $col]]
	    }
	}

	incr col
    }

    return $output
}
# end proc FieldFormat

####################################################################
#
# Procedure TclTimeToSqlTime
#
# Returns sql timestamp of the form YYYY-MM-DD HH:MM:SS
#
# Parameters:
# tcltime     : tcl time as returned by [clock seconds] 
#

namespace export TclTimeToSqlTime
proc TclTimeToSqlTime {tcltime} {
    return [clock format $tcltime -format "%Y-%m-%d %H:%M:%S"]
}
# end proc TclTimeToSqlTime

####################################################################
#
# Procedure SqlTimeToTclTime
#
# Returns tcl time in seconds
#
# Parameters:
# sqltime     : sql timestamp of the form YYYY-MM-DD ?HH:MM:SS?
#

namespace export SqlTimeToTclTime
proc SqlTimeToTclTime {sqltime} {
    set date [lindex $sqltime 0]
    set time [lindex $sqltime 1]

    # simple conversion from YYYY-MM-DD to MM/DD/YYYY
    set date [split $date -]
    set date [join [list [lindex $date 1] [lindex $date 2] [lindex $date 0]] /]

    return [clock scan [concat $date $time]]
}
# end proc SqlTimeToTclTime

}; # end namespace tclodbc
