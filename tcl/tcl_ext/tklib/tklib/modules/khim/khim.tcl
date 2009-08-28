# khim.tcl --
#
#	Kevin's Hacky Input Method
#
# The 'khim' package defines a KHIM bindtag that can be applied to
# entry or text widgets (after widget-specific bindings but before
# Entry or Text bindings) to allow entry of international characters
# from a US keyboard without any input method other than Tk.
#
# It works by defining a "Compose" key (default is <Pause>).  When
# the "Compose" key is pressed, followed by a two-character sequence,
# those two characters are looked up in a user-configurable table and
# replaced with a Unicode character, which is inserted into the widget.
#
# Copyright (c) 2006 by Kevin B. Kenny.  All rights reserved.
#
# Refer to the file "license.terms" for the terms and conditions of
# use and redistribution of this file, and a DISCLAIMER OF ALL WARRANTEES.
#
# $Id: khim.tcl,v 1.10 2007/06/08 19:24:31 kennykb Exp $
# $Source: /cvsroot/tcllib/tklib/modules/khim/khim.tcl,v $
#
#----------------------------------------------------------------------

package require Tcl 8.4
package require Tk 8.4
package require msgcat 1.2
package require autoscroll 1.0

package provide khim 1.0.1

namespace eval khim [list variable KHIMDir [file dirname [info script]]]

namespace eval khim {

    namespace import ::msgcat::mc

    namespace export getOptions getConfig setConfig showHelp

    variable composeKey;		# Keysym of the key used for the
					# Compose function
    variable map;			# Dictionary whose keys are two-
					# character sequences and whose
					# values are the characters to
					# insert when those sequences
					# are composed
    variable UniOK;			# Table of code-point ranges that
					# conform to printable chars
    variable use;			# 1 if KHIM is enabled, 0 if not.

    #----------------------------------------------------------------------

    variable CMapFont;			# Font to use to display Unicode
					# characters in the character map

    variable CMapBadCharFont;		# Font in which to display the hex
					# values of bad code points

    #----------------------------------------------------------------------

    variable CMapCodePage;		# Array whose keys are the
					# path names of KHIM character map
					# dialogs and whose values are
					# the code pages on display in
					# those dialogs

    variable CMapFocus;			# Array whose keys are the path names
					# of KHIM character map dialogs and
					# whose values are the focus windows
					# where characters selected in the
					# dialogs will be inserted.

    variable CMapInputCodePage;		# Array whose keys are the path names
					# of KHIM character map dialogs and
					# whose values are variables used
					# to hold the value of the spinbox
					# that selects the code page.

    variable CMapSelectedCharacter;	# Array whose keys are the path names
					# of KHIM character map dialogs and
					# whose values are the characters
					# currently selected in the dialogs
					
    variable CMapXL;			# Array whose keys are the path names
					# of KHIM character map dialogs and
					# whose values are the
					# X co-ordinates of the columns in
					# the character map

    variable CMapYL;			# Array whose keys are the path names
					# of KHIM character map dialogs and
					# whose values are the
					# Y co-ordinates of the rows in the
					# character map.
}

# Load up message catalogs for the locale

namespace eval khim [list ::msgcat::mcload [file dirname [info script]]]

# Compressed table of which Unicode code points in the BMP are printable
# characters. The table is read, "0x0000-0x001f are not printable,
# 0x0020-0x007e are printable, 0x007f-0x009f are not printable,
# 0x00a0-0x00ac are printable, 0x00ad is not, 0x00ae-0x0241 are, etc."

