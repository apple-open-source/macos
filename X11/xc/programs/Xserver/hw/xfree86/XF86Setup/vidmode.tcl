# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/vidmode.tcl,v 1.1 1999/04/05 07:13:02 dawes Exp $

proc VidMode_describe { mode } {
	set clock	[lindex $mode 0]
	set hdisp	[lindex $mode 1]
	set htotal	[lindex $mode 4]
	set vdisp	[lindex $mode 5]
	set vtotal	[lindex $mode 8]
	set desc [format "%4dx%-4d Clock: %6.2f (%6.2f Hz vert, %6.2f kHz horiz)" \
		$hdisp $vdisp $clock \
		[expr $clock*1000000.0/($htotal*$vtotal)] \
		[expr $clock*1000.0/$htotal] ]
	set flags [string tolower [lrange $mode 9 end]]
	if {[lsearch $flags "interlace"] != -1} {
		append desc " Interlaced"
	}
	if {[lsearch $flags "doublescan"] != -1} {
		append desc " DoubleScan"
	}
	return $desc
}

proc VidMode_create_widgets { win } {
	global vidmListFlag

	VidMode_create_modelist_widgets $win
	VidMode_create_modeadjust_widgets $win

	set vidmListFlag 1
	bind $win n [list VidMode_next $win]
	bind $win p [list VidMode_prev $win]
	#bind $win <Control-Alt-KP_Add>		[list VidMode_next $win]
	#bind $win <Control-Alt-KP_Subtract>	[list VidMode_prev $win]
}

proc VidMode_create_modelist_widgets { win } {
	global vidmMode vidmCurrentModes vidmValidModes MonitorStdModes
	global tcl_library

	set w [winpathprefix $win]
	set vidmCurrentModes [xf86vid_getallmodelines]
	frame	$w.curr
	label	$w.curr.title -anchor w \
		-text "Currently selected/available modes:"
	pack	$w.curr.title -side top -fill x -expand yes
	frame   $w.curr.modes
	listbox	$w.curr.modes.lb  -height 8 -width 70
	foreach mode $vidmCurrentModes {
		$w.curr.modes.lb insert end [VidMode_describe $mode]
	}
	pack	$w.curr.modes.lb -side left -fill x -expand yes
	$w.curr.modes.lb configure -yscroll [list $w.curr.modes.sb set]
	scrollbar $w.curr.modes.sb -command [list $w.curr.modes.lb yview]
	pack	$w.curr.modes.sb -side left -fill y
	frame	$w.curr.buttons
	#frame	$w.curr.buttons.row1
	#frame	$w.curr.buttons.row2
	button	$w.curr.buttons.next -text "Next" \
		    -command [list VidMode_next $win]
	button	$w.curr.buttons.prev -text "Prev" \
		    -command [list VidMode_prev $win]
	button	$w.curr.buttons.move2top -text "Move to Top" \
		    -command [list VidMode_move2top $win]
	button	$w.curr.buttons.delete   -text "Delete" \
		    -command [list VidMode_delete $win]
	#pack	$w.curr.buttons.next $w.curr.buttons.prev \
		    -in $w.curr.buttons.row1 -side left -fill x -expand yes
	#pack	$w.curr.buttons.move2top $w.curr.buttons.delete \
		    -in $w.curr.buttons.row2 -side left -fill x -expand yes
	#pack	$w.curr.buttons.row1 $w.curr.buttons.row2 -side top
	pack	$w.curr.buttons.next $w.curr.buttons.prev \
		$w.curr.buttons.move2top $w.curr.buttons.delete \
		    -side top -fill x -expand yes

	frame	$w.known
	label	$w.known.title -anchor w \
		-text "Known modes likely to work with this card/monitor:"
	pack	$w.known.title -side top -fill x -expand yes
	frame	$w.known.modes
	listbox	$w.known.modes.lb -height 8 -width 70
	set vidmValidModes ""
	foreach mode [lsort [array names MonitorStdModes]] {
		set modeline [string tolower $MonitorStdModes($mode)]
		set retv [xf86vid_checkmodeline $modeline]
		puts "Retv = $retv  Modeline = $modeline"
		if {!$retv} {
			lappend vidmValidModes $modeline
			$w.known.modes.lb insert end [VidMode_describe $modeline]
		} else {
			puts "Rejected: $modeline"
		}
	}
	pack    $w.known.modes.lb -side left -fill x -expand yes
	$w.known.modes.lb configure -yscroll [list $w.known.modes.sb set]
	scrollbar $w.known.modes.sb -command [list $w.known.modes.lb yview]
	pack	$w.known.modes.sb -side left -fill y
	#frame	$w.known.buttons -width 8c -height 4c
	frame	$w.known.buttons
	button	$w.known.buttons.add -text "Add" \
		    -command [list VidMode_add $win]
	button	$w.known.buttons.new -text "New" \
		    -command [list VidMode_new $win]
	button	$w.known.buttons.help -text "Help"
	button	$w.known.buttons.tune -text "Tune" \
		    -command "set vidmListFlag 0;[list VidMode_list_or_adjust $win]"
	pack	$w.known.buttons.add $w.known.buttons.new -side top
	pack	$w.known.buttons.help $w.known.buttons.tune -side top
	#pack	propagate $w.curr.buttons no
	pack	$w.curr.modes $w.curr.buttons -side left -fill x -expand yes
	pack	$w.known.modes $w.known.buttons -side left -fill x -expand yes
	frame	$w.move
	frame	$w.move.but
	button	$w.move.but.left   -text "Left" \
			-command "VidMode_adjust $win left"
	button	$w.move.but.right  -text "Right" \
			-command "VidMode_adjust $win right"
	button	$w.move.but.up   -text "Up" \
			-command "VidMode_adjust $win up"
	button	$w.move.but.down  -text "Down" \
			-command "VidMode_adjust $win down"
	pack	$w.move.but.left $w.move.but.right \
		    $w.move.but.up $w.move.but.down -side left
	frame	$w.size
	frame	$w.size.but
	button	$w.size.but.wider  -bitmap @$tcl_library/wider.xbm \
			-command "VidMode_adjust $win wider"
	button	$w.size.but.narrow -bitmap @$tcl_library/narrower.xbm \
			-command "VidMode_adjust $win narrow"
	button	$w.size.but.taller  -bitmap @$tcl_library/taller.xbm \
			-command "VidMode_adjust $win taller"
	button	$w.size.but.shorter -bitmap @$tcl_library/shorter.xbm \
			-command "VidMode_adjust $win short"
	pack	$w.size.but.wider $w.size.but.narrow \
		    $w.size.but.taller $w.size.but.shorter -side left
	pack	$w.move.but
	pack	$w.size.but
	#pack	$w.move $w.size -side left
	pack	$w.move $w.size
}

