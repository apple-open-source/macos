# ntext.tcl --
# derived from text.tcl
#
# This file defines the Ntext bindings for Tk text widgets and provides
# procedures that help in implementing the bindings.
#
# $Id: ntext.tcl,v 1.1 2007/06/21 21:05:27 hobbs Exp $
#
# Copyright (c) 1992-1994 The Regents of the University of California.
# Copyright (c) 1994-1997 Sun Microsystems, Inc.
# Copyright (c) 1998 by Scriptics Corporation.
# Copyright (c) 2005-2007 additions by Keith Nash.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

##### START OF CODE THAT IS MODIFIED text.tcl, Tk 8.5a5 = ActiveTcl 8.5beta6

#-------------------------------------------------------------------------
# Elements of ::tk::Priv that are used in this file:
#
# afterId -		If non-null, it means that auto-scanning is underway
#			and it gives the "after" id for the next auto-scan
#			command to be executed.
# char -		Character position on the line;  kept in order
#			to allow moving up or down past short lines while
#			still remembering the desired position.
# mouseMoved -		Non-zero means the mouse has moved a significant
#			amount since the button went down (so, for example,
#			start dragging out a selection).
# prevPos -		Used when moving up or down lines via the keyboard.
#			Keeps track of the previous insert position, so
#			we can distinguish a series of ups and downs, all
#			in a row, from a new up or down.
# selectMode -		The style of selection currently underway:
#			char, word, or line.
# x, y -		Last known mouse coordinates for scanning
#			and auto-scanning.
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# The code below creates the Ntext class bindings for text widgets.
#-------------------------------------------------------------------------

package require Tk 8.5

# Mouse bindings: use ::ntext::Bcount to deal with out-of-order multiple
# clicks. This permits the bindings to be simplified