set khim::UniOK {
    0x0000 0x0020 0x007f 0x00a0 0x00ad 0x00ae 0x0242 0x0250 0x0370 0x0374
    0x0376 0x037a 0x037b 0x037e 0x037f 0x0384 0x038b 0x038c 0x038d 0x038e
    0x03a2 0x03a3 0x03cf 0x03d0 0x0487 0x0488 0x04cf 0x04d0 0x04fa 0x0500
    0x0510 0x0531 0x0557 0x0559 0x0560 0x0561 0x0588 0x0589 0x058b 0x0591
    0x05ba 0x05bb 0x05c8 0x05d0 0x05eb 0x05f0 0x0600 0x060b 0x0616 0x061b
    0x061c 0x061e 0x0620 0x0621 0x063b 0x0640 0x065f 0x0660 0x06dd 0x06de
    0x070f 0x0710 0x074b 0x074d 0x076e 0x0780 0x07b2 0x0901 0x093a 0x093c
    0x094e 0x0950 0x0955 0x0958 0x0971 0x097d 0x097e 0x0981 0x0984 0x0985
    0x098d 0x098f 0x0991 0x0993 0x09a9 0x09aa 0x09b1 0x09b2 0x09b3 0x09b6
    0x09ba 0x09bc 0x09c5 0x09c7 0x09c9 0x09cb 0x09cf 0x09d7 0x09d8 0x09dc
    0x09de 0x09df 0x09e4 0x09e6 0x09fb 0x0a01 0x0a04 0x0a05 0x0a0b 0x0a0f
    0x0a11 0x0a13 0x0a29 0x0a2a 0x0a31 0x0a32 0x0a34 0x0a35 0x0a37 0x0a38
    0x0a3a 0x0a3c 0x0a3d 0x0a3e 0x0a43 0x0a47 0x0a49 0x0a4b 0x0a4e 0x0a59
    0x0a5d 0x0a5e 0x0a5f 0x0a66 0x0a75 0x0a81 0x0a84 0x0a85 0x0a8e 0x0a8f
    0x0a92 0x0a93 0x0aa9 0x0aaa 0x0ab1 0x0ab2 0x0ab4 0x0ab5 0x0aba 0x0abc
    0x0ac6 0x0ac7 0x0aca 0x0acb 0x0ace 0x0ad0 0x0ad1 0x0ae0 0x0ae4 0x0ae6
    0x0af0 0x0af1 0x0af2 0x0b01 0x0b04 0x0b05 0x0b0d 0x0b0f 0x0b11 0x0b13
    0x0b29 0x0b2a 0x0b31 0x0b32 0x0b34 0x0b35 0x0b3a 0x0b3c 0x0b44 0x0b47
    0x0b49 0x0b4b 0x0b4e 0x0b56 0x0b58 0x0b5c 0x0b5e 0x0b5f 0x0b62 0x0b66
    0x0b72 0x0b82 0x0b84 0x0b85 0x0b8b 0x0b8e 0x0b91 0x0b92 0x0b96 0x0b99
    0x0b9b 0x0b9c 0x0b9d 0x0b9e 0x0ba0 0x0ba3 0x0ba5 0x0ba8 0x0bab 0x0bae
    0x0bba 0x0bbe 0x0bc3 0x0bc6 0x0bc9 0x0bca 0x0bce 0x0bd7 0x0bd8 0x0be6
    0x0bfb 0x0c01 0x0c04 0x0c05 0x0c0d 0x0c0e 0x0c11 0x0c12 0x0c29 0x0c2a
    0x0c34 0x0c35 0x0c3a 0x0c3e 0x0c45 0x0c46 0x0c49 0x0c4a 0x0c4e 0x0c55
    0x0c57 0x0c60 0x0c62 0x0c66 0x0c70 0x0c82 0x0c84 0x0c85 0x0c8d 0x0c8e
    0x0c91 0x0c92 0x0ca9 0x0caa 0x0cb4 0x0cb5 0x0cba 0x0cbc 0x0cc5 0x0cc6
    0x0cc9 0x0cca 0x0cce 0x0cd5 0x0cd7 0x0cde 0x0cdf 0x0ce0 0x0ce2 0x0ce6
    0x0cf0 0x0d02 0x0d04 0x0d05 0x0d0d 0x0d0e 0x0d11 0x0d12 0x0d29 0x0d2a
    0x0d3a 0x0d3e 0x0d44 0x0d46 0x0d49 0x0d4a 0x0d4e 0x0d57 0x0d58 0x0d60
    0x0d62 0x0d66 0x0d70 0x0d82 0x0d84 0x0d85 0x0d97 0x0d9a 0x0db2 0x0db3
    0x0dbc 0x0dbd 0x0dbe 0x0dc0 0x0dc7 0x0dca 0x0dcb 0x0dcf 0x0dd5 0x0dd6
    0x0dd7 0x0dd8 0x0de0 0x0df2 0x0df5 0x0e01 0x0e3b 0x0e3f 0x0e5c 0x0e81
    0x0e83 0x0e84 0x0e85 0x0e87 0x0e89 0x0e8a 0x0e8b 0x0e8d 0x0e8e 0x0e94
    0x0e98 0x0e99 0x0ea0 0x0ea1 0x0ea4 0x0ea5 0x0ea6 0x0ea7 0x0ea8 0x0eaa
    0x0eac 0x0ead 0x0eba 0x0ebb 0x0ebe 0x0ec0 0x0ec5 0x0ec6 0x0ec7 0x0ec8
    0x0ece 0x0ed0 0x0eda 0x0edc 0x0ede 0x0f00 0x0f48 0x0f49 0x0f6b 0x0f71
    0x0f8c 0x0f90 0x0f98 0x0f99 0x0fbd 0x0fbe 0x0fcd 0x0fcf 0x0fd2 0x1000
    0x1022 0x1023 0x1028 0x1029 0x102b 0x102c 0x1033 0x1036 0x103a 0x1040
    0x105a 0x10a0 0x10c6 0x10d0 0x10fd 0x1100 0x115a 0x115f 0x11a3 0x11a8
    0x11fa 0x1200 0x1249 0x124a 0x124e 0x1250 0x1257 0x1258 0x1259 0x125a
    0x125e 0x1260 0x1289 0x128a 0x128e 0x1290 0x12b1 0x12b2 0x12b6 0x12b8
    0x12bf 0x12c0 0x12c1 0x12c2 0x12c6 0x12c8 0x12d7 0x12d8 0x1311 0x1312
    0x1316 0x1318 0x135b 0x135f 0x137d 0x1380 0x139a 0x13a0 0x13f5 0x1401
    0x1677 0x1680 0x169d 0x16a0 0x16f1 0x1700 0x170d 0x170e 0x1715 0x1720
    0x1737 0x1740 0x1754 0x1760 0x176d 0x176e 0x1771 0x1772 0x1774 0x1780
    0x17b4 0x17b6 0x17de 0x17e0 0x17ea 0x17f0 0x17fa 0x1800 0x180f 0x1810
    0x181a 0x1820 0x1878 0x1880 0x18aa 0x1900 0x191d 0x1920 0x192c 0x1930
    0x193c 0x1940 0x1941 0x1944 0x196e 0x1970 0x1975 0x1980 0x19aa 0x19b0
    0x19ca 0x19d0 0x19da 0x19de 0x1a1c 0x1a1e 0x1a20 0x1d00 0x1dc4 0x1e00
    0x1e9c 0x1ea0 0x1efa 0x1f00 0x1f16 0x1f18 0x1f1e 0x1f20 0x1f46 0x1f48
    0x1f4e 0x1f50 0x1f58 0x1f59 0x1f5a 0x1f5b 0x1f5c 0x1f5d 0x1f5e 0x1f5f
    0x1f7e 0x1f80 0x1fb5 0x1fb6 0x1fc5 0x1fc6 0x1fd4 0x1fd6 0x1fdc 0x1fdd
    0x1ff0 0x1ff2 0x1ff5 0x1ff6 0x1fff 0x2000 0x200b 0x2010 0x202a 0x202f
    0x2060 0x2070 0x2072 0x2074 0x208f 0x2090 0x2095 0x20a0 0x20b6 0x20d0
    0x20ec 0x2100 0x214d 0x2153 0x2184 0x2190 0x23dc 0x2400 0x2427 0x2440
    0x244b 0x2460 0x269d 0x26a0 0x26b2 0x2701 0x2705 0x2706 0x270a 0x270c
    0x2728 0x2729 0x274c 0x274d 0x274e 0x274f 0x2753 0x2756 0x2757 0x2758
    0x275f 0x2761 0x2795 0x2798 0x27b0 0x27b1 0x27bf 0x27c0 0x27c7 0x27d0
    0x27ec 0x27f0 0x2b14 0x2c00 0x2c2f 0x2c30 0x2c5f 0x2c80 0x2ceb 0x2cf9
    0x2d26 0x2d30 0x2d66 0x2d6f 0x2d70 0x2d80 0x2d97 0x2da0 0x2da7 0x2da8
    0x2daf 0x2db0 0x2db7 0x2db8 0x2dbf 0x2dc0 0x2dc7 0x2dc8 0x2dcf 0x2dd0
    0x2dd7 0x2dd8 0x2ddf 0x2e00 0x2e18 0x2e1c 0x2e1e 0x2e80 0x2e9a 0x2e9b
    0x2ef4 0x2f00 0x2fd6 0x2ff0 0x2ffc 0x3000 0x3040 0x3041 0x3097 0x3099
    0x3100 0x3105 0x312d 0x3131 0x318f 0x3190 0x31b8 0x31c0 0x31d0 0x31f0
    0x321f 0x3220 0x3244 0x3250 0x32ff 0x3300 0x4db6 0x4dc0 0x9fbc 0xa000
    0xa48d 0xa490 0xa4c7 0xa700 0xa717 0xa800 0xa82c 0xac00 0xd7a4 0xe000
    0xfa2e 0xfa30 0xfa6b 0xfa70 0xfada 0xfb00 0xfb07 0xfb13 0xfb18 0xfb1d
    0xfb37 0xfb38 0xfb3d 0xfb3e 0xfb3f 0xfb40 0xfb42 0xfb43 0xfb45 0xfb46
    0xfbb2 0xfbd3 0xfd40 0xfd50 0xfd90 0xfd92 0xfdc8 0xfdf0 0xfdfe 0xfe00
    0xfe1a 0xfe20 0xfe24 0xfe30 0xfe53 0xfe54 0xfe67 0xfe68 0xfe6c 0xfe70
    0xfe75 0xfe76 0xfeff 0xff01 0xffbf 0xffc2 0xffc8 0xffca 0xffd0 0xffd2
    0xffd8 0xffda 0xffdd 0xffe0 0xffe7 0xffe8 0xfff9 0xfffc 0xfffe
}

#----------------------------------------------------------------------
#
# BSearch --
#
#	Service procedure that does binary search in several places.
#
# Parameters:
#	list - List of lists, sorted in ascending order by the
#	       first elements
#	key - Value to search for
#
# Results:
#	Returns the index of the greatest element in $list that is less
#	than or equal to $key.
#
# Side effects:
#	None.
#
#----------------------------------------------------------------------

proc ::khim::BSearch { list key } {

    if { $key < [lindex $list 0 0] } {
	return -1
    }

    set l 0
    set u [expr { [llength $list] - 1 }]

    while { $l < $u } {

	# At this point, we know that
	#   $k >= [lindex $list $l 0]
	#   Either $u == [llength $list] or else $k < [lindex $list $u+1 0]
	# We find the midpoint of the interval {l,u} rounded UP, compare
	# against it, and set l or u to maintain the invariant.  Note
	# that the interval shrinks at each step, guaranteeing convergence.

	set m [expr { ( $l + $u + 1 ) / 2 }]
	if { $key >= [lindex $list $m 0] } {
	    set l $m
	} else {
	    set u [expr { $m - 1 }]
	}
    }

    return $l
}

#----------------------------------------------------------------------
#
# khim::ValidChar --
#
#	Test whether a number is the index of a valid character.
#
# Parameters:
#	c - Number of the character.
#
# Results:
#	Returns 1 if the character is a printable Unicode characte
#	in the BMP, and 0 otherwise.
#
#----------------------------------------------------------------------

proc ::khim::ValidChar { c } {
    variable UniOK
    return [expr {( [BSearch $UniOK $c] & 1 )}]
}

#----------------------------------------------------------------------
#
# khim::getOptions --
#
#	Displays a dialog that allows the user to enable/disable KHIM,
#	change key mappings, and change the Compose key.
#
# Parameters:
#	w -- Window path name of the dialog box.
#
# Results:
#	None.
#
# Side effects:
#	Changes options to whatever the user selects.
#
#----------------------------------------------------------------------

