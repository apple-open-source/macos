# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/monitor.tcl,v 3.13 1999/04/05 07:13:00 dawes Exp $
#
# Copyright 1996 by Joseph V. Moss <joe@XFree86.Org>
#
# See the file "LICENSE" for information regarding redistribution terms,
# and for a DISCLAIMER OF ALL WARRANTIES.
#
# $XConsortium: monitor.tcl /main/2 1996/10/25 10:21:20 kaleb $

#
# Monitor configuration routines
#

proc Monitor_create_widgets { win } {
	global MonitorIDs monDevNum monCanvas MonitorDescriptions
	global pc98_EGC messages

	set w [winpathprefix $win]
	set monDevNum 0
        if !$pc98_EGC {
	    frame $w.monitor -width 640 -height 420 \
		    -relief ridge -borderwidth 5
	} else {
	    frame $w.monitor -width 640 -height 400 \
		    -relief ridge -borderwidth 5
	}
	frame $w.monitor.sync
	pack  $w.monitor.sync -side top -pady 1m

	frame $w.monitor.sync.line1
	pack  $w.monitor.sync.line1 -side top -fill x -expand yes
	label $w.monitor.sync.title -text $messages(monitor.1)
	pack  $w.monitor.sync.title -side left -fill x \
		-in $w.monitor.sync.line1 -expand yes
	if { [llength $MonitorIDs] > 1 } {
		label $w.monitor.sync.monsel \
			-text $messages(monitor.2) -anchor w
		pack  $w.monitor.sync.monsel -side left \
			-in $w.monitor.sync.line1
		combobox $w.monitor.sync.monselect -state disabled -bd 2
		pack  $w.monitor.sync.monselect -side left \
			-in $w.monitor.sync.line1
		eval [list $w.monitor.sync.monselect linsert end] $MonitorIDs
		Monitor_cbox_setentry $w.monitor.sync.monselect [lindex $MonitorIDs 0]
		bind $w.monitor.sync.monselect.popup.list <ButtonRelease-1> \
			"+Monitor_monselect $win"
		bind $w.monitor.sync.monselect.popup.list <Return> \
			"+Monitor_monselect $win"
	}
	frame $w.monitor.sync.horz
	pack  $w.monitor.sync.horz -side left -padx 10m
	label $w.monitor.sync.horz.title -text $messages(monitor.3)
	entry $w.monitor.sync.horz.entry -width 35 -bd 2
	pack  $w.monitor.sync.horz.title -side left
	pack  $w.monitor.sync.horz.entry -side left

	frame $w.monitor.sync.vert
	pack  $w.monitor.sync.vert -side left -padx 10m
	label $w.monitor.sync.vert.title -text $messages(monitor.4)
	entry $w.monitor.sync.vert.entry -width 35 -bd 2
	pack  $w.monitor.sync.vert.title -side left
	pack  $w.monitor.sync.vert.entry -side left 

	set canv $w.monitor.canvas
	set monCanvas $canv
	if !$pc98_EGC {
	    canvas $canv -width 600 -height 330 -highlightthickness 0 \
		    -takefocus 0 -relief sunken -borderwidth 2
	} else {
	    canvas $canv -width 600 -height 250 -highlightthickness 0 \
		    -takefocus 0 -relief sunken -borderwidth 2
	}
	pack $canv -side top -fill x

	frame $canv.list
	listbox $canv.list.lb -height 10 -width 55 -setgrid true \
		-yscroll [list $canv.list.sb set]
	scrollbar $canv.list.sb -command [list $canv.list.lb yview]
	if !$pc98_EGC {
	    pack $canv.list.lb -side left -fill y
	    # pack $canv.list.sb -side left -fill y
	} else {
	    $canv.list.lb configure -height 5
	    pack $canv.list.lb -side left -fill y
	    pack $canv.list.sb -side left -fill y
	}
	eval [list $canv.list.lb insert end] $MonitorDescriptions
	bind $canv.list.lb <ButtonRelease-1> \
		[list Monitor_setstandard $win $canv]
	bind $canv.list.lb <Return> \
		[list Monitor_setstandard $win $canv]

	if !$pc98_EGC {
	    $canv create rectangle 150  55 550 305 -fill cyan
	    $canv create rectangle 160  70 540 280 -fill grey
	    $canv create rectangle 170  80 530 270 -fill blue
	    $canv create arc       170  76 530  84 -fill blue \
		    -start  0 -extent  180 -style chord -outline blue
	    $canv create arc       170 266 530 274 -fill blue \
		    -start  0 -extent -180 -style chord -outline blue
	    $canv create arc       166  80 174 270 -fill blue \
		    -start 90 -extent  180 -style chord -outline blue
	    $canv create arc       526  80 534 270 -fill blue \
		    -start 90 -extent -180 -style chord -outline blue
	    $canv create line      160  70 170  80
	    $canv create line      540  70 530  80
	    $canv create line      540 280 530 270
	    $canv create line      160 280 170 270
	    $canv create rectangle 320 305 380 315 -fill cyan
	    $canv create rectangle 285 315 415 320 -fill cyan
	    
	    $canv create window 350 175 -window $canv.list
	    
	    $canv create rectangle 120 30 570 45 -fill white -tag hsync
	    for {set i 20} {$i<=110} {incr i 10} {
		$canv create text [expr $i*5+20] 22 -text $i
	    }
	    
	    $canv create rectangle 50 30 65 305 -fill white -tag vsync
	    for {set i 40} {$i<=150} {incr i 10} {
		$canv create text 35 [expr $i*2.5-70] -text $i -anchor e
	    }
	} else {
	    $canv create rectangle 150  55 550 225 -fill cyan
	    $canv create rectangle 160  70 540 200 -fill grey
	    $canv create rectangle 170  80 530 190 -fill blue
	    $canv create arc       170  76 530  84 -fill blue \
		    -start  0 -extent  180 -style chord -outline blue
	    $canv create arc       170 186 530 194 -fill blue \
		    -start  0 -extent -180 -style chord -outline blue
	    $canv create arc       166  80 174 190 -fill blue \
		    -start 90 -extent  180 -style chord -outline blue
	    $canv create arc       526  80 534 190 -fill blue \
		    -start 90 -extent -180 -style chord -outline blue
	    $canv create line      160  70 170  80
	    $canv create line      540  70 530  80
	    $canv create line      540 200 530 190
	    $canv create line      160 200 170 190
	    $canv create rectangle 320 225 380 235 -fill cyan
	    $canv create rectangle 285 235 415 240 -fill cyan
	    
	    $canv create window 350 135 -window $canv.list
	    
	    $canv create rectangle 120 30 570 45 -fill white -tag hsync
	    for {set i 20} {$i<=110} {incr i 10} {
		$canv create text [expr $i*5+20] 22 -text $i
	    }
	    
	    $canv create rectangle 50 30 65 225 -fill white -tag vsync
	    for {set i 40} {$i<=150} {incr i 10} {
		$canv create text 35 [expr $i*1.7-38] -text $i -anchor e
	    }
	}
	
	frame $w.monitor.bot
	label $w.monitor.bot.message -text $messages(monitor.5)
	pack $w.monitor.bot -side top
	pack $w.monitor.bot.message

	$canv bind hsync <ButtonPress-1>   [list Monitor_sync_sel $canv hsync %x %y]
	$canv bind hsync <B1-Motion>       [list Monitor_sync_chg $canv hsync %x %y]
	$canv bind hsync <ButtonRelease-1> [list Monitor_sync_rel $canv hsync %x %y]
	$canv bind hsync <Button-3>        [list Monitor_sync_del $canv hsync %x %y]
	$canv bind vsync <ButtonPress-1>   [list Monitor_sync_sel $canv vsync %x %y]
	$canv bind vsync <B1-Motion>       [list Monitor_sync_chg $canv vsync %x %y]
	$canv bind vsync <ButtonRelease-1> [list Monitor_sync_rel $canv vsync %x %y]
	$canv bind vsync <Button-3>        [list Monitor_sync_del $canv vsync %x %y]
	bind $w.monitor.sync.horz.entry , \
		[list Monitor_sync_ent $w.monitor.sync.horz.entry $canv horz]
	bind $w.monitor.sync.horz.entry <KP_Enter> \
		[list Monitor_sync_ent $w.monitor.sync.horz.entry $canv horz]
	bind $w.monitor.sync.horz.entry <Return> \
		[list Monitor_sync_ent $w.monitor.sync.horz.entry $canv horz]
	bind $w.monitor.sync.vert.entry , \
		[list Monitor_sync_ent $w.monitor.sync.vert.entry $canv vert]
	bind $w.monitor.sync.vert.entry <KP_Enter> \
		[list Monitor_sync_ent $w.monitor.sync.vert.entry $canv vert]
	bind $w.monitor.sync.vert.entry <Return> \
		[list Monitor_sync_ent $w.monitor.sync.vert.entry $canv vert]
}

