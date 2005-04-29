# lobster.tcl --

# The code formerly known as "gtklook" on the Tcl'ers
# wiki.  Most of this code was originally written by Jeremy Collins.

# $Id: lobster.tcl,v 1.3 2004/03/25 16:22:08 davidw Exp $

package provide style::lobster 0.1

namespace eval styles::lobster {
    if { [tk windowingsystem] == "x11" } {
	set size	-12
	set family	Helvetica
	font create LobsterFont      -size $size -family $family

	option add *borderWidth 1 widgetDefault
	option add *activeBorderWidth 1 widgetDefault
	option add *selectBorderWidth 1 widgetDefault
	option add *font LobsterFont widgetDefault

	option add *padX 2 widgetDefault
	option add *padY 4 widgetDefault

	option add *Listbox.background white widgetDefault
	option add *Listbox.selectBorderWidth 0 widgetDefault
	option add *Listbox.selectForeground white widgetDefault
	option add *Listbox.selectBackground #4a6984 widgetDefault

	option add *Entry.background white widgetDefault
	option add *Entry.foreground black widgetDefault
	option add *Entry.selectBorderWidth 0 widgetDefault
	option add *Entry.selectForeground white widgetDefault
	option add *Entry.selectBackground #4a6984 widgetDefault

	option add *Text.background white widgetDefault
	option add *Text.selectBorderWidth 0 widgetDefault
	option add *Text.selectForeground white widgetDefault
	option add *Text.selectBackground #4a6984 widgetDefault

	option add *Menu.activeBackground #4a6984 widgetDefault
	option add *Menu.activeForeground white widgetDefault
	option add *Menu.activeBorderWidth 0 widgetDefault
	option add *Menu.highlightThickness 0 widgetDefault
	option add *Menu.borderWidth 2 widgetDefault

	option add *Menubutton.activeBackground #4a6984 widgetDefault
	option add *Menubutton.activeForeground white widgetDefault
	option add *Menubutton.activeBorderWidth 0 widgetDefault
	option add *Menubutton.highlightThickness 0 widgetDefault
	option add *Menubutton.borderWidth 0 widgetDefault

	option add *Labelframe.borderWidth 2 widgetDefault
	option add *Frame.borderWidth 2 widgetDefault
	option add *Labelframe.padY 8 widgetDefault
	option add *Labelframe.padX 12 widgetDefault

	option add *highlightThickness 0 widgetDefault
	option add *troughColor #c3c3c3 widgetDefault

	option add *Scrollbar.width		12 widgetDefault
	option add *Scrollbar.borderWidth		1 widgetDefault
	option add *Scrollbar.highlightThickness	0 widgetDefault
    }
}