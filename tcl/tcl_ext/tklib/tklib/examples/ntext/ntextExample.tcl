#!/bin/sh
# the next line restarts using tclsh \
exec tclsh "$0" "$@"

package require Tk

# Copyright (c) 2005-2007 Keith Nash.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

### This is a short, simple example.  It shows the difference
### between a default text widget and one that uses ntext.

### To explore the ntext options, try ntextDemoBindings.tcl
### To explore ntext indentation, try ntextDemoIndent.tcl

# This string defines the text that will be displayed in each widget:
set message {QOTW:  "C/C++, which is used by 16% of users, is the most popular programming language, but Tcl, used by 0%, seems to be the language of choice for the highest scoring users."
}
# End of string for widget text.

package require ntext

#  Whether Shift-Button-1 ignores changes made by the kbd to the insert mark:
set ::ntext::classicMouseSelect 0

#  Whether Shift-Button-1 has a variable or fixed anchor:
set ::ntext::classicAnchor      0

# Whether to activate certain traditional "extra" bindings
variable classicExtras            1

#  Whether to use new or classic word boundary detection:
set ::ntext::classicWordBreak   0

pack [text .right ] -side right
.right configure -width 28 -height 12 -wrap word -font {{Courier} -15} -bg white
.right insert end "  I use the Ntext bindings.\n\n$message"

bindtags .right {.right Ntext . all}

pack [text .left ] -side right
.left configure -width 28 -height 12 -wrap word -font {{Courier} -15} -bg #FFFFEE
.left insert end "  I use the (default) Text bindings.\n\n$message"