proc Monitor_activate { win } {
	set w [winpathprefix $win]
	pack $w.monitor -side top -fill both -expand yes

	Monitor_get_configvars $win
}

proc Monitor_deactivate { win } {
	set w [winpathprefix $win]
	pack forget $w.monitor

	Monitor_set_configvars $win
}

proc Monitor_monselect { win } {
	global monDevNum

	set w [winpathprefix $win]
	if { ![string length [$w.monitor.sync.monselect curselection]] } \
		return
	Monitor_set_configvars $win
	set monDevNum [$w.monitor.sync.monselect curselection]
	Monitor_get_configvars $win
}

proc Monitor_set_configvars { win } {
	global monDevNum MonitorIDs

	set w [winpathprefix $win]
	set devid [lindex $MonitorIDs $monDevNum]
	set varname Monitor_$devid
	global $varname
	set ${varname}(HorizSync)	[$w.monitor.sync.horz.entry get]
	set ${varname}(VertRefresh)	[$w.monitor.sync.vert.entry get]
}

proc Monitor_get_configvars { win } {
	global monDevNum MonitorIDs monCanvas

	set w [winpathprefix $win]
	set devid [lindex $MonitorIDs $monDevNum]
	set varname Monitor_$devid
	global $varname
	$w.monitor.sync.horz.entry delete 0 end
	$w.monitor.sync.horz.entry insert 0 [set ${varname}(HorizSync)]
	$w.monitor.sync.vert.entry delete 0 end
	$w.monitor.sync.vert.entry insert 0 [set ${varname}(VertRefresh)]
	Monitor_sync_ent $w.monitor.sync.horz.entry $monCanvas horz
	Monitor_sync_ent $w.monitor.sync.vert.entry $monCanvas vert
}

