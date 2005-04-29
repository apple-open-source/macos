bind TreeCtrl <Motion> {
    TreeCtrl::CursorCheck %W %x %y
}
bind TreeCtrl <Leave> {
    TreeCtrl::CursorCancel %W
}

bind TreeCtrl <ButtonPress-1> {
    TreeCtrl::ButtonPress1 %W %x %y
}

bind TreeCtrl <Double-ButtonPress-1> {
    TreeCtrl::DoubleButton1 %W %x %y
}

bind TreeCtrl <Button1-Motion> {
    TreeCtrl::Motion1 %W %x %y
}
bind TreeCtrl <ButtonRelease-1> {
    TreeCtrl::Release1 %W %x %y
}
bind TreeCtrl <Shift-ButtonPress-1> {
    set TreeCtrl::Priv(buttonMode) normal
    TreeCtrl::BeginExtend %W [%W index {nearest %x %y}]
}
bind TreeCtrl <Control-ButtonPress-1> {
    set TreeCtrl::Priv(buttonMode) normal
    TreeCtrl::BeginToggle %W [%W index {nearest %x %y}]
}
bind TreeCtrl <Button1-Leave> {
    TreeCtrl::Leave1 %W %x %y
}
bind TreeCtrl <Button1-Enter> {}

bind TreeCtrl <KeyPress-Up> {
    TreeCtrl::SetActiveItem %W [TreeCtrl::UpDown %W -1]
}
bind TreeCtrl <Shift-KeyPress-Up> {
    TreeCtrl::ExtendUpDown %W above
}
bind TreeCtrl <KeyPress-Down> {
    TreeCtrl::SetActiveItem %W [TreeCtrl::UpDown %W 1]
}
bind TreeCtrl <Shift-KeyPress-Down> {
    TreeCtrl::ExtendUpDown %W below
}
bind TreeCtrl <KeyPress-Left> {
    TreeCtrl::SetActiveItem %W [TreeCtrl::LeftRight %W -1]
}
bind TreeCtrl <Shift-KeyPress-Left> {
    TreeCtrl::ExtendUpDown %W left
}
bind TreeCtrl <Control-KeyPress-Left> {
    %W xview scroll -1 pages
}
bind TreeCtrl <KeyPress-Right> {
    TreeCtrl::SetActiveItem %W [TreeCtrl::LeftRight %W 1]
}
bind TreeCtrl <Shift-KeyPress-Right> {
    TreeCtrl::ExtendUpDown %W right
}
bind TreeCtrl <Control-KeyPress-Right> {
    %W xview scroll 1 pages
}
bind TreeCtrl <KeyPress-Prior> {
    %W yview scroll -1 pages
    %W activate {nearest 0 0}
}
bind TreeCtrl <KeyPress-Next> {
    %W yview scroll 1 pages
    %W activate {nearest 0 0}
}
bind TreeCtrl <Control-KeyPress-Prior> {
    %W xview scroll -1 pages
}
bind TreeCtrl <Control-KeyPress-Next> {
    %W xview scroll 1 pages
}
bind TreeCtrl <KeyPress-Home> {
    %W xview moveto 0
}
bind TreeCtrl <KeyPress-End> {
    %W xview moveto 1
}
bind TreeCtrl <Control-KeyPress-Home> {
    %W activate {first visible}
    %W see active
    %W selection modify active all
}
bind TreeCtrl <Shift-Control-KeyPress-Home> {
    TreeCtrl::DataExtend %W 0
}
bind TreeCtrl <Control-KeyPress-End> {
    %W activate {last visible}
    %W see active
    %W selection modify active all
}
bind TreeCtrl <Shift-Control-KeyPress-End> {
    TreeCtrl::DataExtend %W [%W index {last visible}]
}
bind TreeCtrl <<Copy>> {
    if {[string equal [selection own -displayof %W] "%W"]} {
	clipboard clear -displayof %W
	clipboard append -displayof %W [selection get -displayof %W]
    }
}
bind TreeCtrl <KeyPress-space> {
    TreeCtrl::BeginSelect %W [%W index active]
}
bind TreeCtrl <KeyPress-Select> {
    TreeCtrl::BeginSelect %W [%W index active]
}
bind TreeCtrl <Control-Shift-KeyPress-space> {
    TreeCtrl::BeginExtend %W [%W index active]
}
bind TreeCtrl <Shift-KeyPress-Select> {
    TreeCtrl::BeginExtend %W [%W index active]
}
bind TreeCtrl <KeyPress-Escape> {
    TreeCtrl::Cancel %W
}
bind TreeCtrl <Control-KeyPress-slash> {
    TreeCtrl::SelectAll %W
}
bind TreeCtrl <Control-KeyPress-backslash> {
    if {[string compare [%W cget -selectmode] "browse"]} {
	%W selection clear
    }
}