proc khim::getOptions {w} {

    variable use
    variable composeKey
    variable map
    variable inputUse
    variable inputComposeKey
    variable inputMap

    # Set temporary options for the use of the dialog

    set inputUse $use
    set inputComposeKey $composeKey
    array set inputMap $map

    # Create a modal dialog

    toplevel $w -class dialog
    wm withdraw $w
    set p [winfo toplevel [winfo parent $w]]
    set g [wm transient $p]
    if { ![string compare {} $g] } {
	set g $p
    }
    wm transient $w $g
    catch {wm attributes $w -toolwindow 1}
    wm title $w [mc "KHIM Controls"]
    bind $w <Destroy> [list ::khim::HandleDestroy $w %W]

    # Create GUI and manage geometry

    checkbutton $w.v -variable ::khim::inputUse -text [mc "Use KHIM"]
    label $w.l1 -text [mc "Compose key:"]
    button $w.b1 -textvariable khim::inputComposeKey \
	-command [list ::khim::GetComposeKey $w.b1]
    labelframe $w.lf1 -text [mc "Key sequences"] -padx 5 -pady 5 -width 400
    listbox $w.lf1.lb -height 20 -yscroll [list $w.lf1.y set] \
	-font {Courier 12} -width 8 -height 10 \
	-exportselection 0
    bind $w.lf1.lb <<ListboxSelect>> [list ::khim::Select %W]
    scrollbar $w.lf1.y -orient vertical -command [list $w.lf1.lb yview]
    frame $w.lf1.f1
    label $w.lf1.f1.l1 -text [mc "Input key sequence"]
    entry $w.lf1.f1.e1 -textvariable ::khim::inputSequence -width 2 \
	-font {Courier 12}
    bind $w.lf1.f1.e1 <FocusIn> {
	%W selection from 0
	%W selection to end
    }
    grid $w.lf1.f1.l1 $w.lf1.f1.e1
    grid columnconfigure $w.lf1.f1 2 -weight 1
    frame $w.lf1.f2
    label $w.lf1.f2.l1 -text [mc "Character"]
    entry $w.lf1.f2.e1 -textvariable ::khim::inputCharacter -width 2 \
	-font {Courier 12}
    bind $w.lf1.f2.e1 <FocusIn> {
	%W selection from 0
	%W selection to end
    }
    button $w.lf1.f2.b1 -text [mc "Unicode..."] \
	-command [list ::khim::FocusAndInsertSymbol $w.lf1.f2.e1]

    grid $w.lf1.f2.l1 $w.lf1.f2.e1
    grid $w.lf1.f2.b1 -row 0 -column 2 -sticky w -padx {20 0}
    grid columnconfigure $w.lf1.f2 3 -weight 1
    grid $w.lf1.lb -row 0 -column 0 -sticky nsew -rowspan 5
    grid $w.lf1.y -row 0 -column 1 -sticky ns -rowspan 5
    frame $w.lf1.f3
    button $w.lf1.f3.b1 -text [mc Change] \
	-command [list ::khim::ChangeSequence $w]
    button $w.lf1.f3.b2 -text [mc Delete] \
	-command [list ::khim::DeleteSequence $w]
    grid $w.lf1.f1 -row 0 -column 2 -sticky e -padx {20 0}
    grid $w.lf1.f2 -row 1 -column 2 -sticky e -padx {20 0}
    grid $w.lf1.f3.b1 $w.lf1.f3.b2 -padx 5 -sticky ew
    grid columnconfigure $w.lf1.f3 {0 1} -weight 1 -uniform A
    grid $w.lf1.f3 -row 3 -column 2 -sticky e -padx 20
    
    grid rowconfigure $w.lf1 2 -weight 1
    grid columnconfigure $w.lf1 3 -weight 1
    ::autoscroll::autoscroll $w.lf1.y
    frame $w.bf
    button $w.bf.ok -text [mc OK] -command [list ::khim::OK $w]
    button $w.bf.apply -text [mc Apply] -command [list ::khim::Apply $w]
    button $w.bf.cancel -text [mc Cancel] -command [list destroy $w]
    button $w.bf.help -text [mc Help...] \
	-command [list ::khim::showHelp $w.help]
    grid $w.bf.ok -row 0 -column 0 -padx 5 -sticky ew
    grid $w.bf.apply -row 0 -column 1 -padx 5 -sticky ew
    grid $w.bf.cancel -row 0 -column 2 -padx 5 -sticky ew
    grid $w.bf.help -row 0 -column 4 -padx 5
    grid columnconfigure $w.bf 3 -weight 1
    grid columnconfigure $w.bf {0 1 2 4} -uniform A
    grid $w.v -columnspan 2 -sticky w
    grid $w.l1 $w.b1 -sticky w
    grid $w.lf1 -columnspan 2 -sticky nsew -padx 5 -pady 5
    grid $w.bf -pady 5 -sticky ew -columnspan 2
    grid columnconfigure $w 1 -weight 1

    # Initialize the listbox content

    ShowSequences $w

    # Pop up the dialog

    wm deiconify $w
    tkwait window $w
    return
}

#----------------------------------------------------------------------
#
# khim::FocusAndInsertSymbol --
#
#	Shift focus to a given window and call the character map
#	interactor on it.
#
# Parameters:
#	w - Window to focus
#
# Results:
#	None.
#
# Side effects:
#	Whatever the user requests from the character map.
#
#----------------------------------------------------------------------

proc khim::FocusAndInsertSymbol {w} {
    focus $w
    CMapInteractor $w
    return
}

#----------------------------------------------------------------------
#
# khim::showHelp --
#
#	Display a help dialog for KHIM
#
# Parameters:
#	w -- Path name of the dialog
#
# Results:
#	None.
#
# Side effects:
#	Pops up the dialog.
#
# The help text is in the HELPTEXT entry in the message catalog of the
# current locale.
#
#----------------------------------------------------------------------

proc khim::showHelp {w} {

    variable KHIMDir

    # Create dialog to display help

    catch {destroy $w}
    toplevel $w
    wm withdraw $w
    set p [winfo toplevel [winfo parent $w]]
    set g [wm transient $p]
    if { ![string compare {} $g] } {
	set g $p
    }
    wm transient $w $g
    wm title $w [mc {KHIM Help}]
    catch {wm attributes $w -toolwindow 1}

    # Create and manage GUI components

    text $w.t -width 60 -yscrollcommand [list $w.y set] -wrap word
    set text [string trim [mc HELPTEXT]]
    if {$text eq "HELPTEXT"} {
	# This must be a version of Tcl that doesn't support the root
	# locale.  Do The Right Thing anyway
	set locale [::msgcat::mclocale]
	::msgcat::mclocale en
	set text [string trim [mc HELPTEXT]]
	if {$text eq "HELPTEXT"} {
	    ::msgcat::mcload $KHIMDir
	    set text [string trim [mc HELPTEXT]]
	}
	::msgcat::mclocale $locale
    }
    regsub -all -line {^[ \t]+} $text {} text
    regsub -all -line {[ \t]+$} $text {} text
    regsub -all {\n\n} $text <p> text
    regsub -all {\n} $text { } text
    regsub -all <p> $text \n\n text
    $w.t insert insert $text
    $w.t see 1.0
    $w.t configure -state disabled
    scrollbar $w.y -command [list $w.t yview] -orient vertical
    button $w.ok -text [mc OK] -command [list destroy $w]
    grid $w.t -row 0 -column 0 -sticky nsew
    grid $w.y -row 0 -column 1 -sticky ns
    grid $w.ok -pady 5 -row 1 -column 0 -columnspan 2
    grid rowconfigure $w 0 -weight 1
    grid columnconfigure $w 0 -weight 1

    # Determine whether we have a grab in effect

    set gr [grab current $w]
    if {$gr ne {}} {
	bind $w <Map> "focus $w.ok; grab set $w"
    } else {
	bind $w <Map> [list focus $w.ok]
    }

    # Pop up the dialog

    wm deiconify $w

    # Restore the grab if there was one

    if {$gr ne {}} {
	tkwait window $w
	grab set $gr
    }

    return
}

#----------------------------------------------------------------------
#
# khim::GetComposeKey --
#
#	Prompt the user for what key to use for the "Compose" function.
#
# Parameters:
#	parent -- Path name of the parent widget of the dialog
#
# Side effects:
#	Stores the user's selection in 'inputComposeKey'
#
#----------------------------------------------------------------------

