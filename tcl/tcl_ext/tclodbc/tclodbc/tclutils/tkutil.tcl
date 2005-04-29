# require tcl 8.0 because of namespaces
if {$tcl_version < "8.0"} {
    return
}

#package provide tclodbc 2.4

namespace eval tclodbc {;
####################################################################
#
# Procedure TkTableInit
#
# Initialize tk table widget for a given result set 
#
# Parameters:
# table      : tktable widget name
# stmt       : the name of a statement object
#

namespace export TkTableInit
proc TkTableInit {table stmt} {
    set coltypes [$stmt columns type precision scale displaysize]
    set collabels [$stmt columns label]
    set columns  [llength $collabels]

    # general tags
    $table tag configure title -anchor center
    $table configure -titlerows 1 -cols $columns -rows 1
    
    # column tags
    for {set i 0} {$i < $columns} {incr i} {
	set label [lindex $collabels $i]
	set type [lindex $coltypes $i]

	$table set 0,$i $label

	switch [Justification $type] {
	    right  {set anchor e}
	    left   {set anchor w}
	    center {set anchor center}
	}
	$table tag configure c$label -anchor $anchor
	$table tag col c$label $i
	$table width $i [lindex $type 3]
    }
}
# end proc TkTableInit

####################################################################
#
# Procedure TkTableRead
#
# Display whole result set in a tk table widget. The statement object
# should be executed before calling this
#
# Parameters:
# stmt       : the name of a statement object
# table      : tktable widget name
#

namespace export TkTableRead
proc TkTableRead {table stmt} {
    set collabels [$stmt columns label]
    set columns  [llength $collabels]

    set rownum 0
    $table configure -rows [expr $rownum + 1]
    while {[set row [stmt fetch]] != {}} {
	incr rownum
	$table configure -rows [expr $rownum + 1]
	for {set i 0} {$i < $columns} {incr i} {
	    $table set $rownum,$i [string trim [lindex $row $i]]
	}
    }
}
# end proc TkTableRead

}; # end namespace tclodbc