bind TreeCtrl <KeyPress-plus> {
    %W expand [%W index active]
}
bind TreeCtrl <KeyPress-minus> {
    %W collapse [%W index active]
}
bind TreeCtrl <KeyPress-Return> {
    %W toggle [%W index active]
}


# Additional Tk bindings that aren't part of the Motif look and feel:

bind TreeCtrl <ButtonPress-2> {
    TreeCtrl::ScanMark %W %x %y
}
bind TreeCtrl <Button2-Motion> {
    TreeCtrl::ScanDrag %W %x %y
}

if {$tcl_platform(platform) eq "windows"} {
    bind TreeCtrl <Control-ButtonPress-3> {
	TreeCtrl::ScanMark %W %x %y
    }
    bind TreeCtrl <Control-Button3-Motion> {
	TreeCtrl::ScanDrag %W %x %y
    }
}

# The MouseWheel will typically only fire on Windows.  However,
# someone could use the "event generate" command to produce one
# on other platforms.

bind TreeCtrl <MouseWheel> {
    %W yview scroll [expr {- (%D / 120) * 4}] units
}

if {[string equal "unix" $tcl_platform(platform)]} {
    # Support for mousewheels on Linux/Unix commonly comes through mapping
    # the wheel to the extended buttons.  If you have a mousewheel, find
    # Linux configuration info at:
    #	http://www.inria.fr/koala/colas/mouse-wheel-scroll/
    bind TreeCtrl <4> {
	if {!$tk_strictMotif} {
	    %W yview scroll -5 units
	}
    }
    bind TreeCtrl <5> {
	if {!$tk_strictMotif} {
	    %W yview scroll 5 units
	}
    }
}

namespace eval ::TreeCtrl {
    variable Priv
    array set Priv {
	prev {}
	rnc {}
    }
}

proc ::TreeCtrl::CursorCheck {w x y} {
    variable Priv
    set id [$w identify $x $y]
    if {([llength $id] == 3) && ([lindex $id 0] eq "header")} {
	set column [lindex $id 1]
	set side [lindex $id 2]
	set visCount 0
	for {set i 0} {$i < [$w numcolumns]} {incr i} {
	    if {[$w column cget $i -visible]} {
		lappend visColumns $i
		if {$i eq $column} {
		    set columnIndex $visCount
		}
		incr visCount
	    }
	}
	lappend visColumns tail
	if {$column eq "tail"} {
	    set columnIndex $visCount
	}
	if {$side eq "left"} {
	    if {$column eq [lindex $visColumns 0]} {
		return
	    }
	    set column [lindex $visColumns [expr {$columnIndex - 1}]]
	}
	if {![info exists Priv(cursor,$w)]} {
	    set Priv(cursor,$w) [$w cget -cursor]
	    $w configure -cursor sb_h_double_arrow
	    if {[info exists Priv(cursor,afterId,$w)]} {
		after cancel $Priv(cursor,afterId,$w)
	    }
	    set Priv(cursor,afterId,$w) [after 150 [list TreeCtrl::CursorCheckAux $w]]
	}
	return
    }
    CursorCancel $w
    return
}

proc ::TreeCtrl::CursorCheckAux {w} {
    variable Priv
    set x [winfo pointerx $w]
    set y [winfo pointery $w]
    if {[info exists Priv(cursor,$w)]} {
	set x [expr {$x - [winfo rootx $w]}]
	set y [expr {$y - [winfo rooty $w]}]
	CursorCheck $w $x $y
    }
    return
}

proc ::TreeCtrl::CursorCancel {w} {
    variable Priv
    if {[info exists Priv(cursor,$w)]} {
	$w configure -cursor $Priv(cursor,$w)
	unset Priv(cursor,$w)
    }
    if {[info exists Priv(cursor,afterId,$w)]} {
	after cancel $Priv(cursor,afterId,$w)
	unset Priv(cursor,afterId,$w)
    }
    return
}