proc VidMode_create_modeadjust_widgets { win } {
	global vidmMode vidmCurrentModes vidmValidModes MonitorStdModes
	set w [winpathprefix $win]

	frame	$w.modeline
	label	$w.modeline.header -text "Adjusting mode:"
	frame	$w.horz
	frame	$w.horz.beg
	label	$w.horz.beg.text -text "HSyncStart:" -width 12
	scale	$w.horz.beg.sc -variable vidmMode(HBegin) -orient horizontal \
		    -resolution 4
	pack	$w.horz.beg.text -side left
	pack	$w.horz.beg.sc   -side left -fill x -expand yes
	frame	$w.horz.end
	label	$w.horz.end.text -text "HSyncEnd:" -width 12
	scale	$w.horz.end.sc -variable vidmMode(HEnd) -orient horizontal \
		    -resolution 4
	pack	$w.horz.end.text -side left
	pack	$w.horz.end.sc   -side left -fill x -expand yes
	frame	$w.horz.ttl
	label	$w.horz.ttl.text -text "HorizTotal:" -width 12
	scale	$w.horz.ttl.sc -variable vidmMode(HTotal) -orient horizontal \
		    -resolution 8
	pack	$w.horz.ttl.text -side left
	pack	$w.horz.ttl.sc   -side left -fill x -expand yes
	frame	$w.horz.but
	button	$w.horz.but.left   -text "Left" \
			-command "VidMode_adjust $win left"
	button	$w.horz.but.right  -text "Right" \
			-command "VidMode_adjust $win right"
	button	$w.horz.but.wider  -text "Wider" \
			-command "VidMode_adjust $win wider"
	button	$w.horz.but.narrow -text "Narrower" \
			-command "VidMode_adjust $win narrow"
	pack	$w.horz.but.left $w.horz.but.right $w.horz.but.wider \
		    $w.horz.but.narrow -side left
	pack	$w.horz.beg $w.horz.end $w.horz.ttl $w.horz.but \
		    -side top -fill x -expand yes
	frame	$w.vert
	frame	$w.vert.beg
	label	$w.vert.beg.text -text "VSyncStart:" -width 12
	scale	$w.vert.beg.sc -variable vidmMode(VBegin) -orient horizontal \
		    -resolution 2
	pack	$w.vert.beg.text -side left
	pack	$w.vert.beg.sc   -side left -fill x -expand yes
	frame	$w.vert.end
	label	$w.vert.end.text -text "VSyncEnd:" -width 12
	scale	$w.vert.end.sc -variable vidmMode(VEnd) -orient horizontal \
		    -resolution 2
	pack	$w.vert.end.text -side left
	pack	$w.vert.end.sc   -side left -fill x -expand yes
	frame	$w.vert.ttl
	label	$w.vert.ttl.text -text "VertTotal:" -width 12
	scale	$w.vert.ttl.sc -variable vidmMode(VTotal) -orient horizontal \
		    -resolution 4
	pack	$w.vert.ttl.text -side left
	pack	$w.vert.ttl.sc   -side left -fill x -expand yes
	frame	$w.vert.but
	button	$w.vert.but.up      -text "Up" \
		-command "VidMode_adjust $win up"
	button	$w.vert.but.down    -text "Down" \
		-command "VidMode_adjust $win down"
	button	$w.vert.but.shorter -text "Shorter" \
		-command "VidMode_adjust $win short"
	button	$w.vert.but.taller  -text "Taller" \
		-command "VidMode_adjust $win tall"
	pack	$w.vert.but.up $w.vert.but.down $w.vert.but.shorter \
		    $w.vert.but.taller -side left
	pack	$w.vert.beg $w.vert.end $w.vert.ttl $w.vert.but \
		    -side top -fill x -expand yes
	frame	$w.other
	frame	$w.flags
	checkbutton $w.flags.interlace	-text "Interlace"  -variable vidmMode(Intlc) \
		    -onvalue "interlace" -offvalue "" -indicatoron false
	checkbutton $w.flags.dblscan	-text "DoubleScan" -variable vidmMode(DblScn) \
		    -onvalue "doublescan" -offvalue "" -indicatoron false
	checkbutton $w.flags.csync	-text "CSync"  -variable vidmMode(CSync) \
		    -onvalue "composite" -offvalue "" -indicatoron false
	checkbutton $w.flags.pcsync	-text "+CSync" -variable vidmMode(CSync) \
		    -onvalue "+csync"    -offvalue "" -indicatoron false
	checkbutton $w.flags.ncsync	-text "-CSync" -variable vidmMode(CSync) \
		    -onvalue "-csync"    -offvalue "" -indicatoron false
	checkbutton $w.flags.phsync	-text "+HSync" -variable vidmMode(HSync) \
		    -onvalue "+hsync"    -offvalue "" -indicatoron false
	checkbutton $w.flags.nhsync	-text "-HSync" -variable vidmMode(HSync) \
		    -onvalue "-hsync"    -offvalue "" -indicatoron false
	checkbutton $w.flags.pvsync	-text "+VSync" -variable vidmMode(VSync) \
		    -onvalue "+vsync"    -offvalue "" -indicatoron false
	checkbutton $w.flags.nvsync	-text "-VSync" -variable vidmMode(VSync) \
		    -onvalue "-vsync"    -offvalue "" -indicatoron false
	pack	$w.flags.interlace $w.flags.dblscan -side left
	pack	$w.flags.csync $w.flags.pcsync $w.flags.ncsync -side left
	pack	$w.flags.phsync $w.flags.nhsync -side left
	pack	$w.flags.pvsync $w.flags.nvsync -side left
	#button	$w.b -text "Okay" -command [list VidMode_deactivate $win];exit
	#catch	{destroy $w.waitmsg}
	frame	$w.buttons
	button	$w.buttons.abort -text "Abort" \
		    -command "set vidmListFlag 1;[list VidMode_list_or_adjust $win]"
	button	$w.buttons.save -text "Save" \
		    -command "set vidmListFlag 1;[list VidMode_list_or_adjust $win]"
	button	$w.buttons.help -text "Help"
	pack	$w.buttons.abort $w.buttons.save $w.buttons.help -side left
	pack	$w.modeline.header -side top
	pack	$w.horz $w.vert -side left -in $w.modeline -fill x -expand yes
}

