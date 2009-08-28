#!/bin/sh
# the next line restarts using wish \
exec wish "$0" "$@"

# Copyright (c) 2005-2007 Keith Nash.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

### This demo explores ntext indentation
### For a short example, see ntextExample.tcl
### To explore the ntext options, try ntextDemoBindings.tcl

### Points to note when using ntext's indent facilities are commented and numbered (1) to (6).

### If the text in your widget is manipulated only by the keyboard and mouse, then (1), (2) and (3) are all you need to do.  If the text or its layout are manipulated by the script, then you also need to call the function ::ntext::wrapIndent - see comments (4) to (6), and the man page for ntextIndent.

# This string defines the text that will be displayed in each widget:
set message {    This demo shows ntext's indentation facilities.  These are switched off by default, but in this demo they have been switched on.

  To try the demo - place the cursor at the start of a paragraph and change the amount of initial space. The paragraph is a logical line of text; its first display line may have leading whitespace, and ntext indents any subsequent (wrapped) display lines to match the first.
	This paragraph is indented by a tab. Again, the display lines are all indented to match the first.
 Try any text-widget operation, and test whether ntext's handling of display line indentation is satisfactory.  Please report any bugs - for instructions, see the ntext Wiki page, http://wiki.tcl.tk/14918
}
# End of string for widget text.

package require ntext

### (1) Indentation is disabled by default.  Set this variable to 0 to enable it:
set ::ntext::classicWrap        0

#  Activate the traditional "extra" bindings so these can be tested too:
set ::ntext::classicExtras      1

pack [frame .rhf] -side right -anchor nw
pack [text .rhf.new ]

### (2) Set the widget's binding tags to use 'Ntext' instead of the default 'Text':
bindtags .rhf.new {.rhf.new Ntext . all}

### (3) Set the widget to '-wrap word' mode:
.rhf.new configure -wrap word -undo 1
.rhf.new configure -width 42 -height 26 -font {{Courier} -15} -bg white
.rhf.new insert end "  I use the Ntext bindings.\n\n$message"
.rhf.new edit separator

### (4) The script (not the keyboard or mouse) has inserted text.  Because the widget has not yet been drawn, ::ntext::wrapIndent will be called by the <Configure> binding, so it is not really necessary to call it here.  It is necessary in most other cases when the 'insert' command is called by the script.
::ntext::wrapIndent .rhf.new

pack [frame .lhf] -side left -anchor ne
pack [text .lhf.classic ]
.lhf.classic configure -width 42 -height 26 -wrap word -undo 1 -font {{Courier} -15} -bg #FFFFEE
.lhf.classic insert end "  I use the (default) Text bindings.\n\n$message"
.lhf.classic edit separator
pack [label  .lhf.m -text "(The controls do not apply\nto the left-hand text widget)"]

pack [frame .rhf.h] -fill x
### (5) When indentation is switched on or off, call ::ntext::wrapIndent to calculate or clear indentation for the entire widget:
pack [radiobutton .rhf.h.off -text "Indent Off" -variable ::ntext::classicWrap -value 1 -command {::ntext::wrapIndent .rhf.new}] -side right
pack [radiobutton .rhf.h.on  -text "Indent On"  -variable ::ntext::classicWrap -value 0 -command {::ntext::wrapIndent .rhf.new}] -side right
pack [label  .rhf.h.l -text "Switch indentation on/off: "] -side right

pack [frame .rhf.g] -anchor ne
pack [entry  .rhf.g.e -width 3] -side right -padx 5
pack [button .rhf.g.b -text "Click to set tab spacing to value in box" -command changeTabs] -side right

proc changeTabs {} {
    set nTabs [.rhf.g.e get]
    if {[string is integer -strict $nTabs] && $nTabs > 0} {
        set font [lindex [.rhf.new configure -font] 4]
        .rhf.new configure -tabs "[expr {$nTabs * [font measure $font 0]}] left"
        ### (6) Changing the tabs may change the indentation of the first display line of a logical line; if so, the indentation of the other display lines must be recalculated:
        ::ntext::wrapIndent .rhf.new
    }
}