proc ::TreeCtrl::ButtonPress1 {w x y} {
    variable Priv
    focus $w
    set id [$w identify $x $y]
    if {$id eq ""} {
	return
    }
    if {[lindex $id 0] eq "item"} {
	foreach {where item arg1 arg2} $id {}
	if {$arg1 eq "button"} {
	    $w toggle $item
	    return
	} elseif {$arg1 eq "line"} {
	    $w toggle $arg2
	    return
	}
    }
    set Priv(buttonMode) ""
    if {[lindex $id 0] eq "header"} {
	set column [lindex $id 1]
	set visCount 0
	for {set i 0} {$i < [$w numcolumns]} {incr i} {
	    if {[$w column cget $i -visible]} {
		lappend visColumns $i
		if {$i eq $column} {
		    set columnIndex $visCount
		}
		incr visCount
	    }
	}
	lappend visColumns tail
	if {$column eq "tail"} {
	    set columnIndex $visCount
	}
	if {[llength $id] == 3} {
	    set side [lindex $id 2]
	    if {$side == "left"} {
		if {$column eq [lindex $visColumns 0]} {
		    return
		}
		set column [lindex $visColumns [expr {$columnIndex - 1}]]
	    }
	    set Priv(buttonMode) resize
	    set Priv(column) $column
	    set Priv(x) $x
	    set Priv(y) $y
	    set Priv(width) [$w column width $column]
	    return
	}
	if {$column eq "tail"} return
	if {![$w column cget $column -button]} return
	set Priv(buttonMode) header
	set Priv(column) $column
	$w column configure $column -sunken yes
	return
    }
    set Priv(buttonMode) normal
    BeginSelect $w [lindex $id 1]
    return
}

# Double-click between columns to set default column width
proc ::TreeCtrl::DoubleButton1 {w x y} {
    set id [$w identify $x $y]
    if {$id eq ""} {
	return
    }
    if {[lindex $id 0] eq "item"} {
	foreach {where item arg1 arg2} $id {}
	if {$arg1 eq "button"} {
	    $w toggle $item
	    return
	} elseif {$arg1 eq "line"} {
	    $w toggle $arg2
	    return
	}
    }
    if {[lindex $id 0] eq "header"} {
	set column [lindex $id 1]
	set visCount 0
	for {set i 0} {$i < [$w numcolumns]} {incr i} {
	    if {[$w column cget $i -visible]} {
		lappend visColumns $i
		if {$i eq $column} {
		    set columnIndex $visCount
		}
		incr visCount
	    }
	}
	lappend visColumns tail
	if {$column eq "tail"} {
	    set columnIndex $visCount
	}
	if {[llength $id] == 3} {
	    set side [lindex $id 2]
	    if {$side == "left"} {
		if {$column eq [lindex $visColumns 0]} {
		    return
		}
		set column [lindex $visColumns [expr {$columnIndex - 1}]]
	    }
	    if {$column eq "tail"} return
	    $w column configure $column -width ""
	}
    }
    return
}

proc ::TreeCtrl::Motion1 {w x y} {
    variable Priv
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	header {
	    set id [$w identify $x $y]
	    if {![string match "header $Priv(column)*" $id]} {
		if {[$w column cget $Priv(column) -sunken]} {
		    $w column configure $Priv(column) -sunken no
		}
	    } else {
		if {![$w column cget $Priv(column) -sunken]} {
		    $w column configure $Priv(column) -sunken yes
		}
	    }
	}
	normal {
	    set Priv(x) $x
	    set Priv(y) $y
	    Motion $w [$w index [list nearest $x $y]]
	    AutoScanCheck $w $x $y
	}
	resize {
	    set width [expr {$Priv(width) + $x - $Priv(x)}]
	    set minWidth [$w column cget $Priv(column) -minwidth]
	    if {$minWidth eq ""} {
		set minWidth 0
	    }
	    if {$width < $minWidth} {
		set width $minWidth
	    }
	    if {$width == 0} {
		incr width
	    }
	    scan [$w column bbox $Priv(column)] "%d %d %d %d" x1 y1 x2 y2
	    # Use "ne" because -columnproxy could be ""
	    if {($x1 + $width - 1) ne [$w cget -columnproxy]} {
		$w configure -columnproxy [expr {$x1 + $width - 1}]
	    }
	}
    }
    return
}

proc ::TreeCtrl::Leave1 {w x y} {
    variable Priv
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	header {
	    if {[$w column cget $Priv(column) -sunken]} {
		$w column configure $Priv(column) -sunken no
	    }
	}
	normal {
	}
	resize {}
    }
    return
}

