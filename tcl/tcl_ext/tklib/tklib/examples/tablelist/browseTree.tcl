#==============================================================================
# Demonstrates how to use a tablelist widget for displaying information about
# the children of an arbitrary widget.
#
# Copyright (c) 2010  Csaba Nemethi (E-mail: csaba.nemethi@t-online.de)
#==============================================================================

package require Tk 8.3
package require tablelist 5.1

namespace eval demo {
    variable dir [file dirname [info script]]

    #
    # Create two images, needed in the procedure putChildren
    #
    variable leafImg [image create bitmap -file [file join $dir leaf.xbm] \
		      -background coral -foreground gray50]
    variable compImg [image create bitmap -file [file join $dir comp.xbm] \
		      -background yellow -foreground gray50]
}

source [file join $demo::dir config.tcl]

#------------------------------------------------------------------------------
# demo::displayChildren
#
# Displays information on the children of the widget w in a tablelist widget
# contained in a newly created top-level widget.  Returns the name of the
# tablelist widget.
#------------------------------------------------------------------------------
proc demo::displayChildren w {
    if {![winfo exists $w]} {
	bell
	tk_messageBox -title "Error" -icon error -message \
	    "Bad window path name \"$w\""
	return ""
    }

    #
    # Create a top-level widget of the class DemoTop
    #
    set top .browseTop
    for {set n 2} {[winfo exists $top]} {incr n} {
	set top .browseTop$n
    }
    toplevel $top -class DemoTop

    #
    # Create a vertically scrolled tablelist widget with 9 dynamic-width
    # columns and interactive sort capability within the top-level
    #
    set tf $top.tf
    frame $tf
    set tbl $tf.tbl
    set vsb $tf.vsb
    tablelist::tablelist $tbl \
	-columns {0 "Path Name"	left
		  0 "Class"	left
		  0 "X"		right
		  0 "Y"		right
		  0 "Width"	right
		  0 "Height"	right
		  0 "Mapped"	center
		  0 "Viewable"	center
		  0 "Manager"	left} \
	-expandcommand demo::expandCmd -labelcommand demo::labelCmd \
	-yscrollcommand [list $vsb set] -setgrid no -width 0
    if {[$tbl cget -selectborderwidth] == 0} {
	$tbl configure -spacing 1
    }
    foreach col {2 3 4 5} {
	$tbl columnconfigure $col -sortmode integer
    }
    foreach col {6 7} {
	$tbl columnconfigure $col -formatcommand demo::formatBoolean
    }
    scrollbar $vsb -orient vertical -command [list $tbl yview]

    #
    # When displaying the information about the children of any
    # ancestor of the label widgets, the widths of some of the
    # labels and thus also the widths and x coordinates of some
    # children may change.  For this reason, make sure the items
    # will be updated after any change in the sizes of the labels
    #
    foreach l [$tbl labels] {
	bind $l <Configure> [list demo::updateItemsDelayed $tbl]
    }
    bind $tbl <Configure> [list demo::updateItemsDelayed $tbl]

    #
    # Create a pop-up menu with two command entries; bind the script
    # associated with its first entry to the <Double-1> event, too
    #
    set menu $top.menu
    menu $menu -tearoff no
    $menu add command -label "Display Children" \
		      -command [list demo::putChildrenOfSelWidget $tbl]
    $menu add command -label "Display Config" \
		      -command [list demo::dispConfigOfSelWidget $tbl]
    set bodyTag [$tbl bodytag]
    bind $bodyTag <Double-1>   [list demo::putChildrenOfSelWidget $tbl]
    bind $bodyTag <<Button3>>  [bind TablelistBody <Button-1>]
    bind $bodyTag <<Button3>> +[bind TablelistBody <ButtonRelease-1>]
    bind $bodyTag <<Button3>> +[list demo::postPopupMenu $top %X %Y]

    #
    # Create three buttons within a frame child of the top-level widget
    #
    set bf $top.bf
    frame $bf
    set b1 $bf.b1
    set b2 $bf.b2
    set b3 $bf.b3
    button $b1 -text "Refresh"
    button $b2 -text "Parent"
    button $b3 -text "Close" -command [list destroy $top]

    #
    # Manage the widgets
    #
    grid $tbl -row 0 -column 0 -sticky news
    grid $vsb -row 0 -column 1 -sticky ns
    grid rowconfigure    $tf 0 -weight 1
    grid columnconfigure $tf 0 -weight 1
    pack $b1 $b2 $b3 -side left -expand yes -pady 10
    pack $bf -side bottom -fill x
    pack $tf -side top -expand yes -fill both

    #
    # Populate the tablelist with the data of the given widget's children
    #
    putChildren $w $tbl root
    return $tbl
}