proc khim::GetComposeKey {parent} {
    variable KHIMDir
    variable inputComposeKey
    set w [winfo parent $parent].composeKey
    toplevel $w -class dialog
    wm withdraw $w
    wm geometry $w +[winfo rootx $parent]+[winfo rooty $parent]
    set p [winfo toplevel [winfo parent $w]]
    set g [wm transient $p]
    if { ![string compare {} $g] } {
	set g $p
    }
    wm transient $w $g
    catch {wm attributes $w -toolwindow 1}
    wm title $w [mc "Compose Key"]
    set text [mc "SELECT COMPOSE KEY"]
    if {$text eq "SELECT COMPOSE KEY"} {
	# This must be a version of Tcl that doesn't support the root
	# locale.  Do The Right Thing anyway
	set locale [::msgcat::mclocale]
	::msgcat::mclocale en
	set text [string trim [mc "SELECT COMPOSE KEY"]]
	if {$text eq "SELECT COMPOSE KEY"} {
	    ::msgcat::mcload $KHIMDir
	    set text [string trim [mc "SELECT COMPOSE KEY"]]
	}
	::msgcat::mclocale $locale
    }
    grid [label $w.l -text $text]
    bind $w.l <Any-Key> [list set ::khim::inputComposeKey %K]
    bind $w.l <Map> [list focus %W]
    wm resizable $w 0 0
    bind $w <Map> [list grab $w]
    wm deiconify $w
    bind $w <Destroy> {set ::khim::inputComposeKey DESTROYED}
    set holdInputComposeKey $inputComposeKey
    while {1} {
	vwait ::khim::inputComposeKey
	if { $inputComposeKey eq {DESTROYED} } {
	    set inputComposeKey $holdInputComposeKey
	    break
	} elseif {$inputComposeKey ne {}} {
	    bind $w <Destroy> {}
	    after idle [list destroy $w]
	    break
	}
    }
    return
}

#----------------------------------------------------------------------
#
# khim::Select --
#
#	Handles selection in the listbox containing KHIM input
#	character sequences.
#
# Parameters:
#	lb -- Path name of the listbox.
#
# Results:
#	None.
#
# Side effects:
#	Stores the currently selected sequence, and its mapping,
#	in "inputSequence" and "inputCharacter."
#
#----------------------------------------------------------------------

proc khim::Select {lb} {
    variable inputSequence
    variable inputCharacter
    foreach item [$lb curselection] {
	if { [regexp "^(..) \u2192 (.)" [$lb get $item] \
		  -> inputSequence inputCharacter] } {
	    break
	}
    }
    return
}

#----------------------------------------------------------------------
#
# khim::DeleteSequence --
#
#	Deletes the currently selected input sequence from the set.
#
# Parameters:
#	w - Path name of the active dialog box.
#
# Results:
#	None.
#
# Side effects:
#	Removes the currently selected sequence from 'inputMap'
#	and redisplays the sequences in the listbox
#
#----------------------------------------------------------------------

proc khim::DeleteSequence {w} {
    khim::SetSequence $w {}
    return
}

#----------------------------------------------------------------------
#
# khim::ChangeSequence --
#
#	Changes the currently selected input sequence from the set.
#
# Parameters:
#	w - Path name of the active dialog box.
#
# Results:
#	None.
#
# Side effects:
#	Changes the currently selected sequence from 'inputMap'
#	to request the character stored in 'inputCharacter'
#	and redisplays the sequences in the listbox
#
#----------------------------------------------------------------------

proc khim::ChangeSequence {w} {
    variable inputCharacter
    khim::SetSequence $w $inputCharacter
    return
}

#----------------------------------------------------------------------
#
# khim::SetSequence --
#
#	Deletes or changes a character sequence in the input map
#
# Parameters:
#	w - Path name of the active dialog box
#	inputCharacter - Character that the active sequence should
#		         map to.  An empty string deletes the sequence.
#
# Results:
#	None.
#
# Side effects:
#	Changes the currently selected sequence from 'inputMap'
#	to request the character stored in 'inputCharacter'
#	and redisplays the sequences in the listbox
#
#----------------------------------------------------------------------

proc khim::SetSequence {w inputCharacter} {
    variable inputSequence
    variable inputMap
    if { [string length $inputSequence] != 2 } {
	tk_messageBox \
	    -message [mc {Composed sequence must be two characters long}] \
	    -type ok \
	    -icon error \
	    -parent $w \
	    -title [mc {Invalid sequence}]
    } elseif { [string length $inputCharacter] == 0 } {
	catch { unset inputMap($inputSequence) }
	ShowSequences $w
    } else {
	set inputMap($inputSequence) $inputCharacter
	ShowSequences $w $inputSequence
    }
    return
}

#----------------------------------------------------------------------
#
# khim::ShowSequences --
#
#	Updates the listbox in the KHIM configuration dialog with
#	the currently defined input sequences.
#
# Parameters:
#	w -- Path name of the active dialog
#	inputSequence -- Input sequence that has been changed, if any.
#
# Results:
#	None.
#
# Side effects:
#	Listbox is updated to reflect change, and the active sequence
#	is selected.
#
#----------------------------------------------------------------------

proc khim::ShowSequences {w {inputSequence {}}} {
    variable inputMap

    # Remember the scroll position

    foreach {top bottom} [$w.lf1.lb yview] break
    
    # Clear the listbox

    $w.lf1.lb delete 0 end

    # Put all the items back in the listbox, in order.
    # Remember the index of any item that matches the current sequence.

    foreach key [lsort -dictionary [array names inputMap]] {
	if { ![string compare $key $inputSequence] } {
	    set idx [$w.lf1.lb index end]
	}
	$w.lf1.lb insert end "$key \u2192 $inputMap($key)"
    }

    # Select the just-changed item, if any.  If there is nothing to select,
    # simply restore the scroll position.

    if { [info exists idx] } {
	$w.lf1.lb selection set $idx
	$w.lf1.lb see $idx
    } else {
	$w.lf1.lb yview moveto $top
    }
    return
}

#----------------------------------------------------------------------
#
# khim::Apply --
#
#	Apply changes from the KHIM configuration dialog.
#
# Parameters:
#	w - Path name of the dialog
#
# Results:
#	None.
#
# Side effects:
#	Current configuration is stored, and bindings to the KHIM
#	bindtag are applied.
#
#----------------------------------------------------------------------

proc khim::Apply { w } {
    variable use
    variable composeKey
    variable map
    variable inputUse
    variable inputComposeKey
    variable inputMap

    set use $inputUse
    set composeKey $inputComposeKey
    set map [array get inputMap]
    RedoBindings

    return
}

#----------------------------------------------------------------------
#
# khim::OK --
#
#	Apply changes and dismiss the KHIM configuration dialog.
#
# Parameters:
#	w - Path name of the dialog
#
# Results:
#	None.
#
# Side effects:
#	Current configuration is stored, and bindings to the KHIM
#	bindtag are applied.  The dialog is dismissed.
#
#----------------------------------------------------------------------

proc khim::OK { w } {
    Apply $w
    destroy $w
}

#----------------------------------------------------------------------
#
# khim::HandleDestroy --
#
#	Clean up from destruction of the KHIM input dialog.
#
# Parameters:
#	w - Path name of the destroyed window
#	t - Path name of the toplevel of the active dialog.
#
# Results:
#	None.
#
# Side effects:
#	Unsets variables that are used only when the dialog is active.
#
#----------------------------------------------------------------------

proc khim::HandleDestroy { w t } {
    if { [string compare $w $t] } return
    variable inputComposeKey
    variable inputMap
    variable inputUse
    unset inputUse
    unset inputComposeKey
    unset inputMap
    return
}

#----------------------------------------------------------------------
#
# khim::RedoBindings --
#
#	Establish bindings on the KHIM bindtag according to the current
#	settings.
#
# Parameters:
#	None.
#
# Results:
#	None.
#
# Side effects:
#	Binds the Compose key to a {break}, the leading character
#	of each two-character sequence to a break as well, and
#	the second character of each two character sequence to
#	insert the mapped character. Arranges so that unrecognized
#	two-character sequences insert the two individual characters.
#
#----------------------------------------------------------------------

proc khim::RedoBindings {} {
    variable use
    variable composeKey
    variable map
    foreach b [bind KHIM] {
	bind KHIM $b {}
    }
    if { $use } {
	bind KHIM <Key-$composeKey> break
	bind KHIM <Key-$composeKey><Key-$composeKey> {
	    khim::CMapInteractor %W
	}
	foreach {seq char} $map {
	    set c0 [string map {{ } <space> < <less>} [string index $seq 0]]
	    set c1 [string map {{ } <space> < <less>} [string index $seq 1]]
	    bind KHIM <Key-$composeKey>$c0 break
	    bind KHIM <Key-$composeKey>$c0<Key> \
		[list khim::BadCompose %W [string index $seq 0] %A]
	    bind KHIM <Key-$composeKey>$c0$c1 \
		[list khim::Insert %W $char]\;break
	}
    }
    return
}

