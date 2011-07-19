#!/bin/sh
# the next line restarts using wish \
exec wish "$0" ${1+"$@"}

#==============================================================================
# Demonstrates how to use a tablelist widget for displaying the contents of a
# directory.
#
# Copyright (c) 2010  Csaba Nemethi (E-mail: csaba.nemethi@t-online.de)
#==============================================================================

package require Tk 8.3
package require tablelist 5.1

#
# Add some entries to the Tk option database
#
set dir [file dirname [info script]]
source [file join $dir option.tcl]

#
# Create three images
#
set clsdFolderImg [image create photo -file [file join $dir clsdFolder.gif]]
set openFolderImg [image create photo -file [file join $dir openFolder.gif]]
set fileImg       [image create photo -file [file join $dir file.gif]]

#------------------------------------------------------------------------------
# displayContents
#
# Displays the contents of the directory dir in a tablelist widget.
#------------------------------------------------------------------------------
proc displayContents dir {
    #
    # Create a vertically scrolled tablelist widget with 3
    # dynamic-width columns and interactive sort capability
    #
    set tf .tf
    frame $tf
    set tbl $tf.tbl
    set vsb $tf.vsb
    tablelist::tablelist $tbl \
	-columns {0 "Name"	    left
		  0 "Size"	    right
		  0 "Date Modified" left} \
	-expandcommand expandCmd -collapsecommand collapseCmd \
	-yscrollcommand [list $vsb set] -movablecolumns no -setgrid no \
	-showseparators yes -height 20 -width 80
    if {[$tbl cget -selectborderwidth] == 0} {
	$tbl configure -spacing 1
    }
    $tbl columnconfigure 0 -formatcommand formatString -sortmode dictionary
    $tbl columnconfigure 1 -formatcommand formatSize -sortmode integer
    $tbl columnconfigure 2 -formatcommand formatString
    scrollbar $vsb -orient vertical -command [list $tbl yview]

    #
    # Create a pop-up menu with one command entry; bind the script
    # associated with its entry to the <Double-1> event, too
    #
    set menu .menu
    menu $menu -tearoff no
    $menu add command -label "Display Contents" \
		      -command [list putContentsOfSelFolder $tbl]
    set bodyTag [$tbl bodytag]
    bind $bodyTag <<Button3>>  [bind TablelistBody <Button-1>]
    bind $bodyTag <<Button3>> +[bind TablelistBody <ButtonRelease-1>]
    bind $bodyTag <<Button3>> +[list postPopupMenu %X %Y]
    bind $bodyTag <Double-1>   [list putContentsOfSelFolder $tbl]

    #
    # Create three buttons within a frame child of the main widget
    #
    set bf .bf
    frame $bf
    set b1 $bf.b1
    set b2 $bf.b2
    set b3 $bf.b3
    button $b1 -width 10 -text "Refresh"
    button $b2 -width 10 -text "Parent"
    button $b3 -width 10 -text "Close" -command exit

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
    # Populate the tablelist with the contents of the given directory
    #
    $tbl sortbycolumn 0
    putContents $dir $tbl root
}