proc Monitor_cbox_setentry { cb text } {
	$cb econfig -state normal
	$cb edelete 0 end
	if [string length $text] {
		$cb einsert 0 $text
	}
	$cb econfig -state disabled
	set cblist [$cb lget 0 end]
	set idx [lsearch $cblist $text]
	if { $idx != -1 } {
		$cb see $idx
		$cb lselection clear 0 end
		$cb lselection set $idx
		$cb activate $idx
	}
}

proc Monitor_setstandard { win c } {
	global MonitorHsyncRanges MonitorVsyncRanges

	set w [winpathprefix $win]
	set monidx [$c.list.lb curselection]
	if ![string length $monidx] return
	$w.monitor.sync.horz.entry delete 0 end 
	$w.monitor.sync.horz.entry insert end $MonitorHsyncRanges($monidx)
	Monitor_sync_ent $w.monitor.sync.horz.entry $c horz
	$w.monitor.sync.vert.entry delete 0 end 
	$w.monitor.sync.vert.entry insert end $MonitorVsyncRanges($monidx)
	Monitor_sync_ent $w.monitor.sync.vert.entry $c vert
}

proc Monitor_sync_ent { win c dir } {
	global pc98_EGC

	set w [winpathprefix $win]
	if { [string compare $dir horz] == 0 } {
		set min 20.0
		set max 110.0
		set x1 {$beg*5.0+20}
		set y1 30
		set x2 {$end*5.0+20}
		set y2 45
	} else {
		set min 40.0
		set max 150.0
		set x1 50
		set x2 65
		if !$pc98_EGC {
		    set y1 {$beg*2.5-70}
		    set y2 {$end*2.5-70}
		} else {
		    set y1 {$beg*1.7-38}
		    set y2 {$end*1.7-38}
		}
	}
	set rng [zap_white [$w get]]
	set rnglist [split $rng ,]
	set count 0
	catch {$c delete ${dir}rng}
	foreach elem $rnglist {
		set beg [set end 0]
		set elem [zap_white $elem]
		if { [string first - $elem] == -1 } {
			scan $elem %f beg
			set end $beg
		} else {
			scan $elem %f-%f beg end
		}
		if { $beg > $end } {
			set end $beg
		}
		if { $beg < $min } {
			if { $end < $min } continue
			set beg $min
		}
		if { $end > $max } {
			if { $beg > $max } continue
			set end $max
		}
		incr count
		$c create rectangle \
			[expr $x1] [expr $y1] [expr $x2] [expr $y2] \
			-fill red -tag "${dir}$count ${dir}rng"
	}
}

proc Monitor_sync_chg { c t x y } {
}

proc Monitor_sync_del { c t x y } {
}

proc Monitor_sync_rel { c t x y } {
}

proc Monitor_sync_sel { c t x y } {
}

