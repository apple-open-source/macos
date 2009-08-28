# lobster.tcl --

# The code formerly known as "gtklook" on the Tcl'ers
# wiki.  Most of this code was originally written by Jeremy Collins.

# $Id: lobster.tcl,v 1.7 2005/05/18 16:19:53 andreas_kupries Exp $

package require Tk

namespace eval style::lobster {
    # This may need to be adjusted for some window managers that are
    # more aggressive with their own Xdefaults (like KDE and CDE)
    variable prio "widgetDefault"
}

proc style::lobster::init {args} {
    package require Tk
    variable prio

    if {[llength $args]} {
	set arg [lindex $args 0]
	set len [string length $arg]
	if {$len > 2 && [string equal -len $len $arg "-priority"]} {
	    set prio [lindex $args 1]
	    set args [lrange $args 2 end]
	}
    }

    if {[string equal [tk windowingsystem] "x11"]} {
	set size   -12
	set family Helvetica
	font create LobsterFont -size $size -family $family
	font create LobsterBold -size $size -family $family -weight bold

	option add *borderWidth			1 $prio
	option add *activeBorderWidth		1 $prio
	option add *selectBorderWidth		1 $prio
	option add *font			LobsterFont $prio

	option add *padX			2 $prio
	option add *padY			4 $prio

	option add *Listbox.background		white $prio
	option add *Listbox.selectBorderWidth	0 $prio
	option add *Listbox.selectForeground	white $prio
	option add *Listbox.selectBackground	#4a6984 $prio

	option add *Entry.background		white $prio
	option add *Entry.foreground		black $prio
	option add *Entry.selectBorderWidth	0 $prio
	option add *Entry.selectForeground	white $prio
	option add *Entry.selectBackground	#4a6984 $prio

	option add *Text.background		white $prio
	option add *Text.selectBorderWidth	0 $prio
	option add *Text.selectForeground	white $prio
	option add *Text.selectBackground	#4a6984 $prio

	option add *Menu.activeBackground	#4a6984 $prio
	option add *Menu.activeForeground	white $prio
	option add *Menu.activeBorderWidth	0 $prio
	option add *Menu.highlightThickness	0 $prio
	option add *Menu.borderWidth		2 $prio

	option add *Menubutton.activeBackground	#4a6984 $prio
	option add *Menubutton.activeForeground	white $prio
	option add *Menubutton.activeBorderWidth 0 $prio
	option add *Menubutton.highlightThickness 0 $prio
	option add *Menubutton.borderWidth	0 $prio

	option add *Labelframe.borderWidth	2 $prio
	option add *Frame.borderWidth		2 $prio
	option add *Labelframe.padY		8 $prio
	option add *Labelframe.padX		12 $prio

	option add *highlightThickness		0 $prio
	option add *troughColor			#c3c3c3 $prio

	option add *Scrollbar.width		12 $prio
	option add *Scrollbar.borderWidth	1 $prio
	option add *Scrollbar.highlightThickness 0 $prio

	# These don't seem to take effect without the startupFile
	# level specified.
	option add *Dialog.msg.font LobsterBold startupFile
	option add *Dialog.dtl.font LobsterBold startupFile
    }
}

package provide style::lobster 0.2