proc ::TreeCtrl::Release1 {w x y} {
    variable Priv
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	header {
	    if {[$w column cget $Priv(column) -sunken]} {
		$w column configure $Priv(column) -sunken no
		# Don't generate the event if it wasn't installed
		if {[lsearch -exact [$w notify eventnames] Header] != -1} {
		    $w notify generate <Header-invoke> [list T $w \
							    C $Priv(column)]
		}
	    }
	}
	normal {
	    AutoScanCancel $w
	    $w activate [$w index [list nearest $x $y]]
	}
	resize {
	    if {[$w cget -columnproxy] ne ""} {
		scan [$w column bbox $Priv(column)] "%d %d %d %d" x1 y1 x2 y2
		set width [expr {[$w cget -columnproxy] - $x1 + 1}]
		$w configure -columnproxy {}
		$w column configure $Priv(column) -width $width
		CursorCheck $w $x $y
	    }
	}
    }
    unset Priv(buttonMode)
    return
}

# ::TreeCtrl::BeginSelect --
#
# This procedure is typically invoked on button-1 presses.  It begins
# the process of making a selection in the listbox.  Its exact behavior
# depends on the selection mode currently in effect for the listbox;
# see the Motif documentation for details.
#
# Arguments:
# w -		The listbox widget.
# el -		The element for the selection operation (typically the
#		one under the pointer).  Must be in numerical form.

proc ::TreeCtrl::BeginSelect {w el} {
    variable Priv
    if {$el eq ""} return
    if {[string equal [$w cget -selectmode] "multiple"]} {
	if {[$w selection includes $el]} {
	    $w selection clear $el
	} else {
	    $w selection add $el
	}
    } else {
	$w selection anchor $el
	$w selection modify $el all
	set Priv(selection) {}
	set Priv(prev) $el
    }
}

# ::TreeCtrl::Motion --
#
# This procedure is called to process mouse motion events while
# button 1 is down.  It may move or extend the selection, depending
# on the listbox's selection mode.
#
# Arguments:
# w -		The listbox widget.
# el -		The element under the pointer (must be a number).

proc ::TreeCtrl::Motion {w el} {
    variable Priv
    if {$el eq $Priv(prev)} {
	return
    }
    switch [$w cget -selectmode] {
	browse {
	    $w selection modify $el all
	    set Priv(prev) $el
	}
	extended {
	    set i $Priv(prev)
	    if {$i eq ""} {
		set i $el
		$w selection add $el
	    }
	    if {[$w selection includes anchor]} {
		$w selection clear $i $el
		$w selection add anchor $el
	    } else {
		$w selection clear $i $el
		$w selection clear anchor $el
	    }
	    if {![info exists Priv(selection)]} {
		set Priv(selection) [$w selection get]
	    }
	    while {[$w compare $i < $el] && [$w compare $i < anchor]} {
		if {[lsearch $Priv(selection) $i] >= 0} {
		    $w selection add $i
		}
		set i [$w index "$i next visible"]
	    }
	    while {[$w compare $i > $el] && [$w compare $i > anchor]} {
		if {[lsearch $Priv(selection) $i] >= 0} {
		    $w selection add $i
		}
		set i [$w index "$i prev visible"]
	    }
	    set Priv(prev) $el
	}
    }
}

# ::TreeCtrl::BeginExtend --
#
# This procedure is typically invoked on shift-button-1 presses.  It
# begins the process of extending a selection in the listbox.  Its
# exact behavior depends on the selection mode currently in effect
# for the listbox;  see the Motif documentation for details.
#
# Arguments:
# w -		The listbox widget.
# el -		The element for the selection operation (typically the
#		one under the pointer).  Must be in numerical form.

proc ::TreeCtrl::BeginExtend {w el} {
    if {[string equal [$w cget -selectmode] "extended"]} {
	if {[$w selection includes anchor]} {
	    Motion $w $el
	} else {
	    # No selection yet; simulate the begin-select operation.
	    BeginSelect $w $el
	}
    }
}

# ::TreeCtrl::BeginToggle --
#
# This procedure is typically invoked on control-button-1 presses.  It
# begins the process of toggling a selection in the listbox.  Its
# exact behavior depends on the selection mode currently in effect
# for the listbox;  see the Motif documentation for details.
#
# Arguments:
# w -		The listbox widget.
# el -		The element for the selection operation (typically the
#		one under the pointer).  Must be in numerical form.