#------------------------------------------------------------------------------
# putContents
#
# Outputs the contents of the directory dir into the tablelist widget tbl, as
# child items of the one identified by nodeIdx.
#------------------------------------------------------------------------------
proc putContents {dir tbl nodeIdx} {
    #
    # The following check is necessary because this procedure
    # is also invoked by the "Refresh" and "Parent" buttons
    #
    if {[string compare $dir ""] != 0 &&
	(![file isdirectory $dir] || ![file readable $dir])} {
	bell
	if {[string compare $nodeIdx "root"] == 0} {
	    set choice [tk_messageBox -title "Error" -icon warning -message \
			"Cannot read directory \"[file nativename $dir]\"\
			-- replacing it with nearest existent ancestor" \
			-type okcancel -default ok]
	    if {[string compare $choice "ok"] == 0} {
		while {![file isdirectory $dir] || ![file readable $dir]} {
		    set dir [file dirname $dir]
		}
	    } else {
		return ""
	    }
	} else {
	    return ""
	}
    }

    if {[string compare $nodeIdx "root"] == 0} {
	if {[string compare $dir ""] == 0} {
	    wm title . "Contents of the Workspace"
	} else {
	    wm title . "Contents of the Directory \"[file nativename $dir]\""
	}

	$tbl delete 0 end
	set row 0
    } else {
	set row [expr {$nodeIdx + 1}]
    }

    #
    # Build a list from the data of the subdirectories and
    # files of the directory dir.  Prepend a "D" or "F" to
    # each entry's name and modification date & time, for
    # sorting purposes (it will be removed by formatString).
    #
    set itemList {}
    if {[string compare $dir ""] == 0} {
	foreach volume [file volumes] {
	    lappend itemList [list D[file nativename $volume] -1 D $volume]
	}
    } else {
	foreach entry [glob -nocomplain -types {d f} -directory $dir *] {
	    if {[catch {file mtime $entry} modTime] != 0} {
		continue
	    }

	    if {[file isdirectory $entry]} {
		lappend itemList [list D[file tail $entry] -1 \
		    D[clock format $modTime -format "%Y-%m-%d %H:%M"] $entry]
	    } else {
		lappend itemList [list F[file tail $entry] [file size $entry] \
		    F[clock format $modTime -format "%Y-%m-%d %H:%M"] ""]
	    }
	}
    }

    #
    # Sort the above list and insert it into the tablelist widget
    # tbl as list of children of the row identified by nodeIdx
    #
    set itemList [$tbl applysorting $itemList]
    $tbl insertchildlist $nodeIdx end $itemList

    #
    # Insert an image into the first cell of each newly inserted row
    #
    global clsdFolderImg fileImg
    foreach item $itemList {
	set name [lindex $item end]
	if {[string compare $name ""] == 0} {			;# file
	    $tbl cellconfigure $row,0 -image $fileImg
	} else {						;# subdirectory
	    $tbl cellconfigure $row,0 -image $clsdFolderImg
	    $tbl rowattrib $row pathName $name

	    #
	    # Mark the row as collapsed if the subdirectory is non-empty
	    #
	    if {[file readable $name] && [llength \
		[glob -nocomplain -types {d f} -directory $name *]] != 0} {
		$tbl collapse $row
	    }
	}

	incr row
    }

    if {[string compare $nodeIdx "root"] == 0} {
	#
	# Configure the "Refresh" and "Parent" buttons
	#
	.bf.b1 configure -command [list refreshView $dir $tbl]
	set b2 .bf.b2
	if {[string compare $dir ""] == 0} {
	    $b2 configure -state disabled
	} else {
	    $b2 configure -state normal
	    set p [file dirname $dir]
	    if {[string compare $p $dir] == 0} {
		$b2 configure -command [list putContents "" $tbl root]
	    } else {
		$b2 configure -command [list putContents $p $tbl root]
	    }
	}
    }
}

#------------------------------------------------------------------------------
# formatString
#
# Returns the substring obtained from the specified value by removing its first
# character.
#------------------------------------------------------------------------------
proc formatString val {
    return [string range $val 1 end]
}

#------------------------------------------------------------------------------
# formatSize
#
# Returns an empty string if the specified value is negative and the value
# itself in user-friendly format otherwise.
#------------------------------------------------------------------------------
proc formatSize val {
    if {$val < 0} {
	return ""
    } elseif {$val < 1024} {
	return "$val bytes"
    } elseif {$val < 1048576} {
	return [format "%.1f KB" [expr {$val / 1024.0}]]
    } elseif {$val < 1073741824} {
	return [format "%.1f MB" [expr {$val / 1048576.0}]]
    } else {
	return [format "%.1f GB" [expr {$val / 1073741824.0}]]
    }
}

#------------------------------------------------------------------------------
# expandCmd
#
# Outputs the contents of the directory whose leaf name is displayed in the
# first cell of the specified row of the tablelist widget tbl, as child items
# of the one identified by row, and updates the image displayed in that cell.
#------------------------------------------------------------------------------
proc expandCmd {tbl row} {
    if {[$tbl childcount $row] == 0} {
	set dir [$tbl rowattrib $row pathName]
	putContents $dir $tbl $row
    }

    if {[$tbl childcount $row] != 0} {
	global openFolderImg
	$tbl cellconfigure $row,0 -image $openFolderImg
    }
}

