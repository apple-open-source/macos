# $XConsortium: combobox.tcl /main/1 1996/09/21 14:15:02 kaleb $
#
#
#
#
# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/tcllib/combobox.tcl,v 3.6 1996/12/27 06:54:54 dawes Exp $
#
# Copyright 1996 by Joseph V. Moss <joe@XFree86.Org>
#
# See the file "LICENSE" for information regarding redistribution terms,
# and for a DISCLAIMER OF ALL WARRANTIES.
#

# Implements a simple combobox widget

proc combobox {w args} {
	global tcl_library

	frame $w
	# putting the # in front does a pretty good job of hiding things
	rename $w #$w.frame
	proc $w {args} "eval [list \\#combobox_proc $w] \$args"

	entry $w.entry -relief sunken -bd 1m
	button $w.button -command "\\#combobox_popup [list $w]" \
		-bitmap @$tcl_library/downarrow.xbm
	pack $w.entry $w.button -side left

	toplevel $w.popup -cursor top_left_arrow

	listbox $w.popup.list -yscroll "$w.popup.sb set" \
		-selectmode browse -relief sunken -bd 1m
	scrollbar $w.popup.sb -command "$w.popup.list yview"

	set topwin [winfo toplevel $w]
	wm withdraw $w.popup
	wm transient $w.popup $topwin
	wm overrideredirect $w.popup 1

	bind $w.popup.list <ButtonRelease-1> "\\#combobox_buttonrel [list $w]"

	if [llength $args] {
		eval "\\#combobox_proc [list $w] econfig $args"
	}
}

proc #combobox_proc {w op args} {

	set p $w.popup
	switch -- $op {
	    activate	{return [eval [list $p.list  activate]  $args]}
	    bbox	{return [eval [list $p.list  bbox]      $args]}
	    ecget	{return [eval [list $w.entry cget]      $args]}
	    lcget	{return [eval [list $p.list  cget]      $args]}
	    econfig	{return [eval [list $w.entry configure] $args]}
	    lconfig	{return [eval [list $p.list  configure] $args]}
	    curselection {return [eval \
				[list $p.list curselection] $args]}
	    edelete	{return [eval [list $w.entry delete]    $args]}
	    ldelete	{return [eval [list $p.list  delete]    $args]}
	    eget	{return [eval [list $w.entry get]       $args]}
	    lget	{return [eval [list $p.list  get]       $args]}
	    icursor	{return [eval [list $w.entry icursor]   $args]}
	    eindex	{return [eval [list $w.entry index]     $args]}
	    lindex	{return [eval [list $p.list  index]     $args]}
	    einsert	{return [eval [list $w.entry insert]    $args]}
	    linsert	{return [eval [list $p.list  insert]    $args]}
	    nearest	{return [eval [list $p.list  nearest]   $args]}
	    escan	{return [eval [list $w.entry scan]      $args]}
	    lscan	{return [eval [list $p.list  scan]      $args]}
	    see		{return [eval [list $p.list  see]       $args]}
	    eselection	{return [eval [list $w.entry selection] $args]}
	    lselection	{return [eval [list $p.list  selection] $args]}
	    size	{return [eval [list $p.list  size]      $args]}
	    exview	{return [eval [list $w.entry xview]     $args]}
	    lxview	{return [eval [list $p.list  xview]     $args]}
	    yview	{return [eval [list $p.list  yview]     $args]}
	    default	{error "Unknown option" }
	}
}

proc #combobox_popup { w } {
	global tcl_library #combobox_vars

	set count [$w.popup.list size]
	if { $count == 0 } return
	pack forget $w.popup.sb $w.popup.list
	set #combobox_vars(focus) [focus]
	if { $count > 10 } {
		set wid [winfo width $w.entry]
		$w.popup.list configure -height 10 -width [$w.entry cget -width]
		#$w.popup.list configure -height 10
		incr wid [expr [winfo width $w.button] +1 ]
		pack $w.popup.list -side left -fill x -expand yes
		pack $w.popup.sb   -side left -fill y -expand yes
	} else {
		set wid [winfo width $w.entry]
		#$w.popup.list configure -height $count -width $wid
		$w.popup.list configure -height $count -width [$w.entry cget -width]
		pack $w.popup.list -side left -fill x -expand yes
	}
	update idletasks
	set ht   [winfo reqheight $w.popup]
	set xpos [winfo rootx $w]
	set ypos [expr [winfo rooty $w]+[winfo reqheight $w]]
	wm geometry $w.popup ${wid}x${ht}+${xpos}+${ypos}
	#$w.popup.list configure -width [winfo width $w.entry]
	#pack $w.popup.sb   -side left -fill y -expand yes
	wm deiconify $w.popup
	raise $w.popup
	#$w.button configure -state disabled
	#grab -global $w.popup
	grab $w.popup
	bind $w.popup <ButtonPress-1> "\\#combobox_checkmsepos [list $w] %X %Y"
	bind $w.popup.list <Return> "\\#combobox_popdown [list $w]"
	#bind $w.popup.list <Escape> "\\#combobox_popdown [list $w]"
	$w.button configure -command "\\#combobox_popdown [list $w]" \
		-bitmap @$tcl_library/uparrow.xbm
	set #combobox_vars($w,x) [winfo rootx $w]
	set #combobox_vars($w,y) [winfo rooty $w]
	bind [winfo toplevel $w] <Configure> "\\#combobox_follow [list $w]"
	if [string length [focus]] {
		focus $w.popup.list
	}
}

proc #combobox_follow { w } {
	global #combobox_vars

	regexp {([0-9]+)x([0-9]+)\+([0-9]+)\+([0-9]+)} \
		[wm geometry $w.popup] dummy pw ph px py
	set oldx [set #combobox_vars($w,x)]
	set oldy [set #combobox_vars($w,y)]
	set newx [expr $px+[winfo rootx $w]-$oldx]
	set newy [expr $py+[winfo rooty $w]-$oldy]
	wm geometry $w.popup ${pw}x${ph}+${newx}+${newy}
	set #combobox_vars($w,x) [winfo rootx $w]
	set #combobox_vars($w,y) [winfo rooty $w]
}

proc #combobox_popdown { w } {
	global tcl_library #combobox_vars

	wm withdraw $w.popup
	#$w.button configure -state normal
	grab release $w.popup
	if { [info exists #combobox_vars(focus)]
		&& [string length [set #combobox_vars(focus)]] } {
	    focus [set #combobox_vars(focus)]
	}
	set entry ""
	foreach selection [$w.popup.list curselection] {
	    set text [$w.popup.list get $selection]
	    if {    [string compare $text "<None>"  ] != 0 &&
		    [string compare $text "<Probed>"] != 0 } {
		append entry ",$text"
	    }
	}
	set oldstate [$w.entry cget -state]
	$w.entry configure -state normal
	$w.entry delete 0 end
	$w.entry insert end [string range $entry 1 end]
	$w.entry configure -state $oldstate
	$w.button configure -command "\\#combobox_popup [list $w]" \
		-bitmap @$tcl_library/downarrow.xbm
}

proc #combobox_buttonrel { w } {
	set mode [$w.popup.list cget -selectmode]
	if { "$mode" == "multiple" || "$mode" == "extended" } return
	\#combobox_popdown $w
}

proc #combobox_checkmsepos { w xpos ypos } {
	set curwin [winfo containing $xpos $ypos]
	if { "[winfo toplevel $curwin]" == "$w.popup" } return
	\#combobox_popdown $w
}