proc ::TreeCtrl::BeginToggle {w el} {
    variable Priv
    if {[string equal [$w cget -selectmode] "extended"]} {
	set Priv(selection) [$w selection get]
	set Priv(prev) $el
	$w selection anchor $el
	if {[$w selection includes $el]} {
	    $w selection clear $el
	} else {
	    $w selection add $el
	}
    }
}

proc ::TreeCtrl::CancelRepeat {} {
    variable Priv
    if {[info exists Priv(afterId)]} {
	after cancel $Priv(afterId)
	unset Priv(afterId)
    }
}

proc ::TreeCtrl::AutoScanCheck {w x y} {
    variable Priv
    scan [$w contentbox] "%d %d %d %d" x1 y1 x2 y2
    set margin [winfo pixels $w [$w cget -scrollmargin]]
    if {($x < $x1 + $margin) || ($x >= $x2 - $margin) ||
	($y < $y1 + $margin) || ($y >= $y2 - $margin)} {
	if {![info exists Priv(autoscan,afterId,$w)]} {
	    if {$y >= $y2 - $margin} {
		$w yview scroll 1 units
		set delay [$w cget -yscrolldelay]
	    } elseif {$y < $y1 + $margin} {
		$w yview scroll -1 units
		set delay [$w cget -yscrolldelay]
	    } elseif {$x >= $x2 - $margin} {
		$w xview scroll 1 units
		set delay [$w cget -xscrolldelay]
	    } elseif {$x < $x1 + $margin} {
		$w xview scroll -1 units
		set delay [$w cget -xscrolldelay]
	    }
	    set count [scan $delay "%d %d" d1 d2]
	    if {[info exists Priv(autoscan,scanning,$w)]} {
		if {$count == 2} {
		    set delay $d2
		}
	    } else {
		if {$count == 2} {
		    set delay $d1
		}
		set Priv(autoscan,scanning,$w) 1
	    }
	    Motion $w [$w index "nearest $x $y"]
	    set Priv(autoscan,afterId,$w) [after $delay [list TreeCtrl::AutoScanCheckAux $w]]
	}
	return
    }
    AutoScanCancel $w
    return
}

proc ::TreeCtrl::AutoScanCheckAux {w} {
    variable Priv
    # Not quite sure how this can happen
    if {![info exists Priv(autoscan,afterId,$w)]} return
    unset Priv(autoscan,afterId,$w)
    set x [winfo pointerx $w]
    set y [winfo pointery $w]
    set x [expr {$x - [winfo rootx $w]}]
    set y [expr {$y - [winfo rooty $w]}]
    AutoScanCheck $w $x $y
    return
}

proc ::TreeCtrl::AutoScanCancel {w} {
    variable Priv
    if {[info exists Priv(autoscan,afterId,$w)]} {
	after cancel $Priv(autoscan,afterId,$w)
	unset Priv(autoscan,afterId,$w)
    }
    unset -nocomplain Priv(autoscan,scanning,$w)
    return
}

# ::TreeCtrl::UpDown --
#
# Moves the location cursor (active element) up or down by one element,
# and changes the selection if we're in browse or extended selection
# mode.
#
# Arguments:
# w -		The listbox widget.
# amount -	+1 to move down one item, -1 to move back one item.

proc ::TreeCtrl::UpDown {w n} {
    variable Priv
    set rnc [$w item rnc active]
    # active item isn't visible
    if {$rnc eq ""} {
    	set rnc [$w item rnc first]
    	if {$rnc eq ""} return
    }
    scan $rnc "%d %d" row col
    set Priv(row) [expr {$row + $n}]
    if {$rnc ne $Priv(rnc)} {
   	set Priv(col) $col
    }
    set index [$w index "rnc $Priv(row) $Priv(col)"]
    if {[$w compare active == $index]} {
    	set Priv(row) $row
    } else {
	set Priv(rnc) [$w item rnc $index]
    }
    return $index
}

proc ::TreeCtrl::LeftRight {w n} {
    variable Priv
    set rnc [$w item rnc active]
    if {$rnc eq ""} {
    	set rnc [$w item rnc first]
    	if {$rnc eq ""} return
    }
    scan $rnc "%d %d" row col
    set Priv(col) [expr {$col + $n}]
    if {$rnc ne $Priv(rnc)} {
    	set Priv(row) $row
    }
    set index [$w index "rnc $Priv(row) $Priv(col)"]
    if {[$w compare active == $index]} {
    	set Priv(col) $col
    } else {
	set Priv(rnc) [$w item rnc $index]
    }
    return $index
}