#----------------------------------------------------------------------
#
# khim::BadCompose --
#
#	Handle an unrecognized key sequence
#
# Parameters:
#	w - Focus window
#	c0 - First character in the sequence
#	c1 - Second character in the sequence, or an empty string if
#	     there is no second character
#
# Results:
#	None
#
# Side effects:
#	Inserts the two individual characters into the focus window.
#
#----------------------------------------------------------------------

proc khim::BadCompose {w c0 c1} {
    if {$c1 ne {}} {
	khim::Insert $w $c0
	khim::Insert $w $c1
    }
    return -code break
}

#----------------------------------------------------------------------
#
# khim::Insert --
#
#	Inserts a character into a text or entry.
#
# Parameters:
#	w - Window in which to insert
#	c - Character to insert
#
# Results:
#	None.
#
# Side effects:
#	Character is inserted.
#
#----------------------------------------------------------------------

proc khim::Insert {w c} {
    $w insert insert $c
    switch -exact [winfo class $w] {
	Entry - TEntry {
	    set c [$w index insert]
	    if {($c < [$w index @0]) || ($c > [$w index @[winfo width $w]])} {
		$w xview $c
	    }
	}
	Text {
	    $w see insert
	}
    }
}

#----------------------------------------------------------------------
#
# khim::getConfig --
#
#	Returns a script that will restore the current KHIM configuration.
#
# Results:
#	Returns the script.
#
#----------------------------------------------------------------------

proc khim::getConfig {} {
    variable use
    variable composeKey
    variable map
    array set x $map
    set retval [list khim::setConfig 1.0 $use $composeKey]
    append retval { } \{
    foreach key [lsort -dictionary [array names x]] {
	append retval \n {    } [list $key] { } [ReplaceU $x($key)]
    }
    append retval \n\}
}

#----------------------------------------------------------------------
#
# khim::setConfig --
#
#	Restores the saved configuration from "khim::getConfig"
#
# Parameters:
#	version - Version of the configuration command
#	u - Flag for whether KHIM is enabled
#	c - Compose key selected
#	m - Map from compose sequences to characters.
#
# Results:
#	None
#
# Side effects:
#	Configuration is set.
#
#----------------------------------------------------------------------

proc khim::setConfig {v u c m args} {
    variable use
    variable composeKey
    variable map
    switch -exact $v {
	1.0 {
	    set use $u
	    set composeKey $c
	    set map $m
	}
	default {
	    return -code error "Unknown KHIM version $v"
	}
    }
    RedoBindings
    return
}

#----------------------------------------------------------------------
#
# khim::ReplaceU --
#
#	Replaces non-ASCII characters in a Unicode string with \u escapes.
#
# Parameters:
#	s - String to clean up
#
# Results:
#	Returns the cleaned string.
#
#----------------------------------------------------------------------

proc khim::ReplaceU {string} {
    set retval {}
    foreach char [split $string {}] {
	scan $char %c ccode
	if { $ccode >= 0x0020 && $ccode < 0x007f
	     && $char ne "\{" && $char ne "\}" && $char ne "\["
	     && $char ne "\]" && $char ne "\\" && $char ne "\$" } {
	    append retval $char
	} else {
	    append retval \\u [format %04x $ccode]
	}
    }
    return $retval
}

#----------------------------------------------------------------------
#
# khim::CMapUpdateSpinbox --
#
#	Variable trace callback that manages the state of the
#	code page selection spinbox when the code page changes.
#
# Parameters:
#	w - Window path name of the character map dialog
#	args - Extra args from the 'trace' mechanism are not used here.
#
# Results:
#	None.
#
# Side effects:
#	If the CMapInputCodePage variable contains an invalid code
#	page number, the background of the spinbox changes to red.
#	Otherwise, the background of the spinbox changes to white.
#       The values list of the spinbox is updated to be a list of
#	the decimal or hexadecimal code page numbers according to
#	whether the variable's string representation contains
#	'0x'. 
#
#----------------------------------------------------------------------

proc khim::CMapUpdateSpinbox {w args} {
    variable CMapInputCodePage
    variable CMapCodePage
    variable CMapSavedColors

    set spin $w.spin

    # Test validity of the code page number

    if { ![string is integer -strict $CMapInputCodePage($w)]
	 || $CMapInputCodePage($w) < 0
	 || $CMapInputCodePage($w) >= 0x100 } {
	if {![info exists CMapSavedColors($w)]} {
	    set CMapSavedColors($w) \
		[list [$spin cget -background] [$spin cget -foreground]]
	}
	$spin configure -background \#ff6666 -foreground \#000000
    } else {

	# Valid code page - generate the values list. Make sure that
	# the current value is in the list, even if it's formatted
	# eccentrically (e.g., 0x000012).

	if {[info exists CMapSavedColors($w)]} {
	    foreach {bg fg} $CMapSavedColors($w) break
	    $spin configure -background $bg -foreground $fg
	    unset CMapSavedColors($w)
	}
	if { [string match *0x* $CMapInputCodePage($w)] } {
	    set format 0x%02X
	} else {
	    set format %d
	}
	for { set i 0 } { $i < $CMapInputCodePage($w) } { incr i } {
	    lappend values [format $format $i]
	}
	lappend values $CMapInputCodePage($w)
	for { incr i } { $i < 0x100 } { incr i } {
	    lappend values [format $format $i]
	}

	# When we change the values list, the content of the spinbox
	# appears to be lost; deal with this by saving and restoring it.

	set cp $CMapInputCodePage($w)
	set i [$spin index insert]
	$spin configure -values $values
	$spin set $cp
	$spin icursor $i
	set CMapCodePage($w) $CMapInputCodePage($w)
    }
    return
}

#----------------------------------------------------------------------
#
# khim::CMapDrawCanvas --
#
#	Puts a map of a single Unicode code page into a canvas.
#
# Parameters:
#	w -- Path name of the character map dialog
#	args -- Additional arguments resulting from a 'trace' callback
#
# Results:
#	None.
#
# Side effects:
#	The given canvas is redrawn with a 16x16 grid of characters.
#
#----------------------------------------------------------------------

