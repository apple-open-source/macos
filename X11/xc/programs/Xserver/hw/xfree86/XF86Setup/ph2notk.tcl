# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/ph2notk.tcl,v 1.1 1999/04/05 07:13:00 dawes Exp $
#
# Copyright 1997 by Joseph V. Moss <joe@XFree86.Org>
#
# See the file "LICENSE" for information regarding redistribution terms,
# and for a DISCLAIMER OF ALL WARRANTIES.
#

#
# Phase II - Curses/Text server configuration
#

set XKBavailable 0
Kbd_setxkbcomponents
puts $XKBComponents(models,names)
puts $XKBComponents(models,descriptions)
puts $XKBComponents(options,names)
puts $XKBComponents(options,descriptions)
sleep 100

mesg "\n\
	There are five areas of configuration that need to\
		be completed:\n\
	Mouse    - Use this to set the protocol, baud rate, etc.\
		used by your mouse\n\
	Keyboard - Set the nationality and layout of\
		your keyboard\n\
	Card     - Used to select the chipset, RAMDAC, etc.\
		of your card\n\
	Monitor  - Use this to enter your\
		monitor's capabilities\n\
	Other    - Configure some miscellaneous settings\n\n\
	You'll probably want to start with configuring your\
		mouse (you can just press \[Enter\] to do so)\n\
	and when you've finished configuring all five of these,\
		select the Done button.\n\n\
	You can also press ? or click on the Help button at\
		any time for additional instructions\n\n" \
	okay "Introduction to Configuration with XF86Setup"

proc Intro_popup_help { win } {
	.introhelp.text insert 0.0 "\n\
		You need to fill in the requested information on each\
		of the five\n\
		configuration screens.  The buttons along the top allow\
		you to choose which\n\
		screen you are going to work on.  You can do them in\
		any order or go back\n\
		to each of them as many times as you like, however,\
		it will be very\n\
		difficult to use some of them if your mouse is not\
		working, so you\n\
		should configure your mouse first.\n\n\
		Until you get your mouse working, here are some keys you\
		can use:\n\n\
		\ \ Tab, Ctrl-Tab    Move to the \"next\" widget\n\
		\ \ Shift-Tab        Move to the \"previous\" widget\n\
		\ \ <Arrow keys>     Move in the appropriate direction\n\
		\ \ Return           Activate the selected widget\n\n\
		Also, you can press Alt and one of the underlined letters\
		to activate the\n\
		corresponding button."
	.introhelp.text configure -state disabled
	button .introhelp.ok -text "Dismiss" -command "destroy .introhelp"
}