proc VidMode_activate { win } {
	global vidmCurrentModes

	set w [winpathprefix $win]
	xf86vid_lockmodeswitch lock
	VidMode_list_or_adjust $win
	$w.curr.modes.lb selection clear 0 end
	$w.curr.modes.lb selection set 0
	VidMode_tune $w [lindex $vidmCurrentModes 0]
}

proc VidMode_deactivate { win } {
	xf86vid_lockmodeswitch unlock
}

proc VidMode_list_or_adjust { win } {
	global vidmListFlag

	set w [winpathprefix $win]

	pack forget $w.curr $w.known $w.modeline $w.flags $w.buttons
	if $vidmListFlag {
	    pack	$w.curr   -side top -fill x -expand yes
	    pack	$w.known  -side top -fill x -expand yes
	} else {
	    pack	$w.modeline -side top -fill x -expand yes
	    #pack	$w.flags $w.b -side top
	    pack	$w.flags -side top
	    pack	$w.buttons -side top
	}
}

proc VidMode_add { win } {
	global vidmCurrentModes vidmValidModes

	set w [winpathprefix $win]
	set newmode [$w.known.modes.lb curselection]
	if { [llength $newmode] != 1 } return
	set curmode [$w.curr.modes.lb curselection]
	if { [llength $curmode] != 1 } return
	incr curmode
	if { $curmode >= [llength $vidmCurrentModes] } {
		set curmode end
	}
	puts stderr "Adding [lindex $vidmValidModes $newmode]"
	set retval [catch {xf86vid_addmodeline [lindex $vidmValidModes $newmode]} retmsg]
	puts stderr "Added - $retval - $retmsg"
	if !$retval {
	    set vidmCurrentModes [linsert $vidmCurrentModes \
		$curmode [lindex $vidmValidModes $newmode]]
	    $w.curr.modes.lb insert $curmode [$w.known.modes.lb get $newmode]
	}
}