proc ::TreeCtrl::SetActiveItem {w index} {
    if {$index eq ""} return
    $w activate $index
    $w see active
    $w selection modify active all
    switch [$w cget -selectmode] {
	extended {
	    $w selection anchor active
	    set Priv(prev) [$w index active]
	    set Priv(selection) {}
	}
    }
}

# ::TreeCtrl::ExtendUpDown --
#
# Does nothing unless we're in extended selection mode;  in this
# case it moves the location cursor (active element) up or down by
# one element, and extends the selection to that point.
#
# Arguments:
# w -		The listbox widget.
# amount -	+1 to move down one item, -1 to move back one item.

proc ::TreeCtrl::ExtendUpDown {w amount} {
    variable Priv
    if {[string compare [$w cget -selectmode] "extended"]} {
	return
    }
    set active [$w index active]
    if {![info exists Priv(selection)]} {
	$w selection add $active
	set Priv(selection) [$w selection get]
    }
    set index [$w index "active $amount"]
    if {$index eq ""} return
    $w activate $index
    $w see active
    Motion $w [$w index active]
}

# ::TreeCtrl::DataExtend
#
# This procedure is called for key-presses such as Shift-KEndData.
# If the selection mode isn't multiple or extend then it does nothing.
# Otherwise it moves the active element to el and, if we're in
# extended mode, extends the selection to that point.
#
# Arguments:
# w -		The listbox widget.
# el -		An integer element number.

proc ::TreeCtrl::DataExtend {w el} {
    set mode [$w cget -selectmode]
    if {[string equal $mode "extended"]} {
	$w activate $el
	$w see $el
        if {[$w selection includes anchor]} {
	    Motion $w $el
	}
    } elseif {[string equal $mode "multiple"]} {
	$w activate $el
	$w see $el
    }
}

# ::TreeCtrl::Cancel
#
# This procedure is invoked to cancel an extended selection in
# progress.  If there is an extended selection in progress, it
# restores all of the items between the active one and the anchor
# to their previous selection state.
#
# Arguments:
# w -		The listbox widget.

proc ::TreeCtrl::Cancel w {
    variable Priv
    if {[string compare [$w cget -selectmode] "extended"]} {
	return
    }
    set first [$w index anchor]
    set last $Priv(prev)
    if { [string equal $last ""] } {
	# Not actually doing any selection right now
	return
    }
    if {[$w compare $first > $last]} {
	set tmp $first
	set first $last
	set last $tmp
    }
    $w selection clear $first $last
    while {[$w compare $first <= $last]} {
	if {[lsearch $Priv(selection) $first] >= 0} {
	    $w selection add $first
	}
	set first [$w index "$first next visible"]
    }
}

# ::TreeCtrl::SelectAll
#
# This procedure is invoked to handle the "select all" operation.
# For single and browse mode, it just selects the active element.
# Otherwise it selects everything in the widget.
#
# Arguments:
# w -		The listbox widget.

proc ::TreeCtrl::SelectAll w {
    set mode [$w cget -selectmode]
    if {[string equal $mode "single"] || [string equal $mode "browse"]} {
	$w selection modify active all
    } else {
	$w selection add all
    }
}

proc ::TreeCtrl::MarqueeBegin {w x y} {
    set x [$w canvasx $x]
    set y [$w canvasy $y]
    $w marquee coords $x $y $x $y
    $w marquee visible yes
    return
}

proc ::TreeCtrl::MarqueeUpdate {w x y} {
    set x [$w canvasx $x]
    set y [$w canvasy $y]
    $w marquee corner $x $y
    return
}
proc ::TreeCtrl::MarqueeEnd {w x y} {
    $w marquee visible no
    return
}

proc ::TreeCtrl::ScanMark {w x y} {
    variable Priv
    $w scan mark $x $y
    set Priv(x) $x
    set Priv(y) $y
    set Priv(mouseMoved) 0
    return
}

proc ::TreeCtrl::ScanDrag {w x y} {
    variable Priv
    if {![info exists Priv(x)]} { set Priv(x) $x }
    if {![info exists Priv(y)]} { set Priv(y) $y }
    if {($x != $Priv(x)) || ($y != $Priv(y))} {
	set Priv(mouseMoved) 1
    }
    if {[info exists Priv(mouseMoved)] && $Priv(mouseMoved)} {
	$w scan dragto $x $y
    }
    return
}
