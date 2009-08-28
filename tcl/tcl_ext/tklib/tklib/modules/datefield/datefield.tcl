##+##########################################################################
#
# datefield.tcl
#
# Implements a datefield entry widget ala Iwidget::datefield
# by Keith Vetter (keith@ebook.gemstar.com)
#
# Datefield creates an entry widget but with a special binding to KeyPress
# (based on Iwidget::datefield) to ensure that the current value is always
# a valid date. All normal entry commands and configurations still work.
#
# Usage:
#  ::datefield::datefield .df -background yellow -textvariable myDate
#  pack .df
#
# Bugs:
#   o won't work if you programmatically put in an invalid date
#     e.g. .df insert end "abc"	  will cause it to behave erratically
#
# Revisions:
# KPV Feb 07, 2002 - initial revision
#
##+##########################################################################
#############################################################################

package require Tk 8.0
package provide datefield 0.2

namespace eval ::datefield {
    namespace export datefield

    # Have the widget use tile/ttk should it be available.

    variable entry entry
    if {![catch {
	package require tile
    }]} {
	set entry ttk::entry
    }

    proc datefield {w args} {
	variable entry

	eval $entry $w -width 10 -justify center $args
	$w insert end [clock format [clock seconds] -format "%m/%d/%Y"]
	$w icursor 0

	bind $w <KeyPress> [list ::datefield::KeyPress $w %A %K %s]
	bind $w <Button1-Motion> break
	bind $w <Button2-Motion> break
	bind $w <Double-Button>	 break
	bind $w <Triple-Button>	 break
	bind $w <2>		 break

	return $w
    }

    # internal routine for all key presses in the datefield entry widget
    proc KeyPress {w char sym state} {
	set icursor [$w index insert]

	# Handle some non-number characters first
	if {$sym == "plus" || $sym == "Up" || \
		$sym == "minus" || $sym == "Down"} {
	    set dir "1 day"
	    if {$sym == "minus" || $sym == "Down"} {
		set dir "-1 day"
	    }
	    set base [clock scan [$w get]]
	    if {[catch {set new [clock scan $dir -base $base]}] != 0} {
		bell
		return -code break
	    }
	    set date [clock format $new -format "%m/%d/%Y"]
	    if {[catch {clock scan $date}]} {
		bell
		return -code break
	    }
	    $w delete 0 end
	    $w insert end $date
	    $w icursor $icursor
	    return -code break
	} elseif {$sym == "Right" || $sym == "Left" || $sym == "BackSpace" || \
		$sym == "Delete"} {
	    set dir -1
	    if {$sym == "Right"} {set dir 1}

	    set icursor [expr {($icursor + 10 + $dir) % 10}]
	    if {$icursor == 2 || $icursor == 5} {;# Don't land on a slash
		set icursor [expr {($icursor + 10 + $dir) % 10}]
	    }
	    $w icursor $icursor
	    return -code break
	} elseif {($sym == "Control_L") || ($sym == "Shift_L") || \
		($sym == "Control_R") || ($sym == "Shift_R")} {
	    return -code break
	} elseif {$sym == "Tab" && $state == 0} {;# Tab key
	    if {$icursor < 3} {
		$w icursor 3
	    } elseif {$icursor < 6} {
		$w icursor 8
	    } else {
		return -code continue
	    }
	    return -code break
	} elseif {$sym == "Tab" && ($state == 1 || $state == 4)} {
	    if {$icursor > 4} {
		$w icursor 3
	    } elseif {$icursor > 1} {
		$w icursor 0
	    } else {
		return -code continue
	    }
	    return -code break
	}

	if {! [regexp {[0-9]} $char]} {		;# Unknown character
	    bell
	    return -code break
	}

	if {$icursor >= 10} {			;# Can't add beyond end
	    bell
	    return -code break
	}
	foreach {month day year} [split [$w get] "/"] break

	# MONTH SECTION
	if {$icursor < 2} {
	    foreach {m1 m2} [split $month ""] break
	    set cursor 3			;# Where to leave the cursor
	    if {$icursor == 0} {		;# 1st digit of month
		if {$char < 2} {
		    set month "$char$m2"
		    set cursor 1
		} else {
		    set month "0$char"
		}
		if {$month > 12} {set month 10}
		if {$month == "00"} {set month "01"}
	    } else {				;# 2nd digit of month
		set month "$m1$char"
		if {$month > 12} {set month "0$char"}
		if {$month == "00"} {
		    bell
		    return -code break
		}
	    }
	    $w delete 0 2
	    $w insert 0 $month
	    # Validate the day of the month
	    if {$day > [set endday [lastDay $month $year]]} {
		$w delete 3 5
		$w insert 3 $endday
	    }
	    $w icursor $cursor

	    return -code break
	}
	# DAY SECTION
	if {$icursor < 5} {			;# DAY
	    set endday [lastDay $month $year]
	    foreach {d1 d2} [split $day ""] break
	    set cursor 6			;# Where to leave the cursor
	    if {$icursor <= 3} {		;# 1st digit of day
		if {$char < 3 || ($char == 3 && $month != "02")} {
		    set day "$char$d2"
		    if {$day == "00"} { set day "01" }
		    if {$day > $endday} {set day $endday}
		    set cursor 4
		} else {
		    set day "0$char"
		}
	    } else {				;# 2nd digit of day
		set day "$d1$char"
		if {$day > $endday || $day == "00"} {
		    bell
		    return -code break
		}
	    }
	    $w delete 3 5
	    $w insert 3 $day
	    $w icursor $cursor
	    return -code break
	}

	# YEAR SECTION
	set y1 [lindex [split $year ""] 0]
	if {$icursor < 7} {			;# 1st digit of year
	    if {$char != "1" && $char != "2"} {
		bell
		return -code break
	    }
	    if {$char != $y1} {			;# Different century
		set y 1999
		if {$char == "2"} {set y 2000 }
		$w delete 6 end
		$w insert end $y
	    }
	    $w icursor 7
	    return -code break
	}
	$w delete $icursor
	$w insert $icursor $char
	if {[catch {clock scan [$w get]}] != 0} {;# Validate the year
	    $w delete 6 end
	    $w insert end $year			;# Put back in the old year
	    $w icursor $icursor
	    bell
	    return -code break
	}
	return -code break
    }
    # internal routine that returns the last valid day of a given month and year
    proc lastDay {month year} {
	set days [clock format [clock scan "+1 month -1 day" \
		-base [clock scan "$month/01/$year"]] -format %d]
    }
}
