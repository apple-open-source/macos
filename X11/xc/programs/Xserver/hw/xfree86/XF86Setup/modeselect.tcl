# 
#
#
#
#
# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/modeselect.tcl,v 3.2 1998/04/05 16:15:50 robin Exp $
#
# Copyright 1996 by Joseph V. Moss <joe@XFree86.Org>
#           1997 by Dirk H Hohndel <hohndel@XFree86.Org>
#
# See the file "LICENSE" for information regarding redistribution terms,
# and for a DISCLAIMER OF ALL WARRANTIES.
#

#
# routines to select the modes that the user actually wants to use
#

proc Modeselect_create_widgets { win } {
	global MonitorIDs monDevNum monCanvas MonitorDescriptions
	global pc98_EGC messages

	set w [winpathprefix $win]

	if !$pc98_EGC {
		frame $w.modesel -width 640 -height 420 \
			-relief ridge -borderwidth 5
	} else {
		frame $w.modesel -width 640 -height 400 \
			-relief ridge -borderwidth 5
	}
	frame $w.modesel.top
	frame $w.modesel.mid -relief sunken -borderwidth 3
	frame $w.modesel.bot
	pack $w.modesel.top -side top
	pack $w.modesel.mid -side top -expand yes
	pack $w.modesel.bot -side top

	label $w.modesel.top.title -text "Select the modes you want to use"
	frame $w.modesel.type
	pack $w.modesel.top.title $w.modesel.type -in $w.modesel.top -side top
	set canv [canvas $w.modesel.buttons]

	checkbutton $w.modesel.buttons.m640x480 \
		-text $messages(modeselect.2) \
		-indicatoron no -variable m640x480 \
		-command [list modesel_enable $win " 640x480*"]
	lappend lbuttons $w.modesel.buttons.m640x480
	checkbutton $w.modesel.buttons.m800x600 \
		-text $messages(modeselect.3) \
		-indicatoron no -variable m800x600 \
		-command [list modesel_enable $win " 800x600*"]
	lappend lbuttons $w.modesel.buttons.m800x600
	checkbutton $w.modesel.buttons.m1024x768 \
		-text $messages(modeselect.4) \
		-indicatoron no -variable m1024x768 \
		-command [list modesel_enable $win "1024x768*"]
	lappend lbuttons $w.modesel.buttons.m1024x768
	checkbutton $w.modesel.buttons.m1152x864 \
		-text $messages(modeselect.5) \
		-indicatoron no -variable m1152x864 \
		-command [list modesel_enable $win "1152x864*"]
	lappend lbuttons $w.modesel.buttons.m1152x864
	checkbutton $w.modesel.buttons.m1280x1024 \
		-text $messages(modeselect.6) \
  		-indicatoron no -variable m1280x1024 \
		-command [list modesel_enable $win "1280x1024*"]
	lappend lbuttons $w.modesel.buttons.m1280x1024
	checkbutton $w.modesel.buttons.m1600x1200 \
		-text $messages(modeselect.7) \
		-indicatoron no -variable m1600x1200 \
		-command [list modesel_enable $win "1600x1200*"]
	lappend lbuttons $w.modesel.buttons.m1600x1200
	checkbutton $w.modesel.buttons.m640x400 \
		-text $messages(modeselect.8) \
		-indicatoron no -variable m640x400 \
		-command [list modesel_enable $win " 640x400*"]
	lappend lbuttons $w.modesel.buttons.m640x400
	checkbutton $w.modesel.buttons.m320x200 \
		-text $messages(modeselect.9) \
		-indicatoron no -variable m320x200 \
		-command [list modesel_enable $win " 320x200*"]
	lappend lbuttons $w.modesel.buttons.m320x200
	checkbutton $w.modesel.buttons.m320x240 \
		-text $messages(modeselect.10) \
		-indicatoron no -variable m320x240 \
		-command [list modesel_enable $win " 320x240*"]
	lappend lbuttons $w.modesel.buttons.m320x240
	checkbutton $w.modesel.buttons.m400x300 \
		-text $messages(modeselect.11) \
		-indicatoron no -variable m400x300 \
		-command [list modesel_enable $win " 400x300*"]
	lappend lbuttons $w.modesel.buttons.m400x300
	checkbutton $w.modesel.buttons.m480x300 \
		-text $messages(modeselect.12) \
		-indicatoron no -variable m480x300 \
		-command [list modesel_enable $win " 480x300*"]
	lappend lbuttons $w.modesel.buttons.m480x300
	checkbutton $w.modesel.buttons.m512x384 \
		-text $messages(modeselect.13) \
		-indicatoron no -variable m512x384 \
		-command [list modesel_enable $win " 512x384*"]
	lappend lbuttons $w.modesel.buttons.m512x384
	set ht 0
	set wd 0
	foreach wb $lbuttons {
		set bwd [winfo reqwidth $wb]
		if {$wd < $bwd} {set wd $bwd}
	}
	foreach wb $lbuttons {
		$canv create window \
			[expr ($wd-[winfo reqwidth $wb])/2] $ht \
			-anchor nw -window $wb
		set ht [expr $ht + [winfo reqheight $wb]]
	}
	$canv configure -yscrollcommand [list $w.modesel.sb set] \
		-scrollregion [list 0 0 $wd $ht] \
			-width $wd
	scrollbar $w.modesel.sb \
		-command [list $canv yview]
	pack $canv -in $w.modesel.mid \
		-side left -fill both -expand yes -pady 2m
	pack $w.modesel.sb -in $w.modesel.mid -side right \
		-fill y -expand yes
	frame $w.modesel.dcd
	label $w.modesel.dcd.title -text $messages(modeselect.14)
	radiobutton $w.modesel.8bpp -text $messages(modeselect.15) \
		-indicatoron false -variable ColorDepth \
		-value "depth8" -underline 19 \
		-command [list modesel_color_select $w 8]
	radiobutton $w.modesel.16bpp -text $messages(modeselect.16) \
		-indicatoron false -variable ColorDepth \
		-value "depth16" -underline 19 \
		-command [list modesel_color_select $w 16]
	radiobutton $w.modesel.24bpp -text $messages(modeselect.17) \
		-indicatoron false -variable ColorDepth \
		-value "depth24" -underline 19 \
		-command [list modesel_color_select $w 24]
	radiobutton $w.modesel.32bpp -text $messages(modeselect.18) \
		-indicatoron false -variable ColorDepth \
		-value "depth32" -underline 19 \
		-command [list modesel_color_select $w 32]
	pack $w.modesel.8bpp $w.modesel.16bpp $w.modesel.24bpp \
		$w.modesel.32bpp -side left -fill x -expand yes \
		-in $w.modesel.dcd
	pack $w.modesel.dcd.title $w.modesel.dcd -side left -fill y -expand yes
}

