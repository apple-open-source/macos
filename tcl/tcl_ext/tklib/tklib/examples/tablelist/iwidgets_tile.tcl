#!/bin/sh
# the next line restarts using wish \
exec wish "$0" ${1+"$@"}

#==============================================================================
# Demonstrates the interactive tablelist cell editing with the aid of some
# widgets from the Iwidgets package and of the Tk core checkbutton widget.
#
# Copyright (c) 2004-2010  Csaba Nemethi (E-mail: csaba.nemethi@t-online.de)
#==============================================================================

package require tablelist_tile 5.1
package require Iwidgets

wm title . "Serial Line Configuration"

#
# Add some entries to the Tk option database
#
set dir [file dirname [info script]]
source [file join $dir option_tile.tcl]
option add *Tablelist*Checkbutton.background		white
option add *Tablelist*Checkbutton.activeBackground	white
option add *Tablelist*textBackground			white
option add *Tablelist*Entry.disabledBackground		white
option add *Tablelist*Entry.disabledForeground		black
option add *Tablelist*Dateentry*Label.background	white
option add *Tablelist*Timeentry*Label.background	white

#
# Register some widgets from the Iwidgets package for interactive cell editing
#
tablelist::addIncrEntryfield
tablelist::addIncrSpinint
tablelist::addIncrCombobox
tablelist::addIncrDateTimeWidget dateentry -seconds
tablelist::addIncrDateTimeWidget timeentry -seconds

#
# Create two images, to be displayed in tablelist cells with boolean values
#
set checkedImg   [image create photo -file [file join $dir checked.gif]]
set uncheckedImg [image create photo -file [file join $dir unchecked.gif]]

#
# Improve the window's appearance by using a tile
# frame as a container for the other widgets
#
set f [ttk::frame .f]

#
# Create a tablelist widget with editable columns (except the first one)
#
set tbl $f.tbl
tablelist::tablelist $tbl \
    -columns {0 "No."		  right
	      0 "Available"	  center
	      0 "Name"		  left
	      0 "Baud Rate"	  right
	      0 "Data Bits"	  center
	      0 "Parity"	  left
	      0 "Stop Bits"	  center
	      0 "Handshake"	  left
	      0 "Activation Date" center
	      0 "Activation Time" center} \
    -editstartcommand editStartCmd -editendcommand editEndCmd \
    -height 0 -width 0
if {[$tbl cget -selectborderwidth] == 0} {
    $tbl configure -spacing 1
}
$tbl columnconfigure 0 -sortmode integer
$tbl columnconfigure 1 -name available -editable yes -editwindow checkbutton \
    -formatcommand emptyStr
$tbl columnconfigure 2 -name lineName  -editable yes -editwindow entryfield \
    -sortmode dictionary
$tbl columnconfigure 3 -name baudRate  -editable yes -editwindow combobox \
    -sortmode integer
$tbl columnconfigure 4 -name dataBits  -editable yes -editwindow spinint
$tbl columnconfigure 5 -name parity    -editable yes -editwindow combobox
$tbl columnconfigure 6 -name stopBits  -editable yes -editwindow combobox
$tbl columnconfigure 7 -name handshake -editable yes -editwindow combobox
$tbl columnconfigure 8 -name actDate   -editable yes -editwindow dateentry \
    -formatcommand formatDate -sortmode integer
$tbl columnconfigure 9 -name actTime   -editable yes -editwindow timeentry \
    -formatcommand formatTime -sortmode integer

proc emptyStr   val { return "" }
proc formatDate val { return [clock format $val -format "%Y-%m-%d"] }
proc formatTime val { return [clock format $val -format "%H:%M:%S"] }

#
# Populate the tablelist widget; set the activation
# date & time to 10 minutes past the current clock value
#
set clock [clock seconds]
incr clock 600
for {set n 1} {$n <= 8} {incr n} {
    $tbl insert end [list $n 1 "Line $n" 9600 8 None 1 XON/XOFF $clock $clock]
    $tbl cellconfigure end,available -image $checkedImg
}
for {set n 9} {$n <= 16} {incr n} {
    $tbl insert end [list $n 0 "Line $n" 9600 8 None 1 XON/XOFF $clock $clock]
    $tbl cellconfigure end,available -image $uncheckedImg
}

set btn [ttk::button $f.btn -text "Close" -command exit]

