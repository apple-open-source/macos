# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/phase2.tcl,v 3.16 1999/04/05 07:13:00 dawes Exp $
#
# Copyright 1996 by Joseph V. Moss <joe@XFree86.Org>
#
# See the file "LICENSE" for information regarding redistribution terms,
# and for a DISCLAIMER OF ALL WARRANTIES.
#
# $XConsortium: phase2.tcl /main/2 1996/10/25 10:21:29 kaleb $

#
# Phase II - Commands run after connection is made to VGA16 server
#

set_resource_defaults

wm withdraw .

create_main_window [set w .xf86setup]

# put up a message ASAP so the user knows we're still alive
label $w.waitmsg -text $messages(phase2.1)
pack  $w.waitmsg -expand yes -fill both
update idletasks

if $StartServer {
	mesg $messages(phase2.14) info
}

if { [catch {set XKBhandle [xkb_read from_server]} res] } {
	$w.waitmsg configure -text $messages(phase2.2)
	update idletasks
	after 10000
	set XKBavailable 0
	set retval ""
} else {
	set XKBavailable 1
	set retval [xkb_listrules $XKBrules]
}

if { [llength $retval] < 4 } {
	Kbd_setxkbcomponents
} else {
	set XKBComponents(models,names)		 [lindex $retval 0]
	set XKBComponents(models,descriptions)	 [lindex $retval 1]
	set XKBComponents(layouts,names)	 [lindex $retval 2]
	set XKBComponents(layouts,descriptions)	 [lindex $retval 3]
	set XKBComponents(variants,names)	 [lindex $retval 4]
	set XKBComponents(variants,descriptions) [lindex $retval 5]
	set XKBComponents(options,names)	 [lindex $retval 6]
	set XKBComponents(options,descriptions)	 [lindex $retval 7]
}

# Setup the default bindings for the various widgets
source $tcl_library/tk.tcl

source $XF86Setup_library/mseproto.tcl
source $XF86Setup_library/mouse.tcl
source $XF86Setup_library/keyboard.tcl
source $XF86Setup_library/card.tcl
source $XF86Setup_library/monitor.tcl
source $XF86Setup_library/srvflags.tcl
source $XF86Setup_library/done.tcl
source $XF86Setup_library/modeselect.tcl

proc Intro_create_widgets { win } {
	global XF86Setup_library
	global pc98_EGC pc98 messages

	set w [winpathprefix $win]
        if !$pc98_EGC {
	    frame $w.intro -width 640 -height 420 \
		-relief ridge -borderwidth 5
	} else {
	    frame $w.intro -width 640 -height 400 \
		-relief ridge -borderwidth 5
	}
	image create bitmap XFree86-logo \
		-foreground black -background cyan \
		-file $XF86Setup_library/pics/XFree86.xbm \
		-maskfile $XF86Setup_library/pics/XFree86.msk
	label $w.intro.logo -image XFree86-logo
	pack  $w.intro.logo

	frame $w.intro.textframe
	text $w.intro.textframe.text
	$w.intro.textframe.text tag configure heading \
		-justify center -foreground yellow \
		-font -adobe-times-bold-i-normal--24-240-*-*-p-*-iso8859-1
	make_intro_headline $w.intro.textframe.text
	$w.intro.textframe.text insert end $messages(phase2.12)
	if !$pc98_EGC {
	    pack $w.intro.textframe.text -fill both -expand yes
	    pack $w.intro.textframe -fill both -expand yes -padx 10 -pady 10
	} else {
	    scrollbar $w.intro.textframe.scroll \
		    -command "$w.intro.textframe.text yview"
	    bind $w.intro <Prior> \
		    "$w.intro.textframe.text yview scroll -1 unit;break;"
	    bind $w.intro <Next> \
		    "$w.intro.textframe.text yview scroll  1 unit;break;"

	    $w.intro.textframe.text configure \
		    -yscrollcommand "$w.intro.textframe.scroll set"
	    pack $w.intro.textframe.scroll -side right -fill y
	    pack $w.intro.textframe.text -fill both -expand yes -side right
	    pack $w.intro.textframe -fill both  -expand yes -padx 10 -pady 10
	}
	$w.intro.textframe.text configure -state disabled
}