proc Modeselection_activate { win } {
	set w [winpathprefix $win]
	pack $w.modesel -side top -fill both -expand yes
}

proc Modeselection_deactivate { win } {
	set w [winpathprefix $win]
	pack forget $w.modesel

}

proc modesel_color_select { win val } {
    global DefaultColorDepth

    set w [winpathprefix $win]

    set DefaultColorDepth $val
}

proc modesel_enable { win val } {
    global MonitorStdModes SelectedMonitorModes haveSelectedModes

    set w [winpathprefix $win]

    set haveSelectedModes 1

    # we need to handle the fact that these are toggle buttons, so every
    # other time this is selected, the user actually wants to deselect
    # the entries
    # this would be much easier if I have a way to remove pairs from an array...
    if { [array get SelectedMonitorModes $val] != "" } {
#	puts stderr "$val already existed"
	# 
	foreach desc [array names SelectedMonitorModes $val] {
	    if [string match \#removed $SelectedMonitorModes($desc)] {
                # ok, we're enabling again
		set SelectedMonitorModes($desc) $MonitorStdModes($desc)
            } else {
#	        puts stderr "invalidating for $desc"
	        set SelectedMonitorModes($desc) "#removed"
	    }
	}	
    } else {
#	puts stderr "add $val to selected modes"
	foreach desc [array names MonitorStdModes $val] {
	    set modeline $MonitorStdModes($desc)
#	    puts stderr "$desc :: $modeline"
	    set SelectedMonitorModes($desc) $modeline
	}
    }
#    foreach desc [array names SelectedMonitorModes] {
#	puts stderr "$desc -> $SelectedMonitorModes($desc)"
#    }
}