proc khim::CMapDrawCanvas {w args} {
    variable CMapCodePage
    variable CMapInputCodePage
    variable CMapFont
    variable CMapBadCharFont
    variable CMapXL
    variable CMapYL
    variable CMapSelectedCharacter
    variable CMapAfter
    variable CMapBackground
    variable CMapForeground

    if {[info exists CMapAfter($w)]} {
	after cancel $CMapAfter($w)
	unset CMapAfter($w)
    }

    set c $w.c

    set pad 2

    # Clear the canvas

    $c delete all

    set minsize [CMapCellSize $c]

    # Drop glyphs for all the characters onto the canvas, stacking them
    # all at (0,0).  We'll be sliding them by rows and columns to make the
    # grid.

    set rem [expr { $CMapSelectedCharacter($w) % 0x0100 }]
    set srow [expr { $rem / 16 }]
    set scol [expr { $rem % 16 }]
    set tick [clock clicks -milliseconds]
    set ok 1
    for { set row 0 } { $row < 16 } { incr row } {
	for { set col 0 } { $col < 16 } { incr col } {
	    set point [expr { 256 * $CMapCodePage($w) + 16 * $row + $col }]
	    if { ($ok || ($row == $srow && $col == $scol))
		 && [ValidChar $point] } {
		set t [format %c $point]
		set f $CMapFont
	    } else {
		set t [format %02X\n%02X \
			   [expr { $point / 0x100 }] [expr { $point % 0x100 }]]
		set f $CMapBadCharFont
	    }
	    set tags [list text row$row col$col]
	    $c create text 0 0 -text $t -font $f -fill $CMapForeground($w)\
		-anchor center -justify center -tags $tags
	    set tock [clock clicks -milliseconds]
	    if {$ok && $tock-$tick > 1500} {
		set CMapAfter($w) [after 500 [list khim::CMapDrawCanvas $w]]
		set ok 0
	    }
	}
    }

    # Spread out the columns and generate a list of the X co-ordinates
    # of the spacer lines

    set xmin [expr {$pad + 1}]
    set x $xmin
    set CMapXL($w) [list $x]
    for { set col 0 } { $col < 16 } { incr col } {
	foreach { x0 - x1 - } [$c bbox col$col] break
	set cw [expr { $x1 - $x0 + 5 }]
	if { $cw < $minsize } {
	    set cw $minsize
	}
	set xt [expr { $x + $cw/2 }]
	set dx [expr { $xt - ( $x0 + $x1 ) / 2 }]
	$c move col$col $dx 0
	incr x $cw
	lappend CMapXL($w) $x
    }
    set xmax $x

    # Now do the same with the rows

    set ymin [expr {$pad + 1}]
    set y $ymin
    set CMapYL($w) [list $y]
    for { set row 0 } { $row < 16 } { incr row } {
	foreach {  - y0 - y1 } [$c bbox row$row] break
	set rh [expr { $y1 - $y0 + 5 }]
	if { $rh < $minsize } {
	    set rh $minsize
	}
	set yt [expr { $y + $rh/2 }]
	set dy [expr { $yt - ( $y0 + $y1 ) / 2 }]
	$c move row$row 0 $dy
	incr y $rh
	lappend CMapYL($w) $y
    }
    set ymax $y

    # Now that the characters on the grid are properly positioned, draw
    # the separator lines and configure the canvas size

    # We interpolate between foreground and background to draw the lines,
    # so that they appear "finer" visually than a 0-pixel line
    
    set linecolor \#
    foreach \
	c1 [winfo rgb $c $CMapForeground($w)] \
	c2 [winfo rgb $c $CMapBackground($w)] {
	    set c3 [expr {(3 * $c2 + $c1) / 4}]
	    append linecolor [format %04x $c3]
	}
    foreach x $CMapXL($w) {
	$c create line $x $ymin $x $ymax -width 0 -fill $linecolor
    }
    foreach y $CMapYL($w) {
	$c create line $xmin $y $xmax $y -width 0 -fill $linecolor
    }
    $c configure -width [expr { $xmax + $pad }] \
	-height [expr { $ymax + $pad }] \
	-scrollregion [list 0 0 [expr {$xmax + $pad}] [expr {$ymax + $pad}]]

    # Change the codepage in the spinbox

    if { $CMapCodePage($w) != $CMapInputCodePage($w) } {
	set CMapInputCodePage($w) $CMapCodePage($w)
    }

    # Display a selection box

    ShowSelectedCell $w
}

#----------------------------------------------------------------------
#
# khim::CMapCellSize --
#
#	Computes the size of one cell in the character map
#
# Parameters:
#	c - canvas in which the map will be drawn.
#
# Results:
#	Returns the size in pixels of one square cell in the canvas.
#
#----------------------------------------------------------------------

proc khim::CMapCellSize {c} {

    variable CMapFont
    variable CMapBadCharFont

    # Compute the minimum linear dimension of one box in the grid.
    # It is at least 5 pxl greater than
    #   - the linespace of the display font
    #   - 2-line space in the "bad character" font
    #   - one em in the display font
    #   - two digit widths in the "bad character" font

    set minsize \
	[expr { [font metrics $CMapFont -displayof $c -linespace] + 5 }]
    set minsize2 [expr { 2 * [font metrics $CMapBadCharFont \
				  -displayof $c -linespace] + 5 }]
    if { $minsize2 > $minsize } {
	set minsize $minsize2
    }
    set minsize2 [expr { [font measure $CMapFont -displayof $c M] + 5 }]
    if { $minsize2 > $minsize } {
	set minsize $minsize2
    }
    set minsize2 [expr { [font measure $CMapBadCharFont -displayof $c 00] + 5 }]
    if { $minsize2 > $minsize } {
	set minsize $minsize2
    }
    return $minsize
}

#----------------------------------------------------------------------
#
# khim::ShowSelectedCell --
#
#	Paints a border around the cell in the KHIM character map
#	corresponding to the selected character
#
# Parameters:
#	w - Path name of the character map
#
# Results:
#	None.
#
#----------------------------------------------------------------------

proc khim::ShowSelectedCell {w} {
    variable CMapCodePage
    variable CMapSelectedCharacter
    variable CMapXL
    variable CMapYL
    variable CMapBackground
    variable CMapForeground
    variable CMapSelectBackground
    variable CMapSelectForeground
    if { $CMapSelectedCharacter($w) < $CMapCodePage($w) * 0x0100
	 || $CMapSelectedCharacter($w) >= ($CMapCodePage($w) + 1) * 0x100 } {
	set CMapSelectedCharacter($w) \
	    [expr { ($CMapSelectedCharacter($w) % 0x100)
		    + (0x100 * $CMapCodePage($w)) }]
    }
    set c $w.c
    set rem [expr { $CMapSelectedCharacter($w) % 0x0100 }]
    set row [expr { $rem / 16 }]
    set col [expr { $rem % 16 }]

    $c itemconfigure text -fill $CMapForeground($w)
    $c itemconfigure text&&row$row&&col$col -fill $CMapSelectForeground($w)

    set xmin [lindex $CMapXL($w) $col]
    incr col
    set xmax [lindex $CMapXL($w) $col]

    set ymin [lindex $CMapYL($w) $row]
    incr row
    set ymax [lindex $CMapYL($w) $row]
    catch { $c delete selectrect }
    $c create rectangle $xmin $ymin $xmax $ymax \
	-width 2 -fill $CMapSelectBackground($w) \
	-outline $CMapSelectForeground($w) -tags selectrect
    $c lower selectrect text
    return
}

#----------------------------------------------------------------------
#
# khim::CMapSelectedCharacter --
#
#	Given X and Y co-ordinates in the character map, determines
#	what character is selected.
#
# Parameters:
#	c - The canvas displaying the map.
#
# Results:
#	Returns the character, or an empty string if the co-ordinates
#	do not designate a cell.
#
#----------------------------------------------------------------------

proc khim::CMapSelectedCharacter {w x y} {
    variable CMapCodePage
    variable CMapXL
    variable CMapYL
    set row [BSearch $CMapYL($w) $y]
    set col [BSearch $CMapXL($w) $x]
    if { $row >= 0 && $row <= 15 && $col >= 0 && $col <= 15 } {
	return [format %c [expr { 0x100 * $CMapCodePage($w) 
				  + 0x10 * $row 
				  + $col }]]
    } else {
	return {}
    }
}

#----------------------------------------------------------------------
#
# khim::CMapSelect --
#
#	Handles mouse selection in the KHIM color map
#
# Parameters:
#	c - Path name of the canvas
#	x, y - Mouse coordinates relative to the canvas
#
# Results:
#	None
#
# Side effects:
#	Character in the cell containing the pointer is selected, and
#	the display of the selection is updated.
#
#----------------------------------------------------------------------

proc khim::CMapSelect {c x y} {
    variable CMapSelectedCharacter
    set w [khim::CMapCanvToDialog $c]
    set ch [khim::CMapSelectedCharacter $w $x $y]
    if { $ch ne {} } {
	scan $ch %c CMapSelectedCharacter($w)
    }
    ShowSelectedCell $w
    return
}

#----------------------------------------------------------------------
#
# khim::CMapActivate --
#
#	Activates the KHIM character map after a mouse selection.
#
# Parameters:
#	c - Path name of the canvas
#	x, y - Mouse coordinates relative to the canvas
#
# Results:
#	None
#
# Side effects:
#	Directs focus to the canvas, and selects the character designated
#	by the pointer.
#
#----------------------------------------------------------------------

proc khim::CMapActivate {c x y} {
    focus $c
    khim::CMapSelect $c $x $y
    return
}

#----------------------------------------------------------------------
#
# khim::CMapHomeEnd --
#
#	Handles the Home and End keys in the KHIM character map
#
# Parameters:
#	c - Path name of the canvas
#	unit - Unit being homed (word, page, file)
#	key - 1 for End, 0 for Home
#
# Results:
#	None.
#
# Side effects:
#	Moves the selection according to the key pressed.
#
#----------------------------------------------------------------------

proc khim::CMapHome {c unit key} {
    variable CMapSelectedCharacter
    set w [khim::CMapCanvToDialog $c]
    set sc [expr { $unit * ($CMapSelectedCharacter($w) / $unit)
		   + $key * ($unit - 1) }]
    khim::CMapMoveTo $c $sc
    return
}