#------------------------------------------------------------------------------
# demo::putChildren
#
# Outputs the data of the children of the widget w into the tablelist widget
# tbl, as child items of the one identified by nodeIdx.
#------------------------------------------------------------------------------
proc demo::putChildren {w tbl nodeIdx} {
    #
    # The following check is necessary because this procedure
    # is also invoked by the "Refresh" and "Parent" buttons
    #
    if {![winfo exists $w]} {
	bell
	if {[string compare $nodeIdx "root"] == 0} {
	    set choice [tk_messageBox -title "Error" -icon warning \
			-message "Bad window path name \"$w\" -- replacing\
				  it with nearest existent ancestor" \
			-type okcancel -default ok \
			-parent [winfo toplevel $tbl]]
	    if {[string compare $choice "ok"] == 0} {
		while {![winfo exists $w]} {
		    set last [string last "." $w]
		    if {$last != 0} {
			incr last -1
		    }
		    set w [string range $w 0 $last]
		}
	    } else {
		return ""
	    }
	} else {
	    return ""
	}
    }

    if {[string compare $nodeIdx "root"] == 0} {
	set top [winfo toplevel $tbl]
	wm title $top "Children of the [winfo class $w] Widget \"$w\""

	$tbl resetsortinfo
	$tbl delete 0 end
	set row 0
    } else {
	set row [expr {$nodeIdx + 1}]
    }

    #
    # Display the data of the children of the
    # widget w in the tablelist widget tbl
    #
    variable leafImg
    variable compImg
    foreach c [winfo children $w] {
	#
	# Insert the data of the current child into the tablelist widget
	#
	set item {}
	lappend item \
		[winfo name $c] [winfo class $c] [winfo x $c] [winfo y $c] \
		[winfo width $c] [winfo height $c] [winfo ismapped $c] \
		[winfo viewable $c] [winfo manager $c]
	$tbl insertchild $nodeIdx end $item

	#
	# Insert an image into the first cell of the row; mark the
	# row as collapsed if the child widget has children itself
	#
	if {[llength [winfo children $c]] == 0} {
	    $tbl cellconfigure $row,0 -image $leafImg
	} else {
	    $tbl cellconfigure $row,0 -image $compImg
	    $tbl collapse $row
	}

	$tbl rowattrib $row pathName $c
	incr row
    }

    if {[string compare $nodeIdx "root"] == 0} {
	#
	# Configure the "Refresh" and "Parent" buttons
	#
	$top.bf.b1 configure -command [list demo::refreshView $w $tbl]
	set b2 $top.bf.b2
	set p [winfo parent $w]
	if {[string compare $p ""] == 0} {
	    $b2 configure -state disabled
	} else {
	    $b2 configure -state normal -command \
		[list demo::putChildren $p $tbl root]
	}
    }
}

#------------------------------------------------------------------------------
# demo::expandCmd
#
# Outputs the data of the children of the widget whose leaf name is displayed
# in the first cell of the specified row of the tablelist widget tbl, as child
# items of the one identified by row.
#------------------------------------------------------------------------------
proc demo::expandCmd {tbl row} {
    if {[$tbl childcount $row] == 0} {
	set w [$tbl rowattrib $row pathName]
	putChildren $w $tbl $row

	#
	# Apply the last sorting (if any) to the new items
	#
	$tbl refreshsorting $row
    }
}

#------------------------------------------------------------------------------
# demo::formatBoolean
#
# Returns "yes" or "no", according to the specified boolean value.
#------------------------------------------------------------------------------
proc demo::formatBoolean val {
    return [expr {$val ? "yes" : "no"}]
}

#------------------------------------------------------------------------------
# demo::labelCmd
#
# Sorts the contents of the tablelist widget tbl by its col'th column and makes
# sure the items will be updated 500 ms later (because one of the items might
# refer to a canvas containing the arrow that displays the sort order).
#------------------------------------------------------------------------------
proc demo::labelCmd {tbl col} {
    tablelist::sortByColumn $tbl $col
    updateItemsDelayed $tbl
}

#------------------------------------------------------------------------------
# demo::updateItemsDelayed
#
# Arranges for the items of the tablelist widget tbl to be updated 500 ms later.
#------------------------------------------------------------------------------
proc demo::updateItemsDelayed tbl {
    #
    # Schedule the demo::updateItems command for execution
    # 500 ms later, but only if it is not yet pending
    #
    if {[string compare [$tbl attrib afterId] ""] == 0} {
	$tbl attrib afterId [after 500 [list demo::updateItems $tbl]]
    }
}