proc VidMode_new { win } {
}

proc VidMode_next { win } {
	global vidmCurrentModes

	set w [winpathprefix $win]
	set curmode [$w.curr.modes.lb curselection]
	incr curmode
	if { $curmode >= [llength $vidmCurrentModes] } {
		set curmode 0
	}
	$w.curr.modes.lb selection clear 0 end
	$w.curr.modes.lb selection set $curmode
	xf86vid_lockmodeswitch unlock
	xf86vid_switchmode next
	xf86vid_lockmodeswitch lock
	VidMode_tune $w [lindex $vidmCurrentModes $curmode]
}

proc VidMode_prev { win } {
	global vidmCurrentModes

	set w [winpathprefix $win]
	set curmode [$w.curr.modes.lb curselection]
	incr curmode -1
	if { $curmode < 0 } {
		set curmode [expr [llength $vidmCurrentModes]-1]
	}
	$w.curr.modes.lb selection clear 0 end
	$w.curr.modes.lb selection set $curmode
	xf86vid_lockmodeswitch unlock
	xf86vid_switchmode prev
	xf86vid_lockmodeswitch lock
	VidMode_tune $w [lindex $vidmCurrentModes $curmode]
}

proc VidMode_move2top { win } {
	global vidmCurrentModes vidmValidModes

	set w [winpathprefix $win]
}

proc VidMode_delete { win } {
	global vidmCurrentModes vidmValidModes

	set w [winpathprefix $win]
	set curmode [$w.curr.modes.lb curselection]
	if { [llength $curmode] != 1 } return
	puts stderr "Deleting [lindex $vidmCurrentModes $curmode]"
	set retval [catch {xf86vid_deletemodeline [lindex $vidmCurrentModes $curmode]} retmsg]
	puts stderr "deleted - $retval - $retmsg"
	set vidmCurrentModes [lreplace $vidmCurrentModes $curmode $curmode]
	$w.curr.modes.lb delete $curmode
}

