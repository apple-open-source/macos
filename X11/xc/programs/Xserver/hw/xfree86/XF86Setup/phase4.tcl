# $XConsortium: phase4.tcl /main/1 1996/09/21 14:17:41 kaleb $
#
#
#
#
# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/phase4.tcl,v 3.11 1999/04/05 07:13:01 dawes Exp $
#
# Copyright 1996 by Joseph V. Moss <joe@XFree86.Org>
#
# See the file "LICENSE" for information regarding redistribution terms,
# and for a DISCLAIMER OF ALL WARRANTIES.
#

#
# Phase IV - Commands run after second server is started
#

source $XF86Setup_library/texts/local_text.tcl
source $XF86Setup_library/vidmode.tcl

if $StartServer {
	set_resource_defaults

	wm withdraw .

	create_main_window [set w .xf86setup]

	# put up a message ASAP so the user knows we're still alive
	label $w.waitmsg -text $messages(phase4.1)
	pack  $w.waitmsg -expand yes -fill both
	update idletasks

	mesg "The program is running on a different virtual terminal\n\n\
		Please switch to the correct virtual terminal" info

	source $tk_library/tk.tcl
	set_default_arrow_bindings
	set msg $messages(phase4.13)
} else {
	set msg ""
}

proc Phase4_adj_modes { win } {
	global Xwinhome

	VidMode_activate $win
}

proc Phase4_nextphase { win } {
	global Confname StartServer messages

	set w [winpathprefix $win]
	set saveto [$w.saveto.entry get]
	check_tmpdirs
	writeXF86Config $Confname-3 -displayof $w -realdevice
	set backupmsg ""
	make_message_phase4 $saveto
	if [file exists $saveto] {
	    if {[catch {exec mv $saveto $saveto.bak} ret] != 0} {
		bell
		$w.mesg configure -text $messages(phase4.2)
		return
	    }
	    set backupmsg $messages(phase4.3)
	}
	if {[catch {exec cp $Confname-3 $saveto} ret] != 0} {
	    bell
            $w.mesg configure -text $messages(phase4.4)
	    return
	}
	$w.text configure -text $messages(phase4.5)
	pack forget $w.buttons $w.mesg $w.saveto
	if $StartServer {
		set cmd {mesg $messages(phase4.14) info; shutdown}
	} else {
		set cmd {shutdown;source $XF86Setup_library/phase5.tcl}
	}
	button $w.okay -text $messages(phase4.6)  -command $cmd
	pack   $w.text $w.okay -side top
	focus  $w.okay
}

label  $w.text -text " $msg$messages(phase4.7)"
frame  $w.saveto
label  $w.saveto.title -text $messages(phase4.8)
entry  $w.saveto.entry -bd 2 -width 40
pack   $w.saveto.title $w.saveto.entry -side left
if [getuid] {
	if [info exists env(HOME)] {
		$w.saveto.entry insert end $env(HOME)/XF86Config
	} else {
		$w.saveto.entry insert end /tmp/XF86Config.$PID
	}
} else {
	$w.saveto.entry insert end $ConfigFile
}
label  $w.mesg -text ""
frame  $w.buttons
button $w.buttons.xvidtune -text $messages(phase4.9) \
	-command [list Phase4_adj_modes $w]
button $w.buttons.save -text $messages(phase4.10) \
	-command [list Phase4_nextphase $w]
button $w.buttons.abort -text $messages(phase4.11) \
	-command "clear_scrn;puts stderr $messages(phase4.12);shutdown 1"
pack   $w.buttons.xvidtune $w.buttons.save $w.buttons.abort -side top \
	-pady 5m -fill x

catch {destroy $w.waitmsg}
pack    $w.text $w.saveto $w.buttons $w.mesg -side top -pady 8m
focus   $w.buttons.save