#------------------------------------------------------------------------------
# demo::updateItems
#
# Updates the items of the tablelist widget tbl.
#------------------------------------------------------------------------------
proc demo::updateItems tbl {
    #
    # Reset the tablelist's "afterId" attribute
    #
    $tbl attrib afterId ""

    #
    # Update the items
    #
    set rowCount [$tbl size]
    for {set row 0} {$row < $rowCount} {incr row} {
	set c [$tbl rowattrib $row pathName]
	if {![winfo exists $c]} {
	    continue
	}

	set item {}
	lappend item \
		[winfo name $c] [winfo class $c] [winfo x $c] [winfo y $c] \
		[winfo width $c] [winfo height $c] [winfo ismapped $c] \
		[winfo viewable $c] [winfo manager $c]
	$tbl rowconfigure $row -text $item
    }

    #
    # Repeat the last sort operation (if any)
    #
    $tbl refreshsorting
}

#------------------------------------------------------------------------------
# demo::putChildrenOfSelWidget
#
# Outputs the data of the children of the selected widget into the tablelist
# widget tbl.
#------------------------------------------------------------------------------
proc demo::putChildrenOfSelWidget tbl {
    set w [$tbl rowattrib [$tbl curselection] pathName]
    if {![winfo exists $w]} {
	bell
	tk_messageBox -title "Error" -icon error -message \
	    "Bad window path name \"$w\"" -parent [winfo toplevel $tbl]
	return ""
    }

    if {[llength [winfo children $w]] == 0} {
	bell
    } else {
	putChildren $w $tbl root
    }
}

#------------------------------------------------------------------------------
# demo::dispConfigOfSelWidget
#
# Displays the configuration options of the selected widget within the
# tablelist tbl in a tablelist widget contained in a newly created top-level
# widget.
#------------------------------------------------------------------------------
proc demo::dispConfigOfSelWidget tbl {
    demo::displayConfig [$tbl rowattrib [$tbl curselection] pathName]
}

#------------------------------------------------------------------------------
# demo::postPopupMenu
#
# Posts the pop-up menu $top.menu at the given screen position.  Before posting
# the menu, the procedure enables/disables its first entry, depending upon
# whether the selected widget has children or not.
#------------------------------------------------------------------------------
proc demo::postPopupMenu {top rootX rootY} {
    set tbl $top.tf.tbl
    set w [$tbl rowattrib [$tbl curselection] pathName]
    if {![winfo exists $w]} {
	bell
	tk_messageBox -title "Error" -icon error -message \
	    "Bad window path name \"$w\"" -parent $top
	return ""
    }

    set menu $top.menu
    if {[llength [winfo children $w]] == 0} {
	$menu entryconfigure 0 -state disabled
    } else {
	$menu entryconfigure 0 -state normal
    }

    tk_popup $menu $rootX $rootY
}

#------------------------------------------------------------------------------
# demo::refreshView
#
# Redisplays the data of the children of the widget w in the tablelist widget
# tbl and restores the expanded states of the items as well as the vertical
# view.
#------------------------------------------------------------------------------
proc demo::refreshView {w tbl} {
    #
    # Save the vertical view and get the path names of
    # the child widgets displayed in the expanded rows
    #
    set yView [$tbl yview]
    foreach key [$tbl expandedkeys] {
	set pathName [$tbl rowattrib $key pathName]
	set expandedWidgets($pathName) 1
    }

    #
    # Redisplay the data of the widget's (possibly changed) children and
    # restore the expanded states of the children, along with the vertical view
    #
    putChildren $w $tbl root
    restoreExpandedStates $tbl root expandedWidgets
    $tbl yview moveto [lindex $yView 0]
}

#------------------------------------------------------------------------------
# demo::restoreExpandedStates
#   
# Expands those children of the parent identified by nodeIdx that display the
# data of child widgets whose path names are the names of the elements of the
# array specified by the last argument.
#------------------------------------------------------------------------------
proc demo::restoreExpandedStates {tbl nodeIdx expandedWidgetsName} {
    upvar $expandedWidgetsName expandedWidgets

    foreach key [$tbl childkeys $nodeIdx] {
	set pathName [$tbl rowattrib $key pathName]
	if {[info exists expandedWidgets($pathName)]} {
	    $tbl expand $key -partly
	    restoreExpandedStates $tbl $key expandedWidgets
	}
    }
}

#------------------------------------------------------------------------------

if {$tcl_interactive} {
    return "\nTo display information about the children of an arbitrary\
	    widget, enter\n\n\tdemo::displayChildren <widgetName>\n"
} else {
    wm withdraw .
    tk_messageBox -title $argv0 -icon warning -message \
	"Please source this script into\nan interactive wish session"
    exit 1
}