proc Intro_activate { win } {
	set w [winpathprefix $win]
	pack $w.intro -side top -fill both -expand yes
}

proc Intro_deactivate { win } {
	set w [winpathprefix $win]
	pack forget $w.intro
}

proc config_select { win } {
	global CfgSelection prevSelection

	set w [winpathprefix $win]
	$win configure -cursor watch
	${prevSelection}_deactivate $win
	set prevSelection $CfgSelection
	${CfgSelection}_activate $w
	$win configure -cursor top_left_arrow
}

proc config_help { win } {
	global CfgSelection

	set w [winpathprefix $win]
	${CfgSelection}_popup_help $win
}

frame $w.menu -width 640

radiobutton $w.menu.mouse -text $messages(phase2.3) -indicatoron false \
	-variable CfgSelection -value Mouse -underline 0 \
	-command [list config_select $w]
radiobutton $w.menu.keyboard -text $messages(phase2.4) -indicatoron false \
	-variable CfgSelection -value Keyboard -underline 0 \
	-command [list config_select $w]
radiobutton $w.menu.card -text $messages(phase2.5) -indicatoron false \
	-variable CfgSelection -value Card -underline 0 \
	-command [list config_select $w]
radiobutton $w.menu.monitor -text $messages(phase2.6) -indicatoron false \
	-variable CfgSelection -value Monitor -underline 2 \
	-command [list config_select $w]
radiobutton $w.menu.modeselect -text $messages(phase2.7) -indicatoron false \
	-variable CfgSelection -value Modeselection -underline 4 \
	-command [list config_select $w]
radiobutton $w.menu.other -text $messages(phase2.8) -indicatoron false \
	-variable CfgSelection -value Other -underline 0 \
	-command [list config_select $w]
if !$pc98 {
	pack $w.menu.mouse $w.menu.keyboard $w.menu.card \
		$w.menu.monitor $w.menu.modeselect $w.menu.other \
		-side left -fill both -expand yes
} else {
	pack $w.menu.mouse $w.menu.card $w.menu.monitor \
		$w.menu.modeselect $w.menu.other \
		-side left -fill both -expand yes
}

frame $w.buttons
#label $w.buttons.xlogo -bitmap @/usr/X11R6/include/X11/bitmaps/xlogo16 -anchor w
#label $w.buttons.xlogo -bitmap @/usr/tmp/xfset1.xbm -anchor w \
	-foreground black
#pack $w.buttons.xlogo -side left -anchor w -expand no -padx 0 -fill x
button $w.buttons.abort -text $messages(phase2.9) -underline 0 \
	-command "clear_scrn;puts stderr Aborted;shutdown 1"
button $w.buttons.done  -text $messages(phase2.10) -underline 0 \
	-command [list Done_execute $w]
button $w.buttons.help  -text $messages(phase2.11) -underline 0 \
	-command [list config_help $w]
pack   $w.buttons.abort $w.buttons.done $w.buttons.help \
	-expand no -side left -padx 50

Intro_create_widgets	$w
if !$pc98 {
	Keyboard_create_widgets	$w
}
Mouse_create_widgets	$w
Card_create_widgets	$w
Monitor_create_widgets	$w
Modeselect_create_widgets	$w
Other_create_widgets	$w
Done_create_widgets	$w

proc ac_bind { win letter command } {
	bind $win <Alt-$letter>		$command
	bind $win <Control-$letter>	$command
}

bind $w <Alt-x>		[list I'm an error]
ac_bind $w a		[list $w.buttons.abort invoke]
ac_bind $w d		[list $w.buttons.done invoke]
ac_bind $w h		[list config_help $w]
bind $w ?		[list config_help $w]
ac_bind $w m		[list $w.menu.mouse invoke]
ac_bind $w c		[list $w.menu.card invoke]
ac_bind $w k		[list $w.menu.keyboard invoke]
ac_bind $w n		[list $w.menu.monitor invoke]
ac_bind $w s		[list $w.menu.modeselect invoke]
ac_bind $w o		[list $w.menu.other invoke]
set_default_arrow_bindings

set CfgSelection  Intro
set prevSelection Intro
destroy $w.waitmsg
pack $w.menu -side top -fill x
pack $w.buttons -side bottom -expand no
config_select $w
focus $w.menu.mouse