proc VidMode_adjust { win dir } {
	global vidmMode

	array set savemode [array get vidmMode]
	set w [winpathprefix $win]
	switch $dir {
	    up		{
	    		  incr vidmMode(VBegin) 4
	    		  incr vidmMode(VEnd) 4
			}
	    down	{
	    		  incr vidmMode(VBegin) -4
	    		  incr vidmMode(VEnd) -4
			}
	    short	{
	    		  incr vidmMode(VTotal) 4
	    		  incr vidmMode(VBegin) 2
	    		  incr vidmMode(VEnd) 2
			  # Update Sync rates
			}
	    tall	{
	    		  incr vidmMode(VTotal) -4
	    		  incr vidmMode(VBegin) -2
	    		  incr vidmMode(VEnd) -2
			  # Update Sync rates
			}
	    right	{
	    		  incr vidmMode(HBegin) -8
	    		  incr vidmMode(HEnd) -8
			}
	    left	{
	    		  incr vidmMode(HBegin) 8
	    		  incr vidmMode(HEnd) 8
			}
	    narrow	{
	    		  incr vidmMode(HTotal) 8
	    		  incr vidmMode(HBegin) 4
	    		  incr vidmMode(HEnd) 4
			  # Update Sync rates
			}
	    wider	{
	    		  incr vidmMode(HTotal) -8
	    		  incr vidmMode(HBegin) -4
	    		  incr vidmMode(HEnd) -4
			  # Update Sync rates
			}
	}
	set flags "$vidmMode(Intlc) $vidmMode(DblScn) $vidmMode(CSync) \
		    $vidmMode(HSync) $vidmMode(VSync)"
	if { ![string length [string trim $flags]] } {
	    set retval [catch {xf86vid_modifymodeline [list $vidmMode(Clock) \
		$vidmMode(HDisp) $vidmMode(HBegin) $vidmMode(HEnd) $vidmMode(HTotal) \
		$vidmMode(VDisp) $vidmMode(VBegin) $vidmMode(VEnd) $vidmMode(VTotal) \
		] } out ]
	} else {
	    puts "Flags:$flags:"
	    set retval [catch {xf86vid_modifymodeline [concat \
	    	[list $vidmMode(Clock) \
		    $vidmMode(HDisp) $vidmMode(HBegin) $vidmMode(HEnd) $vidmMode(HTotal) \
		    $vidmMode(VDisp) $vidmMode(VBegin) $vidmMode(VEnd) $vidmMode(VTotal) ] \
		$flags] } out ]
	}
	puts "$retval:$out"
	if $retval {
	    array set vidmMode [array get savemode]
	}
}

proc VidMode_tune { win modeline } {
	global vidmMode

	set w [winpathprefix $win]
	$w.modeline.header configure -text "Adjusting mode:\
		[VidMode_describe $modeline]"
	puts stderr "modeline='$modeline'"
	set vidmMode(Clock)	[lindex $modeline 0]
	set vidmMode(HDisp)	[lindex $modeline 1]
	set max [expr int($vidmMode(HDisp)*1.5+0.5)]
	foreach scale [list beg end ttl] {
	    $w.horz.$scale.sc configure -from $vidmMode(HDisp) -to $max
	}
	set vidmMode(HBegin)	[lindex $modeline 2]
	set vidmMode(HEnd)	[lindex $modeline 3]
	set vidmMode(HTotal)	[lindex $modeline 4]

	set vidmMode(VDisp)	[lindex $modeline 5]
	set max [expr int($vidmMode(VDisp)*1.5+0.5)]
	foreach scale [list beg end ttl] {
	    $w.vert.$scale.sc configure -from $vidmMode(VDisp) -to $max
	}
	set vidmMode(VBegin)	[lindex $modeline 6]
	set vidmMode(VEnd)	[lindex $modeline 7]
	set vidmMode(VTotal)	[lindex $modeline 8]

	set vidmMode(CSync)	[set vidmMode(HSync) [set vidmMode(VSync) ""]]
	set vidmMode(Intlc)	[set vidmMode(DblScn) ""]

	foreach flag [lrange $modeline 9 end] {
		set lflag [string tolower $flag]
		switch -- $lflag {
			interlace	{ set vidmMode(Intlc) interlace }
			doublescan	{ set vidmMode(DblScn) doublescan }
			composite	-
			-csync		-
			+csync		{ set vidmMode(CSync)  $lflag }
			-hsync		-
			+hsync		{ set vidmMode(HSync)  $lflag }
			-vsync		-
			+vsync		{ set vidmMode(VSync)  $lflag  }
		}
	}
}