#----------------------------------------------------------------------
#
# khim::CMapMove --
#
#	Handles several cursor keys (Left, Right, Up, Down, PgUp,
#	PgDn) in the KHIM character map.
#
# Parameters:
#	c - Path name of the canvas
#	delta - Number of code points to move
#
# Results:
#	None.
#
# Side effects;
#	Moves the selection by the designated number of codepoints.
#
#----------------------------------------------------------------------

proc khim::CMapMove {c delta} {
    variable CMapSelectedCharacter
    set w [khim::CMapCanvToDialog $c]
    set sc [expr { $CMapSelectedCharacter($w) + $delta }]
    if { $sc < 0 } {
	set sc 0
    } elseif { $sc > 0xffff } {
	set sc 0xffff
    }
    khim::CMapMoveTo $c $sc
    return
}

#----------------------------------------------------------------------
#
# khim:CMapMoveTo --
#
#	Changes the selection in the KHIM character map to a specified
#	codepoint.
#
# Parameters:
#	c - Path name of the canvas
#	sc - Code point to select, expressed as an integer
#
# Results:
#	None
#
# Side effects:
#	Moves the selection to the given character.
#
#----------------------------------------------------------------------

proc khim::CMapMoveTo { c sc } {
    variable CMapSelectedCharacter
    variable CMapCodePage
    set w [khim::CMapCanvToDialog $c]
    set cp [expr { $sc / 0x0100 }]
    set CMapSelectedCharacter($w) $sc
    if { $cp != $CMapCodePage($w) } {
	set CMapCodePage($w) $cp
    } else {
	ShowSelectedCell $w
    }
    return
}

#----------------------------------------------------------------------
#
# CMapKey --
#
#	Handles non-cursor keypresses in the KHIM character map
#
# Parameters:
#	c - Path name of the canvas
#	char - Character sent by the key
#
# Results:
#	None.
#
# Side effects:
#	Selects the given character
#
#----------------------------------------------------------------------

proc khim::CMapKey {c char} {
    if {$char eq {}} return;		# If the key doesn't generate a char,
					# ignore it.
    scan $char %c sc
    CMapMoveTo $c $sc
    return
}

#----------------------------------------------------------------------
#
# khim::CMapWheel --
#
#	Handles the mousewheel in the KHIM character map
#
# Parameters:
#	c - Path name of the canvas
#	delta - Amount by which the canvas is to move.
#
# Return value:
#	None.
#
# Side effects:
#	Adjusts the selection by an appropriately scaled version of 'delta'
#	
#----------------------------------------------------------------------

proc khim::CMapWheel { c delta shifted } {
    # the delta will vary for OS X and X11/Win32, but we only check
    # + or - and move accordingly
    if {$delta > 0} {
	khim::CMapMove $c [expr {$shifted ? -1 : -16}]
    } else {
	khim::CMapMove $c [expr {$shifted ? 1 : 16}]
    }
    return
}

#----------------------------------------------------------------------
#
# khim::CMapCanvToDialog --
#
#	Locates the KHIM character map dialog given the widget path
#	name of the canvas.
#
# Parameters:
#	c - Path name of the canvas
#
# Results:
#	Returns the path name of the dialog.
#
#----------------------------------------------------------------------

proc khim::CMapCanvToDialog {c} {
    return [winfo parent $c]
}

#----------------------------------------------------------------------
#
# khim::CMapInteractor --
#
#	Posts the KHIM character map for interacting with the user.
#
# Parameters:
#	w - Path name of the text or canvas widget to which the
#	    interactor applies.
#
# Results:
#	None.
#
# Side effects:
#	Interactor is posted, and the event loop is entered recursively
#	to handle it.  On return, any requested symbol insertion has
#	already been done.
#
#----------------------------------------------------------------------

proc khim::CMapInteractor {w} {
    variable CMapSelectedCharacter
    variable CMapInputCodePage
    variable CMapCodePage
    variable CMapFocus
    variable CMapBackground
    variable CMapForeground
    variable CMapSelectBackground
    variable CMapSelectForeground
    set t [winfo toplevel $w]
    if { $t eq "." } {
	set t {}
    }
    set map $t.khimcmap
    if {[winfo exists $map]} {
	wm deiconify $map
	return
    }
    toplevel $map
    wm withdraw $map
    wm title $map [mc {Insert Character}]

    if { ![info exists CMapInputCodePage($map)] } {
	set CMapInputCodePage($map) 0
	set CMapCodePage($map) 0
    }
    grid [label $map.l1 -text [mc {Select code page:}]] \
	-row 0 -column 0 -sticky e
    grid [spinbox $map.spin -textvariable khim::CMapInputCodePage($map) \
	      -width 4] \
	-row 0 -column 1 -sticky w

    # Get canvas background from the background that a text widget would
    # have had.
    text $map.text
    set CMapBackground($map) [$map.text cget -background]
    set CMapForeground($map) [$map.text cget -foreground]
    set CMapSelectBackground($map) [$map.text cget -selectbackground]
    set CMapSelectForeground($map) [$map.text cget -selectforeground]
    destroy $map.text

    # Create the dialog
    set c $map.c
    grid [canvas $c -width 400 -height 400 \
	      -bg $CMapBackground($map) -takefocus 1] \
	-columnspan 2 -padx 3 -pady 3
    grid [frame $map.f] -row 2 -column 0 -columnspan 2 -sticky ew -pady 3
    button $map.f.b1 -text [mc OK] -command [list khim::CMapOK $map]
    button $map.f.b2 -text [mc Cancel] -command [list khim::CMapCancel $map]
    button $map.f.b3 -text [mc Help...] \
	-command [list khim::showHelp $map.help]
    grid $map.f.b1 -row 0 -column 0 -sticky ew -padx 5
    grid $map.f.b2 -row 0 -column 1 -sticky ew -padx 5
    grid $map.f.b3 -row 0 -column 3 -sticky ew -padx 5
    grid columnconfigure $map.f 2 -weight 1
    grid columnconfigure $map.f {0 1 3} -uniform A
    grid columnconfigure $map 1 -weight 1

    bindtags $c [list $c khim::cmap Canvas [winfo toplevel $c] all]
    trace add variable ::khim::CMapInputCodePage($map) write \
	[list khim::CMapUpdateSpinbox $map]
    after idle [list khim::CMapUpdateSpinbox $map]
    trace add variable ::khim::CMapCodePage($map) write \
	[list khim::CMapDrawCanvas $map]
    if { ![info exists CMapSelectedCharacter($map)] } {
	set CMapSelectedCharacter($map) 0x0000
    }
    set CMapFocus($map) $w

    # Draw the character map in the canvas
    CMapDrawCanvas $map

    wm deiconify $map
    bind $map <Map> [list grab $map]
    bind $map.c <Map> [list focus %W]

    # eeew, tkwait... make this interaction modal
    tkwait window $map
    catch {
	destroy $map
    }
    focus $w
    return
}    

#----------------------------------------------------------------------
#
# khim::CMapCopypastedismiss --
#
#	Handles double-click in the KHIM character map.
#
# Parameters:
#	c - Path name of the canvas
#	x,y - Mouse co-ordinates of the double click
#
# Results:
#	None.
#
# Side effects:
#	Copies the designated character into the text or entry
#	that KHIM is using, and dismisses the widget.
#
#----------------------------------------------------------------------

proc khim::CMapCopypastedismiss {c x y} {
    CMapSelect $c $x $y
    CMapOK $c
    return
}

#----------------------------------------------------------------------
#
# khim::CMapOK
#
#	Handles the 'OK' button in the KHIM character map.
#
# Parameters:
#	w - Path name of the dialog
#
# Results:
#	None.
#
# Side effects:
#	Copies the selected character into the text or entry
#	that KHIM is using, and dismisses the widget.
#
#----------------------------------------------------------------------

proc khim::CMapOK {w} {
    CMapCopy $w
    CMapPasteToFocus $w
    CMapCancel $w
    return
}

#----------------------------------------------------------------------
#
# khim::CMapCopy --
#
#	Copies a character from the KHIM character map onto the
#	clipboard.
#
# Parameters:
#	w - Path name of the dialog.
#
# Results:
#	None.
#
# Side efffects:
#	Copies the selected character to the clipboard.
#
#----------------------------------------------------------------------