#
# Manage the widgets
#
pack $btn -side bottom -pady 10
pack $tbl -side top -expand yes -fill both
pack $f -expand yes -fill both

#------------------------------------------------------------------------------
# editStartCmd
#
# Applies some configuration options to the edit window; if the latter is a
# combobox, the procedure populates it.
#------------------------------------------------------------------------------
proc editStartCmd {tbl row col text} {
    set w [$tbl editwinpath]

    switch [$tbl columncget $col -name] {
	lineName {
	    #
	    # Set an upper limit of 20 for the number of characters
	    #
	    $w configure -pasting no -fixed 20
	}

	baudRate {
	    #
	    # Populate the combobox and allow no more
	    # than 6 digits in its entry component
	    #
	    $w insert list end 50 75 110 300 1200 2400 4800 9600 19200 38400 \
			       57600 115200 230400 460800 921600
	    $w configure -pasting no -fixed 6 -validate numeric
	}

	dataBits {
	    #
	    # Configure the spinint widget
	    #
	    $w configure -range {5 8} -wrap no -pasting no -fixed 1 \
			 -validate {regexp {^[5-8]$} %c}
	}

	parity {
	    #
	    # Populate the combobox and make it non-editable
	    #
	    $w insert list end None Even Odd Mark Space
	    $w configure -editable no -listheight 120
	}

	stopBits {
	    #
	    # Populate the combobox and make it non-editable
	    #
	    $w insert list end 1 1.5 2
	    $w configure -editable no -listheight 90
	}

	handshake {
	    #
	    # Populate the combobox and make it non-editable
	    #
	    $w insert list end XON/XOFF RTS/CTS None
	    $w configure -editable no -listheight 90
	}

	actDate {
	    #
	    # Set the date format "%Y-%m-%d"
	    #
	    $w configure -int yes
	}

	actTime {
	    #
	    # Set the time format "%H:%M:%S"
	    #
	    $w configure -format military
	}
    }

    return $text
}

#------------------------------------------------------------------------------
# editEndCmd
#
# Performs a final validation of the text contained in the edit window and gets
# the cell's internal contents.
#------------------------------------------------------------------------------
proc editEndCmd {tbl row col text} {
    switch [$tbl columncget $col -name] {
	available {
	    #
	    # Update the image contained in the cell
	    #
	    set img [expr {$text ? $::checkedImg : $::uncheckedImg}]
	    $tbl cellconfigure $row,$col -image $img
	}

	baudRate {
	    #
	    # Check whether the baud rate is an integer in the range 50..921600
	    #
	    if {![regexp {^[0-9]+$} $text] || $text < 50 || $text > 921600} {
		bell
		tk_messageBox -title "Error" -icon error -message \
		    "The baud rate must be an integer in the range 50..921600"
		$tbl rejectinput
	    }
	}

	dataBits {
	    #
	    # Check whether the # of data bits is an integer in the range 5..8
	    #
	    if {![regexp {^[5-8]$} $text]} {
		bell
		tk_messageBox -title "Error" -icon error -message \
		    "The # of data bits must be an integer in the range 5..8"
		$tbl rejectinput
	    }
	}

	actDate {
	    #
	    # Check whether the activation clock value is later than the
	    # current one; if this is the case then make sure the cells
	    # "actDate" and "actTime" will have the same internal value
	    #
	    set actTime [$tbl cellcget $row,actTime -text]
	    set actClock [clock scan [formatTime $actTime] -base $text]
	    if {$actClock <= [clock seconds]} {
		bell
		tk_messageBox -title "Error" -icon error -message \
		    "The activation date & time must be in the future"
		$tbl rejectinput
	    } else {
		$tbl cellconfigure $row,actTime -text $actClock
		return $actClock
	    }
	}

	actTime {
	    #
	    # Check whether the activation clock value is later than the
	    # current one; if this is the case then make sure the cells
	    # "actDate" and "actTime" will have the same internal value
	    #
	    set actDate [$tbl cellcget $row,actDate -text]
	    set actClock [clock scan [formatTime $text] -base $actDate]
	    if {$actClock <= [clock seconds]} {
		bell
		tk_messageBox -title "Error" -icon error -message \
		    "The activation date & time must be in the future"
		$tbl rejectinput
	    } else {
		$tbl cellconfigure $row,actDate -text $actClock
		return $actClock
	    }
	}
    }

    return $text
}