bind Ntext <1> {
    set ::ntext::Bcount 1
    ntext::TextButton1 %W %x %y
    %W tag remove sel 0.0 end
}
bind Ntext <B1-Motion> {
    set tk::Priv(x) %x
    set tk::Priv(y) %y
    ntext::TextSelectTo %W %x %y
}
# Inside the if:
#   The previous Button-1 event was not a single-click, but a double, triple,
#   or quadruple.
#   We can simplify the bindings if we ensure that a double-click is
#   *always* preceded by a single-click.
#   So in this case run the same code as <1> before doing <Double-1>
bind Ntext <Double-1> {
    if {$::ntext::Bcount != 1} {
	set ::ntext::Bcount 1
	ntext::TextButton1 %W %x %y
	%W tag remove sel 0.0 end
    }
    set ::ntext::Bcount 2
    set tk::Priv(selectMode) word
    ntext::TextSelectTo %W %x %y
    catch {%W mark set insert sel.first}
}
# ignore an out-of-order triple click.  This has no adverse consequences.
bind Ntext <Triple-1> {
    if {$::ntext::Bcount != 2} {
	continue
    }
    set ::ntext::Bcount 3
    set tk::Priv(selectMode) line
    ntext::TextSelectTo %W %x %y
    catch {%W mark set insert sel.first}
}
# don't care if a quadruple click is out-of-order (i.e. follows a quadruple
# click, not a triple click).
bind Ntext <Quadruple-1> {
    set ::ntext::Bcount 4
}
bind Ntext <Shift-1> {
    set ::ntext::Bcount 1
    if {(!$::ntext::classicMouseSelect) && ([%W tag ranges sel] eq "")} {
	# Move the selection anchor mark to the old insert mark
	# Should the mark's gravity be set?
	%W mark set tk::anchor%W insert
    }
    if {$::ntext::classicAnchor} {
	tk::TextResetAnchor %W @%x,%y
	# if sel exists, sets anchor to end furthest from x,y
	# changes anchor only, not insert
    }
    set tk::Priv(selectMode) char
    ntext::TextSelectTo %W %x %y
}
# Inside the outer if:
#   The previous Button-1 event was not a single-click, but a double, triple,
#   or quadruple.
#   We can simplify the bindings if we ensure that a double-click is
#   *always* preceded by a single-click.
#   So in this case run the same code as <Shift-1> before doing <Double-Shift-1>
bind Ntext <Double-Shift-1>	{
    if {$::ntext::Bcount != 1} {
	set ::ntext::Bcount 1
	if {(!$::ntext::classicMouseSelect) && ([%W tag ranges sel] eq "")} {
	    # Move the selection anchor mark to the old insert mark
	    # Should the mark's gravity be set?
	    %W mark set tk::anchor%W insert
	}
	if {$::ntext::classicAnchor} {
	    tk::TextResetAnchor %W @%x,%y
	    # if sel exists, sets anchor to end furthest from x,y
	    # changes anchor only, not insert
	}
	set tk::Priv(selectMode) char
	ntext::TextSelectTo %W %x %y
    }
    set ::ntext::Bcount 2
    set tk::Priv(selectMode) word
    ntext::TextSelectTo %W %x %y 1
}
# ignore an out-of-order triple click.  This has no adverse consequences.
bind Ntext <Triple-Shift-1>	{
    if {$::ntext::Bcount != 2} {
	continue
    }
    set ::ntext::Bcount 3
    set tk::Priv(selectMode) line
    ntext::TextSelectTo %W %x %y
}
# don't care if a quadruple click is out-of-order (i.e. follows a quadruple
# click, not a triple click).
bind Ntext <Quadruple-Shift-1> {
    set ::ntext::Bcount 4
}
bind Ntext <B1-Leave> {
    set tk::Priv(x) %x
    set tk::Priv(y) %y
    ntext::TextAutoScan %W
}
bind Ntext <B1-Enter> {
    tk::CancelRepeat
}
bind Ntext <ButtonRelease-1> {
    tk::CancelRepeat
}
bind Ntext <Control-1> {
    %W mark set insert @%x,%y
    if {[%W cget -autoseparators]} {
	%W edit separator
    }
}
bind Ntext <Double-Control-1> { # nothing }
bind Ntext <Control-B1-Motion> { # nothing }
bind Ntext <Left> {
    tk::TextSetCursor %W insert-1displayindices
}
bind Ntext <Right> {
    tk::TextSetCursor %W insert+1displayindices
}
bind Ntext <Up> {
    tk::TextSetCursor %W [tk::TextUpDownLine %W -1]
}
bind Ntext <Down> {
    tk::TextSetCursor %W [tk::TextUpDownLine %W 1]
}
bind Ntext <Shift-Left> {
    tk::TextKeySelect %W [%W index {insert - 1displayindices}]
}
bind Ntext <Shift-Right> {
    tk::TextKeySelect %W [%W index {insert + 1displayindices}]
}
bind Ntext <Shift-Up> {
    tk::TextKeySelect %W [tk::TextUpDownLine %W -1]
}
bind Ntext <Shift-Down> {
    tk::TextKeySelect %W [tk::TextUpDownLine %W 1]
}
bind Ntext <Control-Left> {
    tk::TextSetCursor %W \
	[tk::TextPrevPos %W insert ntext::new_startOfPreviousWord]
}
bind Ntext <Control-Right> {
    tk::TextSetCursor %W [ntext::TextNextWord %W insert]
}
bind Ntext <Control-Up> {
    tk::TextSetCursor %W [tk::TextPrevPara %W insert]
}
bind Ntext <Control-Down> {
    tk::TextSetCursor %W [tk::TextNextPara %W insert]
}
bind Ntext <Shift-Control-Left> {
    tk::TextKeySelect %W \
	[tk::TextPrevPos %W insert ntext::new_startOfPreviousWord]
}
bind Ntext <Shift-Control-Right> {
    tk::TextKeySelect %W [ntext::TextNextWord %W insert]
}
bind Ntext <Shift-Control-Up> {
    tk::TextKeySelect %W [tk::TextPrevPara %W insert]
}
bind Ntext <Shift-Control-Down> {
    tk::TextKeySelect %W [tk::TextNextPara %W insert]
}
bind Ntext <Prior> {
    tk::TextSetCursor %W [ntext::TextScrollPages %W -1 preScroll]
}
bind Ntext <Shift-Prior> {
    tk::TextKeySelect %W [ntext::TextScrollPages %W -1 preScroll]
}
bind Ntext <Next> {
    tk::TextSetCursor %W [ntext::TextScrollPages %W 1 preScroll]
}
bind Ntext <Shift-Next> {
    tk::TextKeySelect %W [ntext::TextScrollPages %W 1 preScroll]
}
bind Ntext <Control-Prior> {
    %W xview scroll -1 page
}
bind Ntext <Control-Next> {
    %W xview scroll 1 page
}

bind Ntext <Home> {
    tk::TextSetCursor %W  [::ntext::HomeIndex %W insert]
}
bind Ntext <Shift-Home> {
    tk::TextKeySelect %W [::ntext::HomeIndex %W insert]
}
bind Ntext <End> {
    tk::TextSetCursor %W  [::ntext::EndIndex %W insert]
}
bind Ntext <Shift-End> {
    tk::TextKeySelect %W [::ntext::EndIndex %W insert]
}
bind Ntext <Control-Home> {
    tk::TextSetCursor %W 1.0
}
bind Ntext <Control-Shift-Home> {
    tk::TextKeySelect %W 1.0
}
bind Ntext <Control-End> {
    tk::TextSetCursor %W {end - 1 indices}
}
bind Ntext <Control-Shift-End> {
    tk::TextKeySelect %W {end - 1 indices}
}

bind Ntext <Tab> {
    if {[%W cget -state] eq "normal"} {
	ntext::TextInsert %W \t
	focus %W
	break
    }
}
bind Ntext <Shift-Tab> {
    # Needed only to keep <Tab> binding from triggering;  doesn't
    # have to actually do anything.
    break
}
bind Ntext <Control-Tab> {
    focus [tk_focusNext %W]
}
bind Ntext <Control-Shift-Tab> {
    focus [tk_focusPrev %W]
}
bind Ntext <Control-i> {
    if {$::ntext::classicExtras} {
	ntext::TextInsert %W \t
    }
}
bind Ntext <Return> {
    ntext::TextInsert %W \n
    if {[%W cget -autoseparators]} {
	%W edit separator
    }
}
bind Ntext <Delete> {
    if {[%W tag nextrange sel 1.0 end] ne ""} {
	set ::ntext::OldFirst [%W index sel.first]
	%W delete sel.first sel.last
	ntext::AdjustIndentOneLine %W $::ntext::OldFirst
    } else {
	%W delete insert
	ntext::AdjustIndentOneLine %W insert
	%W see insert
    }
}
bind Ntext <BackSpace> {
    if {[%W tag nextrange sel 1.0 end] ne ""} {
	set ::ntext::OldFirst [%W index sel.first]
	%W delete sel.first sel.last
	ntext::AdjustIndentOneLine %W $::ntext::OldFirst
    } elseif {[%W compare insert != 1.0]} {
	%W delete insert-1c
	ntext::AdjustIndentOneLine %W insert
	%W see insert
    }
}

bind Ntext <Control-space> {
    if {$::ntext::classicExtras} {
	%W mark set tk::anchor%W insert
    }
}
bind Ntext <Select> {
    %W mark set tk::anchor%W insert
}
bind Ntext <Control-Shift-space> {
    if {$::ntext::classicExtras} {
	set tk::Priv(selectMode) char
	tk::TextKeyExtend %W insert
    }
}
bind Ntext <Shift-Select> {
    set tk::Priv(selectMode) char
    tk::TextKeyExtend %W insert
}
bind Ntext <Control-slash> {
    %W tag add sel 1.0 end
}
bind Ntext <Control-backslash> {
    %W tag remove sel 1.0 end
    if {[%W cget -autoseparators]} {
	%W edit separator
    }
}
bind Ntext <<Cut>> {
    ntext::new_textCut %W
}
bind Ntext <<Copy>> {
    tk_textCopy %W
}
bind Ntext <<Paste>> {
    ntext::new_textPaste %W
}
bind Ntext <<Clear>> {
    if {[%W tag nextrange sel 1.0 end] ne ""} {
	set ::ntext::OldFirst [%W index sel.first]
	%W delete sel.first sel.last
	ntext::AdjustIndentOneLine %W $::ntext::OldFirst
    }
}
bind Ntext <<PasteSelection>> {
    if {$tk_strictMotif || ![info exists tk::Priv(mouseMoved)]
	    || !$tk::Priv(mouseMoved)} {
	ntext::TextPasteSelection %W %x %y
    }
}
# Implement Insert/Overwrite modes
bind Ntext <Insert> {
    set ntext::overwrite [expr !$ntext::overwrite]
#    This behaves strangely on a newline or tab:
#    %W configure -blockcursor $ntext::overwrite
    if {$ntext::overwrite} {
	%W configure -insertbackground red
    } else {
	%W configure -insertbackground black
    }
}
bind Ntext <KeyPress> {
    ntext::TextInsert %W %A
}

# Ignore all Alt, Meta, and Control keypresses unless explicitly bound.
# Otherwise, if a widget binding for one of these is defined, the
# <KeyPress> class binding will also fire and insert the character,
# which is wrong.

bind Ntext <Alt-KeyPress> {# nothing }
bind Ntext <Meta-KeyPress> {# nothing}
bind Ntext <Control-KeyPress> {# nothing}
# Make Escape clear the selection
bind Ntext <Escape> {
    %W tag remove sel 0.0 end
    if {[%W cget -autoseparators]} {
	%W edit separator
    }
}
bind Ntext <KP_Enter> {# nothing}
if {[tk windowingsystem] eq "aqua"} {
    bind Ntext <Command-KeyPress> {# nothing}
}

# Additional emacs-like bindings:

bind Ntext <Control-a> {
    if {$::ntext::classicExtras && !$tk_strictMotif} {
	tk::TextSetCursor %W {insert display linestart}
    }
}
bind Ntext <Control-b> {
    if {$::ntext::classicExtras && !$tk_strictMotif} {
	tk::TextSetCursor %W insert-1displayindices
    }
}
bind Ntext <Control-d> {
    if {$::ntext::classicExtras && !$tk_strictMotif} {
	%W delete insert
	ntext::AdjustIndentOneLine %W insert
    }
}
bind Ntext <Control-e> {
    if {$::ntext::classicExtras && !$tk_strictMotif} {
	tk::TextSetCursor %W {insert display lineend}
    }
}
bind Ntext <Control-f> {
    if {$::ntext::classicExtras && !$tk_strictMotif} {
	tk::TextSetCursor %W insert+1displayindices
    }
}
bind Ntext <Control-k> {
    if {$::ntext::classicExtras && !$tk_strictMotif} {
	if {[%W compare insert == {insert lineend}]} {
	    %W delete insert
	} else {
	    %W delete insert {insert lineend}
	}
	ntext::AdjustIndentOneLine %W insert
    }
}
bind Ntext <Control-n> {
    if {$::ntext::classicExtras && !$tk_strictMotif} {
	tk::TextSetCursor %W [tk::TextUpDownLine %W 1]
    }
}
bind Ntext <Control-o> {
    if {$::ntext::classicExtras && !$tk_strictMotif} {
	%W insert insert \n
	%W mark set insert insert-1c
	ntext::AdjustIndentOneLine %W "insert + 1 line"
    }
}
bind Ntext <Control-p> {
    if {$::ntext::classicExtras && !$tk_strictMotif} {
	tk::TextSetCursor %W [tk::TextUpDownLine %W -1]
    }
}
bind Ntext <Control-t> {
    if {$::ntext::classicExtras && !$tk_strictMotif} {
	ntext::TextTranspose %W
    }
}

bind Ntext <<Undo>> {
    # An Undo operation may remove the separator at the top of the Undo stack.
    # Then the item at the top of the stack gets merged with the subsequent changes.
    # Place separators before and after Undo to prevent this.
    if {[%W cget -autoseparators]} {
	%W edit separator
    }
    if {![catch { %W edit undo }]} {
	# the undo stack does not record tags - so we need to reapply them
	ntext::AdjustIndentMultipleLines %W 1.0 end
    }
    if {[%W cget -autoseparators]} {
	%W edit separator
    }
}

bind Ntext <<Redo>> {
    if {![catch { %W edit redo }]} {
	# the redo stack does not record tags - so we need to reapply them
	ntext::AdjustIndentMultipleLines %W 1.0 end
    }
}

bind Ntext <Meta-b> {
    if {!$tk_strictMotif} {
	tk::TextSetCursor %W \
	    [tk::TextPrevPos %W insert ntext::new_startOfPreviousWord]
    }
}
bind Ntext <Meta-d> {
    if {!$tk_strictMotif} {
	%W delete insert [ntext::TextNextWord %W insert]
    }
    ntext::AdjustIndentOneLine %W insert
}
bind Ntext <Meta-f> {
    if {!$tk_strictMotif} {
	tk::TextSetCursor %W [ntext::TextNextWord %W insert]
    }
}
bind Ntext <Meta-less> {
    if {!$tk_strictMotif} {
	tk::TextSetCursor %W 1.0
    }
}
bind Ntext <Meta-greater> {
    if {!$tk_strictMotif} {
	tk::TextSetCursor %W end-1c
    }
}
bind Ntext <Meta-BackSpace> {
    if {!$tk_strictMotif} {
	%W delete \
	    [tk::TextPrevPos %W insert ntext::new_startOfPreviousWord] insert
    }
    ntext::AdjustIndentOneLine %W insert
}
bind Ntext <Meta-Delete> {
    if {!$tk_strictMotif} {
	%W delete \
	    [tk::TextPrevPos %W insert ntext::new_startOfPreviousWord] insert
    }
    ntext::AdjustIndentOneLine %W insert
}

# Macintosh only bindings:

if {[tk windowingsystem] eq "aqua"} {
bind Ntext <Option-Left> {
    tk::TextSetCursor %W \
	[tk::TextPrevPos %W insert ntext::new_startOfPreviousWord]
}
bind Ntext <Option-Right> {
    tk::TextSetCursor %W [ntext::TextNextWord %W insert]
}
bind Ntext <Option-Up> {
    tk::TextSetCursor %W [tk::TextPrevPara %W insert]
}
bind Ntext <Option-Down> {
    tk::TextSetCursor %W [tk::TextNextPara %W insert]
}
bind Ntext <Shift-Option-Left> {
    tk::TextKeySelect %W \
	[tk::TextPrevPos %W insert ntext::new_startOfPreviousWord]
}
bind Ntext <Shift-Option-Right> {
    tk::TextKeySelect %W [ntext::TextNextWord %W insert]
}
bind Ntext <Shift-Option-Up> {
    tk::TextKeySelect %W [tk::TextPrevPara %W insert]
}
bind Ntext <Shift-Option-Down> {
    tk::TextKeySelect %W [tk::TextNextPara %W insert]
}
# ntext::TextScrollPages is probably not what is needed here, because
# tk::TextScrollPages only scrolls, and relies on the calling code to set the
# insert mark.  Keep the old functionality.
# Don't Mac users need to scroll up as well as down?
# Feedback from Mac users please.
bind Ntext <Control-v> {
#    tk::TextScrollPages %W 1
    %W yview scroll 1 pages
}

# End of Mac only bindings
}

# A few additional bindings of my own.

bind Ntext <Control-h> {
    if {$::ntext::classicExtras && (!$tk_strictMotif)
	    && [%W compare insert != 1.0]} {
	%W delete insert-1c
	%W see insert
	ntext::AdjustIndentOneLine %W insert
    }
}
bind Ntext <2> {
    if {!$tk_strictMotif} {
	tk::TextScanMark %W %x %y
    }
}
bind Ntext <B2-Motion> {
    if {!$tk_strictMotif} {
	tk::TextScanDrag %W %x %y
    }
}
set ::tk::Priv(prevPos) {}

# The MouseWheel will typically only fire on Windows and MacOS X.
# However, someone could use the "event generate" command to produce one
# on other platforms.  We must be careful not to round -ve values of %D
# down to zero.

if {[tk windowingsystem] eq "aqua"} {
    bind Ntext <MouseWheel> {
        %W yview scroll [expr {-15 * (%D)}] pixels
    }
    bind Ntext <Option-MouseWheel> {
        %W yview scroll [expr {-150 * (%D)}] pixels
    }
    bind Ntext <Shift-MouseWheel> {
        %W xview scroll [expr {-15 * (%D)}] pixels
    }
    bind Ntext <Shift-Option-MouseWheel> {
        %W xview scroll [expr {-150 * (%D)}] pixels
    }
} else {
    # We must make sure that positive and negative movements are rounded
    # equally to integers, avoiding the problem that
    #     (int)1/3 = 0,
    # but
    #     (int)-1/3 = -1
    # The following code ensure equal +/- behaviour.
    bind Ntext <MouseWheel> {
	if {%D >= 0} {
	    %W yview scroll [expr {-%D/3}] pixels
	} else {
	    %W yview scroll [expr {(2-%D)/3}] pixels
	}
    }
}

if {"x11" eq [tk windowingsystem]} {
    # Support for mousewheels on Linux/Unix commonly comes through mapping
    # the wheel to the extended buttons.  If you have a mousewheel, find
    # Linux configuration info at:
    #	http://www.inria.fr/koala/colas/mouse-wheel-scroll/
    bind Ntext <4> {
	if {!$tk_strictMotif} {
	    %W yview scroll -50 pixels
	}
    }
    bind Ntext <5> {
	if {!$tk_strictMotif} {
	    %W yview scroll 50 pixels
	}
    }
}

bind Ntext <Configure> {
    ::ntext::AdjustIndentMultipleLines %W 1.0 end
}


##### End of bindings. Now define the namespace and its variables.


namespace eval ::ntext {

# Variables that control the behaviour of certain bindings and may be changed
# by the user's script
# Set to 1 for "classic Text" style (the Tcl/Tk defaults), 0 for "Ntext" style

# Whether Shift-Button-1 has a variable or fixed anchor
variable classicAnchor      0

# Whether to activate certain traditional "extra" bindings
variable classicExtras      0

# Whether Shift-Button-1 ignores changes made by the keyboard to the insert
# mark
variable classicMouseSelect 0

# Type of word-boundary search
variable classicWordBreak   0

# Whether to use -lmargin2 to align the wrapped display lines with their
# own first display line
variable classicWrap        1

# Advanced use (see man page): align to this character on the first display
# line
variable newWrapRegexp   {[^[:space:]]}

# Variable that sets overwrite/insert mode: may be changed by the user's script
# but is normally controlled by a binding to <KeyPress-Insert>
variable overwrite          0

# Debugging aid for developers: sets the background color for each logical line
# according to the magnitude of its hanging (-lmargin2) indent.
variable lm2IndentDebug     0

# Variables that will hold regexp's for word boundary detection

variable tcl_match_wordBreakAfter
variable tcl_match_wordBreakBefore
variable tcl_match_endOfWord
variable tcl_match_startOfNextWord
variable tcl_match_startOfPreviousWord


# These variables are for internal use by ntext only. They should not be
# modified by the user's script.
variable Bcount             0
variable OldFirst          {}


}

##### End of namespace definition.  Now define the procs.

# ::tk::TextClosestGap --
# Given x and y coordinates, this procedure finds the closest boundary
# between characters to the given coordinates and returns the index
# of the character just after the boundary.
#
# Arguments:
# w -		The text window.
# x -		X-coordinate within the window.
# y -		Y-coordinate within the window.

# ::ntext::TextClosestGap is copied from ::tk with modifications:
# modified to fix the jump-to-next-line issue.

proc ::ntext::TextClosestGap {w x y} {
    set pos [$w index @$x,$y]
    set bbox [$w bbox $pos]
    if {$bbox eq ""} {
	return $pos
    }
    if {($x - [lindex $bbox 0]) < ([lindex $bbox 2]/2)} {
	return $pos
    }
    # Never return a position that will place the cursor on the next display
    # line. This used to happen if $x is closer to the end of the display line
    # than to its last character.
    if {[$w cget -wrap] eq "word"} {
	set lineType displaylines
    } else {
	set lineType lines
    }
    if {[$w count -$lineType $pos "$pos + 1 char"] != 0} {
	return $pos
    } else {
    }
    $w index "$pos + 1 char"
}

# ::tk::TextButton1 --
# This procedure is invoked to handle button-1 presses in text
# widgets.  It moves the insertion cursor, sets the selection anchor,
# and claims the input focus.
#
# Arguments:
# w -		The text window in which the button was pressed.
# x -		The x-coordinate of the button press.
# y -		The x-coordinate of the button press.

# ::ntext::TextButton1 is copied from ::tk with no modifications:
# so it calls functions in ::ntext, not ::tk

proc ::ntext::TextButton1 {w x y} {
    variable ::tk::Priv

    set Priv(selectMode) char
    set Priv(mouseMoved) 0
    set Priv(pressX) $x
    $w mark set insert [TextClosestGap $w $x $y]
    $w mark set tk::anchor$w insert
    # Set the anchor mark's gravity depending on the click position
    # relative to the gap
    set bbox [$w bbox [$w index tk::anchor$w]]
    if {$x > [lindex $bbox 0]} {
	$w mark gravity tk::anchor$w right
    } else {
	$w mark gravity tk::anchor$w left
    }
    # Allow focus in any case on Windows, because that will let the
    # selection be displayed even for state disabled text widgets.
    if {$::tcl_platform(platform) eq "windows" \
	    || [$w cget -state] eq "normal"} {
	focus $w
    }
    if {[$w cget -autoseparators]} {
	$w edit separator
    }
}

# ::tk::TextSelectTo --
# This procedure is invoked to extend the selection, typically when
# dragging it with the mouse.  Depending on the selection mode (character,
# word, line) it selects in different-sized units.  This procedure
# ignores mouse motions initially until the mouse has moved from
# one character to another or until there have been multiple clicks.
#
# Note that the 'anchor' is implemented programmatically using
# a text widget mark, and uses a name that will be unique for each
# text widget (even when there are multiple peers).  Currently the
# anchor is considered private to Tk, hence the name 'tk::anchor$w'.
#
# Arguments:
# w -		The text window in which the button was pressed.
# x -		Mouse x position.
# y - 		Mouse y position.

# ::ntext::TextSelectTo is copied from ::tk with modifications:
# modified to prevent word selection from crossing a line end.

proc ::ntext::TextSelectTo {w x y {extend 0}} {
    global tcl_platform
    variable ::tk::Priv

    set cur [TextClosestGap $w $x $y]
    if {[catch {$w index tk::anchor$w}]} {
	$w mark set tk::anchor$w $cur
    }
    set anchor [$w index tk::anchor$w]
    if {[$w compare $cur != $anchor] || (abs($Priv(pressX) - $x) >= 3)} {
	set Priv(mouseMoved) 1
    }
    switch -- $Priv(selectMode) {
	char {
	    if {[$w compare $cur < tk::anchor$w]} {
		set first $cur
		set last tk::anchor$w
	    } else {
		set first tk::anchor$w
		set last $cur
	    }
	}
	word {
	    # Set initial range based only on the anchor (1 char min width -
	    # MOD - unless this straddles a display line end)
	    if {[$w cget -wrap] eq "word"} {
		set lineType displaylines
	    } else {
		set lineType lines
	    }
	    if {[$w mark gravity tk::anchor$w] eq "right"} {
		set first "tk::anchor$w"
		set last "tk::anchor$w + 1c"
		if {[$w count -$lineType $first $last] != 0} {
			set last $first
		} else {
		}
	    } else {
		set first "tk::anchor$w - 1c"
		set last "tk::anchor$w"
		if {[$w count -$lineType $first $last] != 0} {
			set first $last
		} else {
		}
	    }
	    if {($last eq $first) && ([$w index $first] eq $cur)} {
		# Use $first and $last as above; further extension will straddle
		# a display line. Better to have no selection than a bad one.
	    } else {
		# Extend range (if necessary) based on the current point
		if {[$w compare $cur < $first]} {
		    set first $cur
		} elseif {[$w compare $cur > $last]} {
		    set last $cur
		}

		# Now find word boundaries
		set first1 [$w index "$first + 1c"]
		set last1  [$w index "$last - 1c"]
		if {[$w count -$lineType $first $first1] != 0} {
		    set first1 [$w index $first]
		} else {
		}
		if {[$w count -$lineType $last $last1] != 0} {
		    set last1 [$w index $last]
		} else {
		}
		set first2 [::tk::TextPrevPos $w "$first1" \
		    ntext::new_wordBreakBefore]
		set last2  [::tk::TextNextPos $w "$last1"  \
		    ntext::new_wordBreakAfter]
		# Don't allow a "word" to straddle a display line boundary (or,
		# in -wrap char mode, a logical line boundary). This is not the
		# right result if -wrap word has been forced into -wrap char
		# because a word is too long.
		if {[$w count -$lineType $first2 $first] != 0} {
		    set first [$w index "$first display linestart"]
		} else {
		    set first $first2
		}
		if {[$w count -$lineType $last2 $last] != 0} {
		    set last [$w index "$last display lineend"]
		} else {
		    set last $last2
		}
	    }
	}
	line {
	    # Set initial range based only on the anchor
	    set first "tk::anchor$w linestart"
	    set last "tk::anchor$w lineend"

	    # Extend range (if necessary) based on the current point
	    if {[$w compare $cur < $first]} {
		set first "$cur linestart"
	    } elseif {[$w compare $cur > $last]} {
		set last "$cur lineend"
	    }
	    set first [$w index $first]
	    set last [$w index "$last + 1c"]
	}
    }
    if {$Priv(mouseMoved) || ($Priv(selectMode) ne "char")} {
	$w tag remove sel 0.0 end
	$w mark set insert $cur
	$w tag add sel $first $last
	$w tag remove sel $last end
	update idletasks
    }
}


# ::tk::TextKeyExtend -- called without modification

# ::tk::TextPasteSelection --
# This procedure sets the insertion cursor to the mouse position,
# inserts the selection, and sets the focus to the window.
#
# Arguments:
# w -		The text window.
# x, y - 	Position of the mouse.

# ::ntext::TextPasteSelection is copied from ::tk with modifications:
# modified to set oldInsert and call AdjustIndentMultipleLines.

proc ::ntext::TextPasteSelection {w x y} {
    $w mark set insert [TextClosestGap $w $x $y]
    set oldInsert [$w index insert]
    if {![catch {::tk::GetSelection $w PRIMARY} sel]} {
	set oldSeparator [$w cget -autoseparators]
	if {$oldSeparator} {
	    $w configure -autoseparators 0
	    $w edit separator
	}
	$w insert insert $sel
	AdjustIndentMultipleLines $w $oldInsert insert
	if {$oldSeparator} {
	    $w edit separator
	    $w configure -autoseparators 1
	}
    }
    if {[$w cget -state] eq "normal"} {
	focus $w
    }
}


# ::tk::TextAutoScan --
# This procedure is invoked when the mouse leaves a text window
# with button 1 down.  It scrolls the window up, down, left, or right,
# depending on where the mouse is (this information was saved in
# ::tk::Priv(x) and ::tk::Priv(y)), and reschedules itself as an "after"
# command so that the window continues to scroll until the mouse
# moves back into the window or the mouse button is released.
#
# Arguments:
# w -		The text window.

# ::ntext::TextAutoScan is copied from ::tk with modifications:
# chiefly so it calls ::ntext::TextSelectTo not ::tk::TextSelectTo
# modified so it calls itself and not ::tk::TextAutoScan

proc ::ntext::TextAutoScan {w} {
    variable ::tk::Priv
    if {![winfo exists $w]} {
	return
    }
    if {$Priv(y) >= [winfo height $w]} {
	$w yview scroll [expr {1 + $Priv(y) - [winfo height $w]}] pixels
    } elseif {$Priv(y) < 0} {
	$w yview scroll [expr {-1 + $Priv(y)}] pixels
    } elseif {$Priv(x) >= [winfo width $w]} {
	$w xview scroll 2 units
    } elseif {$Priv(x) < 0} {
	$w xview scroll -2 units
    } else {
	return
    }
    TextSelectTo $w $Priv(x) $Priv(y)
    set Priv(afterId) [after 50 [list ntext::TextAutoScan $w]]
}

# ::tk::TextSetCursor -- called without modification

# ::tk::TextKeySelect -- called without modification

# ::tk::TextResetAnchor -- called without modification

# ::tk::TextInsert --
# Insert a string into a text at the point of the insertion cursor.
# If there is a selection in the text, and it covers the point of the
# insertion cursor, then delete the selection before inserting.
#
# Arguments:
# w -		The text window in which to insert the string
# s -		The string to insert (usually just a single character)

# ::ntext::TextInsert is copied from ::tk with modifications:
# modified to implement Insert/Overwrite and to call AdjustIndentOneLine
# combine nested 'if' statements to avoid repetition of 'else' code

proc ::ntext::TextInsert {w s} {
    if {($s eq "") || ([$w cget -state] eq "disabled")} {
	return
    }
    set compound 0
    if {[llength [set range [$w tag ranges sel]]] &&
	[$w compare [lindex $range 0] <= insert]  &&
	[$w compare [lindex $range end] >= insert]} {

	set oldSeparator [$w cget -autoseparators]
	if {$oldSeparator} {
	    $w configure -autoseparators 0
	    $w edit separator
	    set compound 1
	}
	$w delete [lindex $range 0] [lindex $range end]
    } elseif {$::ntext::overwrite && ($s ne "\n") && ($s ne "\t") &&
		([$w get insert] ne "\n")} {
	set oldSeparator [$w cget -autoseparators]
	if {$oldSeparator} {
	    $w configure -autoseparators 0
	    $w edit separator
	    set compound 1
	    # When undoing an overwrite, the insert mark is left
	    # in the "wrong" place - after and not before the change.
	    # Some non-Tk editors do this too.
	}
	$w delete insert
    }
    $w insert insert $s
    AdjustIndentOneLine $w insert
    $w see insert
    if {$compound && $oldSeparator} {
	$w edit separator
	$w configure -autoseparators 1
    }
}

# ::tk::TextUpDownLine -- called without modification

# ::tk::TextPrevPara -- called without modification

# ::tk::TextNextPara -- called without modification

# ::tk::TextScrollPages --
# This is a utility procedure used in bindings for moving up and down
# pages and possibly extending the selection along the way.  It scrolls
# the view in the widget by the number of pages, and it returns the
# index of the character that is at the same position in the new view
# as the insertion cursor used to be in the old view.
#
# Arguments:
# w -		The text window in which the cursor is to move.
# count -	Number of pages forward to scroll;  may be negative
#		to scroll backwards.

# ::ntext::TextScrollPages is called like ::tk::TextScrollPages, but is
# completely rewritten, and behaves differently.
#
# ::tk::TextScrollPages scrolls the widget, and returns an index (a new value
# for the insert mark); if the mark was on-screen before the scroll,
# ::tk::TextScrollPages tries to return an index that keeps it in the same
# screen position.
#
# ::ntext::TextScrollPages takes a slightly different approach:
# like ::tk::TextScrollPages, it returns an index (a new value for the insert
# mark), and lets the calling code decide whether to move the mark.
# Unlike ::tk::TextScrollPages, when called with two arguments it does no
# scrolling - it relies on the calling code to do the scrolling, which in
# practice is usually when it tries to 'see' the returned index value.
#
# By focussing on the insert mark, ::ntext::TextScrollPages has the
# following useful features:
#  - When the slack is less than one page, it "moves" the insert mark as far
#    as possible.
#  - When there is no slack, it "moves" the insert mark to the start/end of
#    the widget.
#  - It uses ::tk::TextUpDownLine to remember the initial x-value.
#
# When called with three arguments, 3rd argument = "preScroll", then, if the
# new position of the insert mark is off-screen, ::ntext::TextScrollPages
# will scroll the widget, to try to make the calling code's "see" move the
# returned index value to the middle, not the edge, of the widget.  This
# feature is most useful in widgets with only a few visible lines, where it
# prevents successive calls from moving the insert mark between the middle and
# the edge of the widget.

proc ::ntext::TextScrollPages {w count {help ""}} {
    set spareLines 1 ;# adjustable

    set oldInsert [$w index insert]
    set count [expr {int($count)}]
    if {$count == 0} {
	return $oldInsert
    }
    set visibleLines [$w count -displaylines @0,0 @0,20000]
    if {$visibleLines > $spareLines} {
	set pageLines [expr {$visibleLines - $spareLines}]
    } else {
	set pageLines 1
    }
    set newInsert  [::tk::TextUpDownLine $w [expr {$pageLines * $count}]]
    if {[$w compare $oldInsert != $newInsert]} {
	set finalInsert $newInsert
    } elseif {$count < 0} {
	set finalInsert 1.0
    } else {
	set finalInsert [$w index "end -1 char"]
    }
    if {($help eq "preScroll") && ([$w bbox $finalInsert] eq "")} {
	# If $finalInsert is offscreen, try to put it in the middle
	if {    [$w count -displaylines 1.0 $finalInsert] > \
		[$w count -displaylines $finalInsert end]} {
	    $w see 1.0
	} else {
	    $w see end
	}
	$w see $finalInsert
    }
    return $finalInsert
}

# ::tk::TextTranspose --
# This procedure implements the "transpose" function for text widgets.
# It tranposes the characters on either side of the insertion cursor,
# unless the cursor is at the end of the line.  In this case it
# transposes the two characters to the left of the cursor.  In either
# case, the cursor ends up to the right of the transposed characters.
#
# Arguments:
# w -		Text window in which to transpose.

# ::ntext::TextTranspose is copied from ::tk::TextTranspose with modifications:
# modified to call AdjustIndentOneLine.
# rename local variable autosep to oldSeparator for uniformity with other procs

proc ::ntext::TextTranspose w {
    set pos insert
    if {[$w compare $pos != "$pos lineend"]} {
	set pos [$w index "$pos + 1 char"]
    }
    set new [$w get "$pos - 1 char"][$w get  "$pos - 2 char"]
    if {[$w compare "$pos - 1 char" == 1.0]} {
	return
    }
    # ensure this is seen as an atomic op to undo
    set oldSeparator [$w cget -autoseparators]
    if {$oldSeparator} {
	$w configure -autoseparators 0
	$w edit separator
    }
    $w delete "$pos - 2 char" $pos
    $w insert insert $new

    if {[$w compare insert == "insert linestart"]} {
	AdjustIndentOneLine $w "insert - 1 line"
    }
    AdjustIndentOneLine $w insert

    $w see insert
    if {$oldSeparator} {
	$w edit separator
	$w configure -autoseparators 1
    }
}

# ::tk_textCopy -- called without modification

# ::tk_textCut --
# This procedure copies the selection from a text widget into the
# clipboard, then deletes the selection (if it exists in the given
# widget).
#
# Arguments:
# w -		Name of a text widget.

# ::ntext::new_textCut is copied from ::tk_textCut with modifications:
# modified to set LocalOldFirst, call AdjustIndentOneLine, and add autoseparators

# LocalOldFirst is never off by one: the final newline of the widget cannot
# be deleted.

proc ::ntext::new_textCut w {
    if {![catch {set data [$w get sel.first sel.last]}]} {
	set oldSeparator [$w cget -autoseparators]
	if {$oldSeparator} {
	    $w configure -autoseparators 0
	    $w edit separator
	}
	set LocalOldFirst [$w index sel.first]
	clipboard clear -displayof $w
	clipboard append -displayof $w $data
	$w delete sel.first sel.last
	AdjustIndentOneLine $w $LocalOldFirst
	if {$oldSeparator} {
	    $w edit separator
	    $w configure -autoseparators 1
	}
    }
    return
}

# ::tk_textPaste --
# This procedure pastes the contents of the clipboard to the insertion
# point in a text widget.
#
# Arguments:
# w -		Name of a text widget.

# ::ntext::new_textPaste is copied from ::tk_textPaste with modifications:
# - modified to set oldInsert, LocalOldFirst and ntextIndentMark, and call
#   AdjustIndentMultipleLines.
# - modified to behave the same way for X11 as for other windowing systems
# - modified to overwrite the selection (if it exists), even if the insert mark
#   is elsewhere

proc ::ntext::new_textPaste w {
    set oldInsert [$w index insert]
    global tcl_platform
    if {![catch {::tk::GetSelection $w CLIPBOARD} sel]} {
	set oldSeparator [$w cget -autoseparators]
	if {$oldSeparator} {
	    $w configure -autoseparators 0
	    $w edit separator
	}
	if {([tk windowingsystem] ne "x11TheOldFashionedWay") && \
		([$w tag nextrange sel 1.0 end] ne "")} {
	    set LocalOldFirst [$w index sel.first]
	    $w mark set ntextIndentMark sel.last
	    # right gravity mark, survives deletion
	    $w delete sel.first sel.last
	    $w insert $LocalOldFirst $sel
	    AdjustIndentMultipleLines $w $LocalOldFirst ntextIndentMark
	} else {
	    $w insert insert $sel
	    AdjustIndentMultipleLines $w $oldInsert insert
	}
	if {$oldSeparator} {
	    $w edit separator
	    $w configure -autoseparators 1
	}
    }
    return
}

# ::tk::TextNextWord --
# Returns the index of the next word position after a given position in the
# text.  The next word is platform dependent and may be either the next
# end-of-word position or the next start-of-word position after the next
# end-of-word position.
#
# Arguments:
# w -		The text window in which the cursor is to move.
# start -	Position at which to start search.

# ::ntext::TextNextWord is copied from ::tk::TextNextWord with modifications:
# modified to use a platform-independent definition: always goes to the start
# of the next word.

proc ::ntext::TextNextWord {w start} {
    ::tk::TextNextPos $w $start ntext::new_startOfNextWord
}

# ::tk::TextNextPos  -- called without modification
# ::tk::TextPrevPos  -- called without modification
# ::tk::TextScanMark -- called without modification
# ::tk::TextScanDrag -- called without modification


# Two new functions, HomeIndex and EndIndex, that can be used for "smart" Home
# and End operations

# ::ntext::HomeIndex --
#
# Return the index to jump to (from $index) as "Smart Home"
# Some corner cases (e.g. lots of leading whitespace, wrapped around)
# probably have a better solution; but there's no consensus on how a
# text editor should behave in such cases.
#
# Arguments:
# w -    		Name of a text widget.
# index -		an index in the widget

proc ::ntext::HomeIndex {w index} {
    set index   [$w index $index]
    set dls     [$w index "$index display linestart"]

    # Set firstNonSpace to the index of the first non-space character on the
    # logical line.
    set dlsList [split $dls .]
    set dlsLine [lindex $dlsList 0]
    set lls     $dlsLine.0
    set firstNonSpace \
	[$w search -regexp -- {[^[:space:]]} \
	     $dlsLine.0 [expr {$dlsLine + 1}].0]

    # Now massage $firstNonSpace so it contains the "usual" home position on
    # the first display line
    if {$firstNonSpace eq {}} {
	# No non-whitespace characters on the line
	set firstNonSpace $dlsLine.0
    } elseif {[$w count -displaylines $lls $firstNonSpace] != 0} {
	# Either lots of whitespace, or whitespace with character wrap forces
	# $firstNonSpace onto the next.
	# display line
	set firstNonSpace $dlsLine.0
    } else {
	# The usual case: the first non-whitespace $firstNonSpace is on the
	# first display line
    }

    if {$dls eq $lls} {
	# We're on the first display line
	if {$index eq $firstNonSpace} {
	    # we're at the first non-whitespace of the first display line
	    set home $lls
	} else {
	    # we're on the first display line, but not at the first
	    # non-whitespace
	    set home $firstNonSpace
	}
    } else {
	if {$dls eq $index} {
	    # we're at the start of a display line other than the first
	    set home $firstNonSpace
	} else {
	    # we're not on the first display line, and we're not at our display
	    # line's start
	    set home $dls
	}
    }
    return $home
}

# ::ntext::EndIndex --
#
# Return the index to jump to (from $index) as "Smart End"
#
# Arguments:
# w -    		Name of a text widget.
# index -		an index in the widget

proc ::ntext::EndIndex {w index} {
    set index    [$w index $index]
    set dle      [$w index "$index display lineend"]

    if {$dle eq $index} {
	# we're at the end of a display line: return the logical line end
	return [$w index "$index lineend"]
    } else {
	# return the display line end
	return $dle
    }
}

##### END OF CODE THAT IS MODIFIED text.tcl
##### THE CODE ABOVE DEPENDS ON THE PROCS DEFINED BELOW

##### START OF CODE FOR WORD BOUNDARY DETECTION

# We define ::ntext counterparts for the functions in lib/tcl8.5/word.tcl
# such as ::tcl_wordBreakAfter
# See man page for discussion of the variables ::tcl_wordchars
# and ::tcl_nonwordchars defined in word.tcl

# This code block defines the seven namespace procs
#   createMatchPatterns
#   initializeMatchPatterns
#   new_wordBreakAfter
#   new_wordBreakBefore
#   new_endOfWord
#   new_startOfNextWord
#   new_startOfPreviousWord


# ::ntext::createMatchPatterns --
#
# This procedure defines the regexp patterns that are used in text
# searches, and saves them in namespace variables ::ntext::tcl_match_*
#
# Each argument should be a regexp expression defining a class of
# characters (usually a bracket expression, a class-shorthand escape,
# or a single character); the third argument may be omitted, or supplied
# as the empty string, in which case it is unused.
#
# The arguments are analogous to lib/tcl8.5/word.tcl's global variables
# tcl_wordchars and tcl_nonwordchars, but are not exposed as global or
# namespace variables: instead, the regexp patterns that are used for
# the searches are exposed as namespace variables.
#
# Usually this procedure is called by ::ntext::initializeMatchPatterns
# with machine-generated arguments.
#
# Arguments:
# new_nonwordchars -		regexp expression for non-word characters
#                   		(e.g. whitespace)
# new_word1chars -		regexp expression for first set of word
#                 		characters (e.g. alphanumerics)
# new_word2chars -		(optional) regexp expression for second set
#                 		of word characters (e.g. punctuation)

proc ::ntext::createMatchPatterns {new_nonwordchars new_word1chars {new_word2chars {}}} {

    variable tcl_match_wordBreakAfter
    variable tcl_match_wordBreakBefore
    variable tcl_match_endOfWord
    variable tcl_match_startOfNextWord
    variable tcl_match_startOfPreviousWord

    if {$new_word2chars eq {}} {
	# With one "non-word" character class, and one "word" class, generate
	# the same regexp patterns as Tcl's default search functions:
	# The shorthand is based on ntext's default definitions for the
	# function arguments:
	# "s" $new_nonwordchars (space)
	# "w" $new_word1chars   (word)
	# "p" $new_word2chars   (punctuation)
	set wordBreakAfter      "ws|sw"
	set wordBreakBefore     "^.*($wordBreakAfter)"
	set endOfWord           "s*w+s"
	set startOfNextWord     "w*s+w"
	set startOfPreviousWord "s*(w+)s*\$"
    } else {
	# Generalise to one "non-word" character class, and two "word" classes
	set wordBreakAfter      "ps|pw|sp|sw|wp|ws"
	set wordBreakBefore     "^.*($wordBreakAfter)"
	set endOfWord           "s*w+s|s*w+p|s*p+s|s*p+w"
	set startOfNextWord     "w*s+w|p*s+w|p+w|w*s+p|p*s+p|w+p"
	set startOfPreviousWord "s*(w+)s*\$|p*(w+)s*\$|w*(p+)s*\$|s*(p+)s*\$"
	# all tested, the first two with Double-1
	# in the last three, note that whitespace is not considered a "word"
	# - in endOfWord, note that leading space is acceptable, but not leading
	#   anything else
	# - in startOfNextWord, note that leading characters are acceptable only
	#   before a space
	# - in startOfPreviousWord, note that trailing space is acceptable, but
	# - not trailing anything else
	# With these rules, generalisation to more classes of characters is
	# straightforward.
    }

    foreach pattern {wordBreakAfter wordBreakBefore endOfWord \
	    startOfNextWord startOfPreviousWord} {
	# Define the search pattern
	set tcl_match_$pattern [string map [list w $new_word1chars p \
		$new_word2chars s $new_nonwordchars] [set $pattern]]
    }
    return
}

# ::ntext::initializeMatchPatterns --
#
# This procedure calls createMatchPatterns with arguments appropriate for
# the values of ::ntext::classicWordBreak and ::tcl_platform(platform).

proc ::ntext::initializeMatchPatterns {} {
    variable classicWordBreak
    if {!$classicWordBreak} {
	# ntext style: two classes of word character
	set punct {]`|.,:;/~!%&*_+='~[{}^"?()}     ;#" keep \ as a word char
	set space {[:space:]}
	set tcl_punctchars "\[${punct}-\]"
	set tcl_spacechars "\[${space}\]"
	set tcl_word1chars "\[^${punct}${space}-\]"
    } elseif {$::tcl_platform(platform) eq "windows"} {
	# Windows style - any but a unicode space char
	set tcl_word1chars "\\S"
	set tcl_spacechars "\\s"
	set tcl_punctchars {}
    } else {
	# Motif style - any unicode word char (number, letter, or underscore)
	set tcl_word1chars "\\w"
	set tcl_spacechars "\\W"
	set tcl_punctchars {}
    }

    createMatchPatterns $tcl_spacechars $tcl_word1chars $tcl_punctchars
    return
}


# Now procs derived from those in lib/tcl8.5/word.tcl, Tcl 8.5a5
# = ActiveTcl 8.5beta6

# tcl_wordBreakAfter --
#
# This procedure returns the index of the first word boundary
# after the starting point in the given string, or -1 if there
# are no more boundaries in the given string.  The index returned refers
# to the first character of the pair that comprises a boundary.
#
# Arguments:
# str -		String to search.
# start -	Index into string specifying starting point.

# ::ntext::new_wordBreakAfter is copied from ::tcl_wordBreakAfter with
# modifications: new word-boundary detection rules

proc ::ntext::new_wordBreakAfter {str start} {
    variable tcl_match_wordBreakAfter
    set str [string range $str $start end]
    if {[regexp -indices $tcl_match_wordBreakAfter $str result]} {
	return [expr {[lindex $result 1] + $start}]
    }
    return -1
}

# tcl_wordBreakBefore --
#
# This procedure returns the index of the first word boundary
# before the starting point in the given string, or -1 if there
# are no more boundaries in the given string.  The index returned
# refers to the second character of the pair that comprises a boundary.
#
# Arguments:
# str -		String to search.
# start -	Index into string specifying starting point.

# ::ntext::new_wordBreakBefore is copied from ::tcl_wordBreakBefore with
# modifications: new word-boundary detection rules

proc ::ntext::new_wordBreakBefore {str start} {
    variable tcl_match_wordBreakBefore
    if {$start eq "end"} {
	set start [string length $str]
    }
    if {[regexp -indices $tcl_match_wordBreakBefore \
	    [string range $str 0 $start] result]} {
	return [lindex $result 1]
    }
    return -1
}

# tcl_endOfWord --
#
# This procedure returns the index of the first end-of-word location
# after a starting index in the given string.  An end-of-word location
# is defined to be the first whitespace character following the first
# non-whitespace character after the starting point.  Returns -1 if
# there are no more words after the starting point.
#
# Arguments:
# str -		String to search.
# start -	Index into string specifying starting point.

# ::ntext::new_endOfWord is copied from ::tcl_endOfWord with
# modifications:
# new word-boundary detection rules

proc ::ntext::new_endOfWord {str start} {
    variable tcl_match_endOfWord
    if {[regexp -indices $tcl_match_endOfWord \
	    [string range $str $start end] result]} {
	return [expr {[lindex $result 1] + $start}]
    }
    return -1
}

# tcl_startOfNextWord --
#
# This procedure returns the index of the first start-of-word location
# after a starting index in the given string.  A start-of-word
# location is defined to be a non-whitespace character following a
# whitespace character.  Returns -1 if there are no more start-of-word
# locations after the starting point.
#
# Arguments:
# str -		String to search.
# start -	Index into string specifying starting point.

# ::ntext::new_startOfNextWord is copied from ::tcl_startOfNextWord with
# modifications: new word-boundary detection rules

proc ::ntext::new_startOfNextWord {str start} {
    variable tcl_match_startOfNextWord
    if {[regexp -indices $tcl_match_startOfNextWord \
	    [string range $str $start end] result]} {
	return [expr {[lindex $result 1] + $start}]
    }
    return -1
}

# tcl_startOfPreviousWord --
#
# This procedure returns the index of the first start-of-word location
# before a starting index in the given string.
#
# Arguments:
# str -		String to search.
# start -	Index into string specifying starting point.

# ::ntext::new_startOfPreviousWord is copied from ::tcl_startOfPreviousWord
# with modifications: new word-boundary detection rules

proc ::ntext::new_startOfPreviousWord {str start} {
    variable tcl_match_startOfPreviousWord
    if {$start eq "end"} {
	set start [string length $str]
    }
    if {[regexp -indices \
	    $tcl_match_startOfPreviousWord \
	    [string range $str 0 [expr {$start - 1}]] result words(1) \
	    words(2) words(3) words(4) words(5) words(6) words(7) words(8) \
	    words(9) words(10) words(11) words(12) words(13) words(14) \
	    words(15) words(16)]} {
	set result -1
	foreach name [array names words] {
	    set val [lindex $words($name) 0]
	    if {$val != -1} {
		set result $val
		break
	    }
	}
	return $result
    }
    return -1
}

##### END OF CODE FOR WORD BOUNDARY DETECTION

##### START OF CODE TO HANDLE (OPTIONAL) INDENTATION USING -lmargin2

# ::ntext::wrapIndent --
#
# Procedure to adjust the hanging indent of a text widget.
# If indentation is active, i.e. if
# ::ntext::classicWrap == 0 and the widget has "-wrap word",
# the logical lines specified by the arguments will be indented so that for
# each logical line, the start of every wrapped display line is aligned with
# the first display line.
# If indentation is inactive, the procedure removes any existing indentation.
#
# This procedure is the only indentation procedure that should be called
# by user scripts.  It uses -lmargin2 to adjust the hanging indent of lines
# in a text widget.
#
# Call with one argument to adjust the indentation of the entire widget;
# with two arguments, to adjust the indentation of a single logical line;
# with three arguments, to adjust the indentation of a range of logical lines.
#
# Arguments:
# textWidget -		text widget to be indented
# index1 -		(optional) index in the first logical line to be
#         		indented
# index2 -		(optional) index in the last logical line to be indented

proc ::ntext::wrapIndent {textWidget args} {
    variable classicWrap
    if {([$textWidget cget -wrap] eq "word") && !$classicWrap} {
	if {[llength $args] == 0} {
	    AdjustIndentMultipleLines $textWidget 1.0 end
	} elseif {[llength $args] == 1} {
	    AdjustIndentOneLine $textWidget [lindex $args 0]
	} else {
	    AdjustIndentMultipleLines $textWidget \
		[lindex $args 0] [lindex $args 1]
	}
    } else {
	if {[llength $args] == 0} {
	    RemoveIndentMultipleLines $textWidget 1.0 end
	} elseif {[llength $args] == 1} {
	    RemoveIndentOneLine $textWidget [lindex $args 0]
	} else {
	    RemoveIndentMultipleLines $textWidget \
		[lindex $args 0] [lindex $args 1]
	}
    }
    return
}

# ::ntext::AdjustIndentMultipleLines --
#
# Procedure to adjust the hanging indent of multiple logical lines
# of a text widget - but only if indentation is active,
# i.e. if ::ntext::classicWrap == 0 and the widget has "-wrap word";
# otherwise the procedure does nothing.
#
# User scripts should call ::ntext::wrapIndent instead.
#
# Arguments:
# textWidget -		text widget to be indented
# index1 -		index in the first logical line to be indented
# index2 -		index in the last logical line to be indented

proc ::ntext::AdjustIndentMultipleLines {textWidget index1 index2} {
    # Ensure that each line has precisely one tag whose name begins
    # "ntextAlignLM2Indent=", and that this tag covers the whole line; set
    # its -lmargin2 value so that for each line, the start of every wrapped
    # display line is aligned with the first display line.
    variable classicWrap
    if {([$textWidget cget -wrap] eq "word") && !$classicWrap} {
	if {[$textWidget count -lines $index1 $index2] < 0} {
	    set index3 $index1
	    set index1 $index2
	    set index2 $index3
	}
	set index1 [$textWidget index "$index1 linestart"]
	set index2 [$textWidget index "$index2 linestart"]
	for     {set index $index1} \
		{$index <= $index2 && [$textWidget compare $index != end]} \
		{set index [$textWidget index "$index + 1 line"]} {
	    AdjustIndentOneLine $textWidget $index
	    set oldIndex $index
	}
    } else {
	# indentation not active
    }
    return
}

# ::ntext::AdjustIndentOneLine --
#
# Procedure to adjust the hanging indent of a single logical line
# of a text widget - but only if indentation is active,
# i.e. if ::ntext::classicWrap == 0 and the widget has "-wrap word";
# otherwise the procedure does nothing.
#
# User scripts should call ::ntext::wrapIndent instead.
#
# Arguments:
# textWidget -		text widget to be indented
# index -		index in the logical line to be indented

proc ::ntext::AdjustIndentOneLine {textWidget index} {
    # Ensure that the line has precisely one tag whose name begins
    # "ntextAlignLM2Indent=", and that this tag covers the whole line; set
    # its -lmargin2 value so that the start of every wrapped display line
    # is aligned with the first display line.
    variable classicWrap
    if {([$textWidget cget -wrap] eq "word") && !$classicWrap} {
	RemoveIndentOneLine $textWidget $index
	set pix [HowMuchIndent $textWidget $index]
	AddIndent $textWidget $index $pix
    } else {
	# indentation not active
    }
    return
}

# ::ntext::AddIndent --
#
# Procedure to set the hanging indent of a single logical line
# of a text widget.  The line must not already have indentation.
#
# User scripts should call ::ntext::wrapIndent instead.
#
# Arguments:
# textWidget -		text widget to be indented
# index -		index in the logical line to be indented
# pix -  		number of pixels of indentation

proc ::ntext::AddIndent {textWidget index pix} {
    # Add a tag with properties "-lmargin2 $pix" to the entire logical line
    variable lm2IndentDebug
    set lineStart     [$textWidget index "$index linestart"]
    set nextLineStart [$textWidget index "$lineStart + 1 line"]
    set tagName ntextAlignLM2Indent=${pix}
    $textWidget tag add $tagName $lineStart $nextLineStart
    $textWidget tag configure $tagName -lmargin2 ${pix}
    if {$lm2IndentDebug} {
	$textWidget tag configure $tagName -background [IntToColor $pix 100]
    }
    $textWidget tag lower $tagName
    return $tagName
}

# ::ntext::HowMuchIndent --
#
# Procedure to measure and return the number of pixels of hanging
# indent required by a single logical line of a text widget;
# i.e. how many pixels of -lmargin2 indentation does the logical line
# need, for alignment with its own first display line?
#
# User scripts should call ::ntext::wrapIndent instead.
#
# N.B. This procedure cannot be used before the widget is drawn: it uses
# display lines, which the widget calculates only when it is drawn.
#
# Arguments:
# textWidget -		text widget to be examined
# index -		index in the logical line to be examined

proc ::ntext::HowMuchIndent {textWidget index} {
    variable newWrapRegexp
    set lineStart [$textWidget index "$index linestart"]
    set secondDispLineStart [$textWidget index "$lineStart + 1 display line"]
    # checked that this gives the start of the next display line in
    # the *updated* display
    set indentTo  [$textWidget search -regexp -count matchLen -- \
	    $newWrapRegexp $lineStart $secondDispLineStart]
    if {$indentTo eq {}} {
	set pix 0
    } else {
	set indentTo [$textWidget index "$indentTo + $matchLen chars - 1 char"]
	set pix [$textWidget count -xpixels $lineStart $indentTo]
	# -update doesn't work yet for -xpixels: so this line appears to
	# assume a fixed-width font: yet it gets the correct result (with or
	# without -update) when a tab is inserted.
    }
    return $pix
}

# ::ntext::RemoveIndentOneLine --
#
# Procedure to remove the hanging indent of a single logical line
# of a text widget.  It does this regardless of whether indentation
# is active, i.e. regardless of the value of ::ntext::classicWrap
#
# User scripts should call ::ntext::wrapIndent instead.
#
# Arguments:
# textWidget -		text widget to be dedented
# index -		index in the logical line to be dedented

proc ::ntext::RemoveIndentOneLine {textWidget index} {
    # Remove -lmargin2 indentation, by removing each tag in the
    # line whose name begins "ntextAlignLM2Indent="

    set lineStart     [$textWidget index "$index linestart"]
    set nextLineStart [$textWidget index "$lineStart + 1 line"]

    set tagNames [$textWidget tag names $lineStart]

    foreach {dum1 tag dum2} [$textWidget dump -tag $lineStart $nextLineStart] {
	lappend tagNames $tag
    }

    # tagNames now holds all tags on this logical line
    # Remove the ones that ntext has previously used to set -lmargin2
    # These tags' names all begin with the same string.

    foreach tag $tagNames {
	if {[string range $tag 0 19] eq "ntextAlignLM2Indent="} {
	    #### puts $tag
	    $textWidget tag remove $tag $lineStart $nextLineStart
	}
    }
    return
}

# ::ntext::RemoveIndentMultipleLines --
#
# Procedure to remove the hanging indent of multiple logical lines
# of a text widget.  It does this regardless of whether indentation
# is active, i.e. regardless of the value of ::ntext::classicWrap
#
# User scripts should call ::ntext::wrapIndent instead.
#
# Arguments:
# textWidget -		text widget to be dedented
# index1 -		index in the first logical line to be dedented
# index2 -		index in the last logical line to be dedented

proc ::ntext::RemoveIndentMultipleLines {textWidget index1 index2} {
    # Remove -lmargin2 indentation, by removing each tag in these
    # lines whose name begins "ntextAlignLM2Indent="

    if {[$textWidget count -lines $index1 $index2] < 0} {
	set index3 $index1
	set index1 $index2
	set index2 $index3
    } else {
    }
    if {    [$textWidget compare $index1 == 1.0] && \
	    [$textWidget compare $index2 == end]} {
	# shortcut if whole widget needs processing

	# Remove -lmargin2 indentation, by removing each tag in the
	# widget whose name begins "ntextAlignLM2Indent="

	set tagNames [$textWidget tag names]

	# tagNames now holds all tags in the widget
	# Remove the ones that ntext has previously used to set -lmargin2
	# These tags' names all begin with the same string.

	foreach tag $tagNames {
	    if {[string range $tag 0 19] eq  "ntextAlignLM2Indent="} {
		#### puts $tag
		$textWidget tag remove $tag 1.0 end
	    }
	}
    } else {
	# go through the widget line-by-line
	set index1 [$textWidget index "$index1 linestart"]
	set index2 [$textWidget index "$index2 linestart"]
	for     {set index $index1} \
		{$index <= $index2 && [$textWidget compare $index != end]} \
		{set index [$textWidget index "$index + 1 line"]} {
	    RemoveIndentOneLine $textWidget $index
	    set oldIndex $index
	}
    }
    return
}

# ::ntext::IntToColor --
#
# Return a color in 24-bit hexadecimal format (e.g. "#FF8080") whose
# value is a periodic function of the number $pix, with period $range.
# Nothing too dark: each of R, G and B is in the range 156 to 255.
# Return value is white if $pix == 0
#
# Arguments:
# pix -  		real or integer number
# range -		real or integer number, non-zero

proc ::ntext::IntToColor {pix range} {
    set val [expr {int(99.9 - $pix * 100.0 / $range) % 100 + 156}]
    set r $val
    set g $val
    set b 255
    set color [format "#%02x%02x%02x" $r $g $b]
    return $color
}

##### END OF CODE TO HANDLE (OPTIONAL) INDENTATION USING -lmargin2

##### End of procs.

# Initialize match patterns for word boundary detection -

::ntext::initializeMatchPatterns

package provide ntext 0.81