proc khim::CMapCopy {w} {
    variable CMapSelectedCharacter
    clipboard clear -displayof $w
    upvar 0 CMapSelectedCharacter([winfo toplevel $w]) ch
    if { [info exists ch] && $ch ne {} } {
	clipboard append -displayof $w -- [format %c $ch]
    }
    return
}

#----------------------------------------------------------------------
#
# khim::CMapPasteToFocus --
#
#	Sends a <<Paste>> event into the window on whose behalf
#	the KHIM character map was invoked, to copy a character selection
#	into it.
#
# Parameters:
#	w - Path name of the character map dialog.
#
# Results:
#	None.
#
# Side effects:
#	<<Paste>> is generated.
#
#----------------------------------------------------------------------

proc khim::CMapPasteToFocus {w} {
    variable CMapFocus
    event generate $CMapFocus([winfo toplevel $w]) <<Paste>>
    return
}

#----------------------------------------------------------------------
#
# khim::CMapCancel --
#
#	Handles the 'Cancel' button in the KHIM character map.
#
# Parameters:
#	w - Path name of the character map dialog.
#
# Results:
#	None.
#
# Side effects:
#	Destroys the dialog without taking further action.
#
#----------------------------------------------------------------------

proc khim::CMapCancel {w} {
    destroy [winfo toplevel $w]
}

#----------------------------------------------------------------------
#
# khim::CMapDestroy --
#
#	Handles the <Destroy> notification in the KHIM character map
#
# Parameters:
#	c - Path name of the character map canvas
#
# Results:
#	None.
#
# Side effects:
#	Cleans up memory for the destroyed widget.
#
#----------------------------------------------------------------------

proc khim::CMapDestroy {c} {
    variable CMapFocus
    variable CMapAfter
    variable CMapXL
    variable CMapYL
    variable CMapSavedColors
    variable CMapForeground
    variable CMapBackground
    variable CMapSelectForeground
    variable CMapSelectBackground
    set w [winfo toplevel $c]
    if {[info exists CMapAfter($w)]} {
	after cancel $CMapAfter($w)
	unset CMapAfter($w)
    }
    catch {unset CMapFocus($w)}
    catch {unset CMapXL($w)}
    catch {unset CMapYL($w)}
    catch {unset CMapSavedColors($w)}
    catch {unset CMapForeground($w)}
    catch {unset CMapSelectForeground($w)}
    catch {unset CMapBackground($w)}
    catch {unset CMapSelectBackground($w)}
    return
}
    
# Bindings for the "khim::cmap" bindtag that is used in the character map
# dialog

bind khim::cmap <1> {khim::CMapSelect %W %x %y}
bind khim::cmap <Double-1> {khim::CMapCopypastedismiss %W %x %y}
bind khim::cmap <B1-Motion> {khim::CMapSelect %W %x %y}
bind khim::cmap <ButtonRelease-1> {khim::CMapActivate %W %x %y}
bind khim::cmap <Up> {khim::CMapMove %W -16; break}
bind khim::cmap <Left> {khim::CMapMove %W -1; break}
bind khim::cmap <Right> {khim::CMapMove %W 1; break}
bind khim::cmap <Down> {khim::CMapMove %W 16; break}
bind khim::cmap <Next> {khim::CMapMove %W 0x100; break}
bind khim::cmap <Prior> {khim::CMapMove %W -0x100; break}
bind khim::cmap <Control-Home> {khim::CMapMoveTo %W 0x0000; break}
bind khim::cmap <Control-End> {khim::CMapMoveTo %W 0xffff; break}
bind khim::cmap <Shift-Home> {khim::CMapHome %W 0x0100 0; break}
bind khim::cmap <Shift-End> {khim::CMapHome %W 0x0100 1; break}
bind khim::cmap <Home> {khim::CMapHome %W 0x010 0; break}
bind khim::cmap <End> {khim::CMapHome %W 0x010 1; break}
bind khim::cmap <Key> {khim::CMapKey %W %A}
bind khim::cmap <<Cut>> {khim::CMapCopy %W}
bind khim::cmap <<Copy>> {khim::CMapCopy %W}
bind khim::cmap <space> {khim::CMapOK %W}
bind khim::cmap <Return> {khim::CMapOK %W}
bind khim::cmap <Escape> {khim::CMapCancel %W}
bind khim::cmap <MouseWheel> {khim::CMapWheel %W %D 0; break}
bind khim::cmap <Shift-MouseWheel> {khim::CMapWheel %W %D 1; break}
bind khim::cmap <Tab> {tk::TabToWindow [tk_focusNext %W]; break}
bind khim::cmap <<PrevWindow>> {tk::TabToWindow [tk_focusPrev %W]; break}
if { [string equal "x11" [tk windowingsystem]] } {
    bind khim::cmap <4> {khim::CMapWheel %W 120 0; break}
    bind khim::cmap <5> {khim::CMapWheel %W -120 0; break}
    bind khim::cmap <Shift-4> {khim::CMapWheel %W 120 1; break}
    bind khim::cmap <Shift-5> {khim::CMapWheel %W -120 1; break}
}
bind khim::cmap <Destroy> {khim::CMapDestroy %W}

# Set initial default configuration

khim::setConfig 1.0 1 Pause {
    !! \u00a1
    {"A} \u00c4
    {"a} \u00e4
    {"E} \u00cb
    {"e} \u00eb
    {"I} \u00cf
    {"i} \u00ef
    {"O} \u00d6
    {"o} \u00f6
    {"U} \u00dc
    {"u} \u00fc
    'A \u00c1
    'a \u00e1
    'E \u00c9
    'e \u00e9
    'I \u00cd
    'i \u00ed
    'O \u00d3
    'o \u00f3
    'U \u00da
    'u \u00fa
    'Y \u00dd
    'y \u00fd
    *A \u00c5
    *a \u00e5
    ,C \u00c7
    ,c \u00e7
    -> \u2192
    -L \u0141
    -l \u0142
    /O \u00d8
    /o \u00f8
    12 \u00bd
    13 \u2153
    14 \u00bc
    18 \u215b
    23 \u2154
    34 \u00be
    38 \u215c
    58 \u215d
    78 \u215e
    :( \u2639
    :) \u263a
    <- \u2190
    << \u00ab
    >> \u00bb
    ?? \u00bf
    ^A \u00c2
    ^a \u00e2
    ^E \u00ca
    ^e \u00ea
    ^I \u00ce
    ^i \u00ee
    ^O \u00d4
    ^o \u00f4
    ^U \u00db
    ^u \u00fb
    `A \u00c0
    `a \u00e0
    `E \u00c8
    `e \u00e8
    `I \u00cc
    `i \u00ec
    `O \u00d2
    `o \u00f2
    `U \u00d9
    `u \u00f9
    AA \u00c5
    aa \u00e5
    AE \u00c6
    ae \u00e6
    bu \u2022
    de \u00b0
    eu \u20ac
    LP \u2615
    mu \u00b5
    OE \u0152
    oe \u0153
    OC \u00a9
    OR \u00ae
    ss \u00df
    |c \u00a2
    ~A \u00c3
    ~a \u00e3
    ~N \u00d1
    ~n \u00f1
    ~O \u00d5
    ~o \u00f5
}

# Set initial bindings on the KHIM bind tag.

khim::RedoBindings

set khim::CMapFont [font create -family helvetica -size 15]
set khim::CMapBadCharFont [font create -family courier -size 8]

# Test program

if {[info exists ::argv0] && ![string compare $::argv0 [info script]]} {

    grid [entry .e -font {Courier 18}] -columnspan 5 -sticky ew
    .e insert end {Type here}
    bindtags .e {.e KHIM Entry . all}
    grid [button .test -text "Test" -command "khim::getOptions .khim"] \
	[button .bload -text "Load config" -command "testLoadConfig"] \
	[button .bsave -text "Save config" -command "testSaveConfig"] \
	[button .bhelp -text "Help" -command "khim::showHelp .help"] \
	[button .bquit -text "Quit" -command "exit"] \
	-padx 5 -pady 5
    
    proc testLoadConfig {} {
	source ~/.khimrc
    }
    proc testSaveConfig {} {
	set f [open ~/.khimrc w]
	puts $f [khim::getConfig]
	close $f
    }

}
