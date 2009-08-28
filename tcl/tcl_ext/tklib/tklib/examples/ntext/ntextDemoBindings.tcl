#!/bin/sh
# the next line restarts using wish \
exec wish "$0" "$@"

# Copyright (c) 2005-2007 Keith Nash.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

### This demo explores the ntext options
### For a short example, see ntextExample.tcl
### To explore ntext indentation, try ntextDemoIndent.tcl

# This string defines the text that will be displayed in each widget:
set message {QOTW:  "C/C++, which is used by 16% of users, is the most popular programming language, but Tcl, used by 0%, seems to be the language of choice for the highest scoring users."

example code {alph {bet {b}} {gam {c}}} {
    # Example code rich in punctuation
    if {!($alph eq "a" && $bet eq "b")} {
        puts "$gam $::messages::demo(d)"
    }
}

Try editing the text with the keyboard and mouse; compare the bindings for Text (left panel) and Ntext (right panel).

Try word-by-word navigation (Control key with left cursor or right cursor key); try word selection (double click); try these for the different word-break detection options (selected below).

The classicMouseSelect and classicAnchor options are discussed in the man page for ntextBindings.}
# End of string for widget text.

package require ntext

# Whether Shift-Button-1 ignores changes made by the kbd to the insert mark:
set ::ntext::classicMouseSelect 0

# Whether Shift-Button-1 has a variable or fixed anchor:
set ::ntext::classicAnchor      0

# Whether the traditional "extra" bindings are activated:
set ::ntext::classicExtras      1

# Whether to use new or classic word boundary detection:
set ::ntext::classicWordBreak   0

# Set to 0 to align wrapped display lines with the first display line of the logical line:
set ::ntext::classicWrap        1

pack [frame .rhf] -side right -anchor nw
pack [text .rhf.new ]
bindtags .rhf.new {.rhf.new Ntext . all}

.rhf.new configure -wrap word -undo 1
.rhf.new configure -width 42 -height 29 -font {{Courier} -15} -bg white
.rhf.new insert end "  I use the Ntext bindings.\n\n$message"
.rhf.new edit separator

pack [frame .lhf] -side left -anchor ne
pack [text .lhf.classic ]
.lhf.classic configure -width 42 -height 29 -wrap word -undo 1 -font {{Courier} -15} -bg #FFFFEE
.lhf.classic insert end "  I use the (default) Text bindings.\n\n$message"
.lhf.classic edit separator
pack [label  .lhf.m -text "(The controls do not apply\nto the left-hand text widget)"]

pack [frame .rhf.h] -fill x
pack [radiobutton .rhf.h.on  -text "On " -variable ::ntext::classicMouseSelect -value 1] -side right
pack [radiobutton .rhf.h.off -text "Off" -variable ::ntext::classicMouseSelect -value 0] -side right
pack [label  .rhf.h.l -text "classicMouseSelect: "] -side right

pack [frame .rhf.g] -anchor ne
pack [radiobutton .rhf.g.on  -text "On " -variable ::ntext::classicAnchor -value 1] -side right
pack [radiobutton .rhf.g.off -text "Off" -variable ::ntext::classicAnchor -value 0] -side right
pack [label  .rhf.g.l -text "classicAnchor: "] -side right

pack [frame .rhf.k] -anchor ne
pack [radiobutton .rhf.k.on  -text "On " -variable ::ntext::classicExtras -value 1] -side right
pack [radiobutton .rhf.k.off -text "Off" -variable ::ntext::classicExtras -value 0] -side right
pack [label  .rhf.k.l -text "classicExtras: "] -side right

pack [frame .rhf.j] -anchor ne
set wordBreakChoice new
pack [radiobutton .rhf.j.wind -text "On (Windows)" -variable wordBreakChoice -value "windows" -command {setPattern}] -side right
pack [radiobutton .rhf.j.unix -text "On (Unix)" -variable wordBreakChoice -value "unix" -command {setPattern}] -side right
pack [radiobutton .rhf.j.off  -text "Off" -variable wordBreakChoice -value "new" -command {setPattern}] -side right
pack [label  .rhf.j.l -text "classicWordBreak: "] -side right

proc setPattern {} {
    global wordBreakChoice
    set platform $::tcl_platform(platform)

    if {$wordBreakChoice eq "unix"} {
        set ::tcl_platform(platform) unix
        set ::ntext::classicWordBreak 1
    } elseif {$wordBreakChoice eq "windows"} {
        set ::tcl_platform(platform) windows
        set ::ntext::classicWordBreak 1
    } else {
        set ::ntext::classicWordBreak 0
    }

    ::ntext::initializeMatchPatterns
    set ::tcl_platform(platform) $platform
}
