#!/bin/sh
# the next line restarts using wish \
exec wish "$0" ${1+"$@"}

#==============================================================================
# Demonstrates the use of embedded windows in tablelist widgets.
#
# Copyright (c) 2004-2010  Csaba Nemethi (E-mail: csaba.nemethi@t-online.de)
#==============================================================================

package require tablelist_tile 5.1

wm title . "Tile Library Scripts"

#
# Add some entries to the Tk option database
#
set dir [file dirname [info script]]
source [file join $dir option_tile.tcl]

#
# Create the font TkFixedFont if not yet present
#
catch {font create TkFixedFont -family Courier -size -12}

#
# Create an image to be displayed in buttons embedded in a tablelist widget
#
set openImg [image create photo -file [file join $dir open.gif]]

if {[tablelist::getCurrentTheme] eq "aqua"} {
    #
    # Work around the improper appearance of the tile scrollbars
    #
    interp alias {} ttk::scrollbar {} ::scrollbar
} else {
    #
    # Make the embedded buttons as small as possible.  Recall that in most
    # themes, the tile buttons consist of the following element hierarchy:
    #
    # Button.border
    #     Button.focus	      (one of its options is -focusthickness)
    #         Button.padding  (two of its options are -padding and -shiftrelief)
    #             Button.label
    #
    if {[info commands "::ttk::style"] ne ""} {
	interp alias {} styleConfig {} ttk::style configure
    } elseif {[string compare $tile::version "0.7"] >= 0} {
	interp alias {} styleConfig {} style configure
    } else {
	interp alias {} styleConfig {} style default
    }
    styleConfig Embedded.TButton -focusthickness 0 -padding 0 -shiftrelief 0
}

#
# Improve the window's appearance by using a tile
# frame as a container for the other widgets
#
set f [ttk::frame .f]

#
# Create a vertically scrolled tablelist widget with 5
# dynamic-width columns and interactive sort capability
#
set tbl $f.tbl
set vsb $f.vsb
tablelist::tablelist $tbl \
    -columns {0 "File Name" left
	      0 "Bar Chart" center
	      0 "File Size" right
	      0 "View"      center
	      0 "Seen"      center} \
    -setgrid no -yscrollcommand [list $vsb set] -width 0
if {[$tbl cget -selectborderwidth] == 0} {
    $tbl configure -spacing 1
}
$tbl columnconfigure 0 -name fileName
$tbl columnconfigure 1 -formatcommand emptyStr -sortmode integer
$tbl columnconfigure 2 -name fileSize -sortmode integer
$tbl columnconfigure 4 -name seen
ttk::scrollbar $vsb -orient vertical -command [list $tbl yview]

proc emptyStr val { return "" }

eval font create BoldFont [font actual [$tbl cget -font]] -weight bold

#
# Populate the tablelist widget
#
if {[info exists ttk::library]} {
    cd $ttk::library
} else {
    cd $tile::library
}
set maxFileSize 0
foreach fileName [lsort [glob *.tcl]] {
    set fileSize [file size $fileName]
    $tbl insert end [list $fileName $fileSize $fileSize "" no]

    if {$fileSize > $maxFileSize} {
	set maxFileSize $fileSize
    }
}

#------------------------------------------------------------------------------
# createFrame
#
# Creates a frame widget w to be embedded into the specified cell of the
# tablelist widget tbl, as well as a child frame representing the size of the
# file whose name is diplayed in the first column of the cell's row.
#------------------------------------------------------------------------------
proc createFrame {tbl row col w} {
    #
    # Create the frame and replace the binding tag "Frame"
    # with "TablelistBody" in the list of its binding tags
    #
    frame $w -width 102 -height 14 -background ivory -borderwidth 1 \
	     -relief solid
    bindtags $w [lreplace [bindtags $w] 1 1 TablelistBody]

    #
    # Create the child frame and replace the binding tag "Frame"
    # with "TablelistBody" in the list of its binding tags
    #
    frame $w.f -height 12 -background red -borderwidth 1 -relief raised
    bindtags $w.f [lreplace [bindtags $w] 1 1 TablelistBody]

    #
    # Manage the child frame
    #
    set fileSize [$tbl cellcget $row,fileSize -text]
    place $w.f -relwidth [expr {double($fileSize) / $::maxFileSize}]
}

#------------------------------------------------------------------------------
# createButton
#
# Creates a button widget w to be embedded into the specified cell of the
# tablelist widget tbl.
#------------------------------------------------------------------------------
proc createButton {tbl row col w} {
    set key [$tbl getkeys $row]
    ttk::button $w -style Embedded.TButton -image $::openImg -takefocus 0 \
		   -command [list viewFile $tbl $key]
}

#------------------------------------------------------------------------------
# viewFile
#
# Displays the contents of the file whose name is contained in the row with the
# given key of the tablelist widget tbl.
#------------------------------------------------------------------------------
proc viewFile {tbl key} {
    set top .top$key
    if {[winfo exists $top]} {
	raise $top
	return ""
    }

    toplevel $top
    set fileName [$tbl cellcget k$key,fileName -text]
    wm title $top "File \"$fileName\""

    #
    # Improve the window's appearance by using a tile
    # frame as a container for the other widgets
    #
    set f [ttk::frame $top.f]

    #
    # Create a vertically scrolled text widget as a child of the toplevel
    #
    set txt $f.txt
    set vsb $f.vsb
    text $txt -background white -font TkFixedFont -highlightthickness 0 \
	      -setgrid yes -yscrollcommand [list $vsb set]
    catch {$txt configure -tabstyle wordprocessor}		;# for Tk 8.5
    ttk::scrollbar $vsb -orient vertical -command [list $txt yview]

    #
    # Insert the file's contents into the text widget
    #
    set chan [open $fileName]
    $txt insert end [read $chan]
    close $chan

    set btn [ttk::button $f.btn -text "Close" -command [list destroy $top]]

    #
    # Manage the widgets
    #
    grid $txt -row 0 -column 0 -sticky news
    grid $vsb -row 0 -column 1 -sticky ns
    grid $btn -row 1 -column 0 -columnspan 2 -pady 10
    grid rowconfigure    $f 0 -weight 1
    grid columnconfigure $f 0 -weight 1
    pack $f -expand yes -fill both

    #
    # Mark the file as seen
    #
    $tbl rowconfigure k$key -font BoldFont
    $tbl cellconfigure k$key,seen -text yes
}

#------------------------------------------------------------------------------

#
# Create embedded windows in the columns no. 1 and 3
#
set rowCount [$tbl size]
for {set row 0} {$row < $rowCount} {incr row} {
    $tbl cellconfigure $row,1 -window createFrame -stretchwindow yes
    $tbl cellconfigure $row,3 -window createButton
}

set btn [ttk::button $f.btn -text "Close" -command exit]

#
# Manage the widgets
#
grid $tbl -row 0 -column 0 -sticky news
grid $vsb -row 0 -column 1 -sticky ns
grid $btn -row 1 -column 0 -columnspan 2 -pady 10
grid rowconfigure    $f 0 -weight 1
grid columnconfigure $f 0 -weight 1
pack $f -expand yes -fill both
