# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/texts/generic/message_proc.tcl,v 1.2 1998/04/05 15:30:30 robin Exp $
#
# These procedures generate local messages with arguments

proc make_message_phase4 { saveto } {
    global messages
    set messages(phase4.2) \
	    "Unable to backup $saveto as $saveto.bak!\n\
	    The configuration has not been saved!\n\
	    Try again, with a different file name"
    set messages(phase4.3) "A backup of the previous configuration has\n\
	    been saved to the file $saveto.bak"
    set messages(phase4.4) "Unable to save the configuration to\n\
	    the file $saveto.\n\n\
	    Try again, with a different file name"
    set messages(phase4.5) "The configuration has been completed.\n\n"

}
proc make_message_card { args } {
    global pc98 messages Xwinhome
    global cardServer
    
    set mes ""
    if !$pc98 {
	if ![file exists $Xwinhome/bin/XF86_$cardServer] {
	    if ![string compare $args cardselected] {
		set mes \
		        "*** The server required by your card is not\
		        installed!  Please abort, install the\
		        $cardServer server as\n\
		        $Xwinhome/bin/XF86_$cardServer and\
		        run this program again ***"
	    } else {
		set mes \
		        "*** The selected server is not\
		        installed!  Please abort, install the\
		        $cardServer server as\n\
		        $Xwinhome/bin/XF86_$cardServer and\
		        run this program again ***"
	    }
	    bell
	}
    } else {
	if ![file exists $Xwinhome/bin/XF98_$cardServer] {
	    if ![string compare $args cardselected] {
		set mes \
			"*** The server required by your card is not\
			installed!  Please abort, install the\
			$cardServer server as\n\
			$Xwinhome/bin/XF98_$cardServer and\
			run this program again ***"
	    } else {
		set mes \
			"*** The selected server is not\
			installed!  Please abort, install the\
			$cardServer server as\n\
			$Xwinhome/bin/XF98_$cardServer and\
			run this program again ***"
	    }
	    bell
	}
    }
    return $mes
}

proc make_intro_headline { win } {
	global pc98

	$win insert end \
		"Introduction to Configuration with XF86Setup" heading
	if !$pc98 {
	    $win insert end "\n"
	}
}

proc make_underline { win } {
}
