#!/bin/sh
# the next line restarts using wish \
exec wish "$0" ${1+"$@"}

#==============================================================================
# Demonstrates the interactive tablelist cell editing with the aid of some
# widgets from the BWidget package and of the Tk core checkbutton widget.
#
# Copyright (c) 2004-2010  Csaba Nemethi (E-mail: csaba.nemethi@t-online.de)
#==============================================================================

package require tablelist_tile 5.1
package require BWidget

wm title . "Serial Line Configuration"

#
# Add some entries to the Tk option database
#
set dir [file dirname [info script]]
source [file join $dir option_tile.tcl]
option add *Tablelist*Checkbutton.background		white
option add *Tablelist*Checkbutton.activeBackground	white
option add *Tablelist*Entry.background			white

#
# Register some widgets from the BWidget package for interactive cell editing
#
tablelist::addBWidgetEntry
tablelist::addBWidgetSpinBox
tablelist::addBWidgetComboBox

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
$tbl columnconfigure 2 -name lineName  -editable yes -editwindow Entry \
    -sortmode dictionary
$tbl columnconfigure 3 -name baudRate  -editable yes -editwindow ComboBox \
    -sortmode integer
$tbl columnconfigure 4 -name dataBits  -editable yes -editwindow SpinBox
$tbl columnconfigure 5 -name parity    -editable yes -editwindow ComboBox
$tbl columnconfigure 6 -name stopBits  -editable yes -editwindow ComboBox
$tbl columnconfigure 7 -name handshake -editable yes -editwindow ComboBox
$tbl columnconfigure 8 -name actDate   -editable yes -editwindow Entry \
    -formatcommand formatDate -sortmode integer
$tbl columnconfigure 9 -name actTime   -editable yes -editwindow Entry \
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
# ComboBox, the procedure populates it.
#------------------------------------------------------------------------------
proc editStartCmd {tbl row col text} {
    set w [$tbl editwinpath]

    switch [$tbl columncget $col -name] {
	lineName {
	    #
	    # Set an upper limit of 20 for the number of characters
	    #
	    $w configure -invalidcommand bell -validate key \
			 -validatecommand {expr {[string length %P] <= 20}}
	}

	baudRate {
	    #
	    # Populate the ComboBox and allow no more
	    # than 6 digits in its Entry component
	    #
	    $w configure -values {50 75 110 300 1200 2400 4800 9600 19200 38400
				  57600 115200 230400 460800 921600}
	    $w configure -invalidcommand bell -validate key -validatecommand \
		{expr {[string length %P] <= 6 && [regexp {^[0-9]*$} %S]}}
	}

	dataBits {
	    #
	    # Configure the SpinBox
	    #
	    $w configure -range {5 8 1} -editable no
	}

	parity {
	    #
	    # Populate the ComboBox and make it non-editable
	    #
	    $w configure -values {None Even Odd Mark Space} -editable no
	}

	stopBits {
	    #
	    # Populate the ComboBox and make it non-editable
	    #
	    $w configure -values {1 1.5 2} -editable no
	}

	handshake {
	    #
	    # Populate the ComboBox and make it non-editable
	    #
	    $w configure -values {XON/XOFF RTS/CTS None} -editable no
	}

	actDate {
	    #
	    # Set an upper limit of 10 for the number of characters
	    # and allow only digits and the "-" character in it
	    #
	    $w configure -invalidcommand bell -validate key -validatecommand \
		{expr {[string length %P] <= 10 && [regexp {^[0-9-]*$} %S]}}
	}

	actTime {
	    #
	    # Set an upper limit of 8 for the number of characters
	    # and allow only digits and the ":" character in it
	    #
	    $w configure -invalidcommand bell -validate key -validatecommand \
		{expr {[string length %P] <= 8 && [regexp {^[0-9:]*$} %S]}}
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

	actDate {
	    #
	    # Get the activation date in seconds from the last argument 
	    #
	    if {[catch {clock scan $text} actDate] != 0} {
		bell
		tk_messageBox -title "Error" -icon error -message "Invalid date"
		$tbl rejectinput
		return ""
	    }

	    #
	    # Check whether the activation clock value is later than the
	    # current one; if this is the case then make sure the cells
	    # "actDate" and "actTime" will have the same internal value
	    #
	    set actTime [$tbl cellcget $row,actTime -text]
	    set actClock [clock scan [formatTime $actTime] -base $actDate]
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
	    # Get the activation clock value in seconds from the last argument 
	    #
	    set actDate [$tbl cellcget $row,actDate -text]
	    if {[catch {clock scan $text -base $actDate} actClock] != 0} {
		bell
		tk_messageBox -title "Error" -icon error -message "Invalid time"
		$tbl rejectinput
		return ""
	    }

	    #
	    # Check whether the activation clock value is later than the
	    # current one; if this is the case then make sure the cells
	    # "actDate" and "actTime" will have the same internal value
	    #
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