#------------------------------------------------------------------------------
# collapseCmd
#
# Updates the image displayed in the first cell of the specified row of the
# tablelist widget tbl.
#------------------------------------------------------------------------------
proc collapseCmd {tbl row} {
    global clsdFolderImg
    $tbl cellconfigure $row,0 -image $clsdFolderImg
}

#------------------------------------------------------------------------------
# putContentsOfSelFolder
#
# Outputs the contents of the selected folder into the tablelist widget tbl.
#------------------------------------------------------------------------------
proc putContentsOfSelFolder tbl {
    set row [$tbl curselection]
    if {[$tbl hasrowattrib $row pathName]} {		;# subdirectory item
	set dir [$tbl rowattrib $row pathName]
	if {[file isdirectory $dir] && [file readable $dir]} {
	    if {[llength [glob -nocomplain -types {d f} -directory $dir *]]
		== 0} {
		bell
	    } else {
		putContents $dir $tbl root
	    }
	} else {
	    bell
	    tk_messageBox -title "Error" -icon error -message \
		"Cannot read directory \"[file nativename $dir]\""
	    return ""
	}
    } else {						;# file item
	bell
    }
}

#------------------------------------------------------------------------------
# postPopupMenu
#
# Posts the pop-up menu .menu at the given screen position.  Before posting
# the menu, the procedure enables/disables its only entry, depending upon
# whether the selected item represents a readable directory or not.
#------------------------------------------------------------------------------
proc postPopupMenu {rootX rootY} {
    set tbl .tf.tbl
    set row [$tbl curselection]
    set menu .menu
    if {[$tbl hasrowattrib $row pathName]} {		;# subdirectory item
	set dir [$tbl rowattrib $row pathName]
	if {[file isdirectory $dir] && [file readable $dir]} {
	    if {[llength [glob -nocomplain -types {d f} -directory $dir *]]
		== 0} {
		$menu entryconfigure 0 -state disabled
	    } else {
		$menu entryconfigure 0 -state normal
	    }
	} else {
	    bell
	    tk_messageBox -title "Error" -icon error -message \
		"Cannot read directory \"[file nativename $dir]\""
	    return ""
	}
    } else {						;# file item
	$menu entryconfigure 0 -state disabled
    }

    tk_popup $menu $rootX $rootY
}

#------------------------------------------------------------------------------
# refreshView
#
# Redisplays the contents of the directory dir in the tablelist widget tbl and
# restores the expanded states of the folders as well as the vertical view.
#------------------------------------------------------------------------------
proc refreshView {dir tbl} {
    #
    # Save the vertical view and get the path names
    # of the folders displayed in the expanded rows
    #
    set yView [$tbl yview]
    foreach key [$tbl expandedkeys] {
	set pathName [$tbl rowattrib $key pathName]
	set expandedFolders($pathName) 1
    }

    #
    # Redisplay the directory's (possibly changed) contents and restore
    # the expanded states of the folders, along with the vertical view
    #
    putContents $dir $tbl root
    restoreExpandedStates $tbl root expandedFolders
    $tbl yview moveto [lindex $yView 0]
}

#------------------------------------------------------------------------------
# restoreExpandedStates
#
# Expands those children of the parent identified by nodeIdx that display
# folders whose path names are the names of the elements of the array specified
# by the last argument.
#------------------------------------------------------------------------------
proc restoreExpandedStates {tbl nodeIdx expandedFoldersName} {
    upvar $expandedFoldersName expandedFolders

    foreach key [$tbl childkeys $nodeIdx] {
	set pathName [$tbl rowattrib $key pathName]
	if {[string compare $pathName ""] != 0 &&
	    [info exists expandedFolders($pathName)]} {
	    $tbl expand $key -partly
	    restoreExpandedStates $tbl $key expandedFolders
	}
    }
}

displayContents ""
