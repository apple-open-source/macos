#==============================================================================
# Main Tablelist and Tablelist_tile package module.
#
# Copyright (c) 2000-2008  Csaba Nemethi (E-mail: csaba.nemethi@t-online.de)
#==============================================================================

namespace eval ::tablelist {
    #
    # Public variables:
    #
    variable version	4.10
    variable library	[DIR]

    #
    # Creates a new tablelist widget:
    #
    namespace export	tablelist

    #
    # Sort the items of a tablelist widget by one or more of its columns:
    #
    namespace export	sortByColumn addToSortColumns

    #
    # Helper procedures used in binding scripts:
    #
    namespace export	getTablelistPath convEventFields

    #
    # Register various widgets for interactive cell editing:
    #
    namespace export	addBWidgetEntry addBWidgetSpinBox addBWidgetComboBox
    namespace export    addIncrEntryfield addIncrDateTimeWidget \
			addIncrSpinner addIncrSpinint addIncrCombobox
    namespace export	addOakleyCombobox
    namespace export	addDateMentry addTimeMentry addDateTimeMentry \
			addFixedPointMentry addIPAddrMentry
}

package provide tablelist::common $::tablelist::version

#
# The following procedure, invoked in "tablelist.tcl" and "tablelist_tile.tcl",
# sets the variable ::tablelist::usingTile to the given value and sets a trace
# on this variable.
#
proc ::tablelist::useTile {bool} {
    variable usingTile $bool
    trace variable usingTile wu [list ::tablelist::restoreUsingTile $bool]
}

#
# The following trace procedure is executed whenever the variable
# ::tablelist::usingTile is written or unset.  It restores the variable to its
# original value, given by the first argument.
#
proc ::tablelist::restoreUsingTile {origVal varName index op} {
    variable usingTile $origVal
    switch $op {
	w {
	    return -code error "it is not allowed to use both Tablelist and\
				Tablelist_tile in the same application"
	}
	u {
	    trace variable usingTile wu \
		  [list ::tablelist::restoreUsingTile $origVal]
	}
    }
}

interp alias {} ::tk::frame {} ::frame
interp alias {} ::tk::label {} ::label

#
# Everything else needed is lazily loaded on demand, via the dispatcher
# set up in the subdirectory "scripts" (see the file "tclIndex").
#
lappend auto_path [file join $::tablelist::library scripts]
