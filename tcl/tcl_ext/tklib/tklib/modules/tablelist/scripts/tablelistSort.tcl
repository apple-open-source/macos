#==============================================================================
# Contains the implementation of the tablelist::sortByColumn and
# tablelist::addToSortColumns commands, as well as of the tablelist sort,
# sortbycolumn, and sortbycolumnlist subcommands.
#
# Structure of the module:
#   - Public procedures related to sorting
#   - Private procedures implementing the sorting
#
# Copyright (c) 2000-2010  Csaba Nemethi (E-mail: csaba.nemethi@t-online.de)
#==============================================================================

#
# Public procedures related to sorting
# ====================================
#

#------------------------------------------------------------------------------
# tablelist::sortByColumn
#
# Sorts the contents of the tablelist widget win by its col'th column.  Returns
# the sort order (increasing or decreasing).
#------------------------------------------------------------------------------
proc tablelist::sortByColumn {win col} {
    #
    # Check the arguments
    #
    if {![winfo exists $win]} {
	return -code error "bad window path name \"$win\""
    }
    if {[string compare [winfo class $win] "Tablelist"] != 0} {
	return -code error "window \"$win\" is not a tablelist widget"
    }
    if {[catch {::$win columnindex $col} result] != 0} {
	return -code error $result
    }
    if {$result < 0 || $result >= [::$win columncount]} {
	return -code error "column index \"$col\" out of range"
    }
    set col $result
    if {[::$win columncget $col -showlinenumbers]} {
	return ""
    }

    #
    # Determine the sort order
    #
    if {[set idx [lsearch -exact [::$win sortcolumnlist] $col]] >= 0 &&
	[string compare [lindex [::$win sortorderlist] $idx] "increasing"]
	== 0} {
	set sortOrder decreasing
    } else {
	set sortOrder increasing
    }

    #
    # Sort the widget's contents based on the given column
    #
    if {[catch {::$win sortbycolumn $col -$sortOrder} result] == 0} {
	event generate $win <<TablelistColumnSorted>>
	return $sortOrder
    } else {
	return -code error $result
    }
}

#------------------------------------------------------------------------------
# tablelist::addToSortColumns
#
# Adds the col'th column of the tablelist widget win to the latter's list of
# sort columns and sorts the contents of the widget by the modified column
# list.  Returns the specified column's sort order (increasing or decreasing).
#------------------------------------------------------------------------------
proc tablelist::addToSortColumns {win col} {
    #
    # Check the arguments
    #
    if {![winfo exists $win]} {
	return -code error "bad window path name \"$win\""
    }
    if {[string compare [winfo class $win] "Tablelist"] != 0} {
	return -code error "window \"$win\" is not a tablelist widget"
    }
    if {[catch {::$win columnindex $col} result] != 0} {
	return -code error $result
    }
    if {$result < 0 || $result >= [::$win columncount]} {
	return -code error "column index \"$col\" out of range"
    }
    set col $result
    if {[::$win columncget $col -showlinenumbers]} {
	return ""
    }

    #
    # Update the lists of sort columns and orders
    #
    set sortColList [::$win sortcolumnlist]
    set sortOrderList [::$win sortorderlist]
    if {[set idx [lsearch -exact $sortColList $col]] >= 0} {
	if {[string compare [lindex $sortOrderList $idx] "increasing"] == 0} {
	    set sortOrder decreasing
	} else {
	    set sortOrder increasing
	}
	set sortOrderList [lreplace $sortOrderList $idx $idx $sortOrder]
    } else {
	lappend sortColList $col
	lappend sortOrderList increasing
	set sortOrder increasing
    }

    #
    # Sort the widget's contents according to the
    # modified lists of sort columns and orders
    #
    if {[catch {::$win sortbycolumnlist $sortColList $sortOrderList} result]
	== 0} {
	event generate $win <<TablelistColumnsSorted>>
	return $sortOrder
    } else {
	return -code error $result
    }
}

#
# Private procedures implementing the sorting
# ===========================================
#

#------------------------------------------------------------------------------
# tablelist::sortItems
#
# Processes the tablelist sort, sortbycolumn, and sortbycolumnlist subcommands.
#------------------------------------------------------------------------------
proc tablelist::sortItems {win parentKey sortColList sortOrderList} {
    variable canElide
    variable snipSides
    upvar ::tablelist::ns${win}::data data

    set sortAllItems [expr {[string compare $parentKey "root"] == 0}]
    if {[winfo viewable $win] && $sortAllItems} {
	purgeWidgets $win
	update idletasks
	if {![winfo exists $win]} {		;# because of update idletasks
	    return ""
	}
    }

    #
    # Make sure sortOrderList has the same length as sortColList
    #
    set sortColCount [llength $sortColList]
    set sortOrderCount [llength $sortOrderList]
    if {$sortOrderCount < $sortColCount} {
	for {set n $sortOrderCount} {$n < $sortColCount} {incr n} {
	    lappend sortOrderList increasing
	}
    } else {
	set sortOrderList [lrange $sortOrderList 0 [expr {$sortColCount - 1}]]
    }

    #
    # Save the keys corresponding to anchorRow and activeRow,
    # as well as the indices of the selected cells
    #
    foreach type {anchor active} {
	set ${type}Key [lindex $data(keyList) $data(${type}Row)]
    }
    set selCells [curCellSelection $win 1]

    #
    # Save some data of the edit window if present
    #
    if {[set editCol $data(editCol)] >= 0} {
	set editKey $data(editKey)
	saveEditData $win
    }

    #
    # Update the sort info and sort the item list
    #
    set descItemList {}
    if {[llength $sortColList] == 1 && [lindex $sortColList 0] == -1} {
	if {[string compare $data(-sortcommand) ""] == 0} {
	    return -code error "value of the -sortcommand option is empty"
	}

	set order [lindex $sortOrderList 0]

	if {$sortAllItems} {
	    #
	    # Update the sort info
	    #
	    for {set col 0} {$col < $data(colCount)} {incr col} {
		set data($col-sortRank) 0
		set data($col-sortOrder) ""
	    }
	    set data(sortColList) {}
	    set data(arrowColList) {}
	    set data(sortOrder) $order
	}

	#
	# Sort the child item list
	#
	sortChildren $win $parentKey [list lsort -$order -command \
	    $data(-sortcommand)] descItemList
    } else {					;# sorting by a column (list)
	#
	# Check the specified column indices
	#
	set sortColCount2 $sortColCount
	foreach col $sortColList {
	    if {$data($col-showlinenumbers)} {
		incr sortColCount2 -1
	    }
	}
	if {$sortColCount2 == 0} {
	    return ""
	}

	if {$sortAllItems} {
	    #
	    # Update the sort info
	    #
	    for {set col 0} {$col < $data(colCount)} {incr col} {
		set data($col-sortRank) 0
		set data($col-sortOrder) ""
	    }
	    set rank 1
	    foreach col $sortColList order $sortOrderList {
		if {$data($col-showlinenumbers)} {
		    continue
		}

		set data($col-sortRank) $rank
		set data($col-sortOrder) $order
		incr rank
	    }
	    makeSortAndArrowColLists $win
	}

	#
	# Sort the child item list based on the specified columns
	#
	for {set idx [expr {$sortColCount - 1}]} {$idx >= 0} {incr idx -1} {
	    set col [lindex $sortColList $idx]
	    if {$data($col-showlinenumbers)} {
		continue
	    }

	    set descItemList {}
	    set order [lindex $sortOrderList $idx]
	    if {[string compare $data($col-sortmode) "command"] == 0} {
		if {![info exists data($col-sortcommand)]} {
		    return -code error "value of the -sortcommand option for\
					column $col is missing or empty"
		}

		sortChildren $win $parentKey [list lsort -$order -index $col \
		    -command $data($col-sortcommand)] descItemList
	    } elseif {[string compare $data($col-sortmode) "asciinocase"]
		== 0} {
		if {$::tk_version < 8.5} {
		    sortChildren $win $parentKey [list lsort -$order \
			-index $col -command compareNoCase] descItemList
		} else {
		    sortChildren $win $parentKey [list lsort -$order \
			-index $col -ascii -nocase] descItemList
		}
	    } else {
		sortChildren $win $parentKey [list lsort -$order -index $col \
		    -$data($col-sortmode)] descItemList
	    }
	}
    }

    if {$sortAllItems} {
	#
	# Cancel the execution of all delayed
	# redisplay and redisplayCol commands
	#
	foreach name [array names data *redispId] {
	    after cancel $data($name)
	    unset data($name)
	}

	set canvasWidth $data(arrowWidth)
	if {[llength $data(arrowColList)] > 1} {
	    incr canvasWidth 6
	}
	foreach col $data(arrowColList) {
	    #
	    # Make sure the arrow will fit into the column
	    #
	    set idx [expr {2*$col}]
	    set pixels [lindex $data(colList) $idx]
	    if {$pixels == 0 && $data($col-maxPixels) > 0 &&
		$data($col-reqPixels) > $data($col-maxPixels) &&
		$data($col-maxPixels) < $canvasWidth} {
		set data($col-maxPixels) $canvasWidth
		set data($col-maxwidth) -$canvasWidth
	    }
	    if {$pixels != 0 && $pixels < $canvasWidth} {
		set data(colList) \
		    [lreplace $data(colList) $idx $idx $canvasWidth]
		set idx [expr {3*$col}]
		set data(-columns) \
		    [lreplace $data(-columns) $idx $idx -$canvasWidth]
	    }
	}

	#
	# Adjust the columns; this will also place the
	# canvas widgets into the corresponding labels
	#
	adjustColumns $win allLabels 1
    }

    if {[llength $descItemList] == 0} {
	return ""
    }

    set parentRow [keyToRow $win $parentKey]
    set firstDescRow [expr {$parentRow + 1}]
    set lastDescRow [expr {$parentRow + [descCount $win $parentKey]}]
    set firstDescLine [expr {$firstDescRow + 1}]
    set lastDescLine [expr {$lastDescRow + 1}]

    #
    # Update the line numbers (if any)
    #
    for {set col 0} {$col < $data(colCount)} {incr col} {
	if {!$data($col-showlinenumbers)} {
	    continue
	}

	set newDescItemList {}
	set line $firstDescLine
	foreach item $descItemList {
	    set item [lreplace $item $col $col $line]
	    lappend newDescItemList $item
	    set key [lindex $item end]
	    if {![info exists data($key-hide)]} {
		incr line
	    }
	}
	set descItemList $newDescItemList
    }

    set data(itemList) [eval [list lreplace $data(itemList) \
	$firstDescRow $lastDescRow] $descItemList]

    #
    # Replace the contents of the list variable if present
    #
    condUpdateListVar $win

    #
    # Delete the items from the body text widget and insert the sorted ones.
    # Interestingly, for a large number of items it is much more efficient
    # to empty each line individually than to invoke a global delete command.
    #
    set w $data(body)
    $w tag remove hiddenRow $firstDescLine.0 $lastDescLine.end
    for {set line $firstDescLine} {$line <= $lastDescLine} {incr line} {
	$w delete $line.0 $line.end
    }
    set snipStr $data(-snipstring)
    set rowTagRefCount $data(rowTagRefCount)
    set cellTagRefCount $data(cellTagRefCount)
    set isSimple [expr {$data(imgCount) == 0 && $data(winCount) == 0 &&
			$data(indentCount) == 0}]
    set padY [expr {[$w cget -spacing1] == 0}]
    set descKeyList {}
    for {set row $firstDescRow; set line $firstDescLine} \
	{$row <= $lastDescRow} {set row $line; incr line} {
	set item [lindex $data(itemList) $row]
	set key [lindex $item end]
	lappend descKeyList $key
	set data($key-row) $row
	set dispItem [lrange $item 0 $data(lastCol)]
	if {$data(hasFmtCmds)} {
	    set dispItem [formatItem $win $key $row $dispItem]
	}

	#
	# Clip the elements if necessary and
	# insert them with the corresponding tags
	#
	if {$rowTagRefCount == 0} {
	    set hasRowFont 0
	} else {
	    set hasRowFont [info exists data($key-font)]
	}
	set col 0
	if {$isSimple} {
	    set insertArgs {}
	    set multilineData {}
	    foreach text [strToDispStr $dispItem] \
		    colFont $data(colFontList) \
		    colTags $data(colTagsList) \
		    {pixels alignment} $data(colList) {
		if {$data($col-hide) && !$canElide} {
		    incr col
		    continue
		}

		#
		# Build the list of tags to be applied to the cell
		#
		if {$hasRowFont} {
		    set cellFont $data($key-font)
		} else {
		    set cellFont $colFont
		}
		set cellTags $colTags
		if {$cellTagRefCount != 0} {
		    if {[info exists data($key,$col-font)]} {
			set cellFont $data($key,$col-font)
			lappend cellTags cell-font-$data($key,$col-font)
		    }
		    foreach opt {-background -foreground} {
			if {[info exists data($key,$col$opt)]} {
			    lappend cellTags cell$opt-$data($key,$col$opt)
			}
		    }
		}

		#
		# Clip the element if necessary
		#
		set multiline [string match "*\n*" $text]
		if {$pixels == 0} {		;# convention: dynamic width
		    if {$data($col-maxPixels) > 0} {
			if {$data($col-reqPixels) > $data($col-maxPixels)} {
			    set pixels $data($col-maxPixels)
			}
		    }
		}
		if {$pixels != 0} {
		    incr pixels $data($col-delta)

		    if {$data($col-wrap) && !$multiline} {
			if {[font measure $cellFont -displayof $win $text] >
			    $pixels} {
			    set multiline 1
			}
		    }

		    set snipSide \
			$snipSides($alignment,$data($col-changesnipside))
		    if {$multiline} {
			set list [split $text "\n"]
			if {$data($col-wrap)} {
			    set snipSide ""
			}
			set text [joinList $win $list $cellFont \
				  $pixels $snipSide $snipStr]
		    } else {
			set text [strRange $win $text $cellFont \
				  $pixels $snipSide $snipStr]
		    }
		}

		if {$multiline} {
		    lappend insertArgs "\t\t" $cellTags
		    lappend multilineData $col $text $colFont $pixels $alignment
		} else {
		    lappend insertArgs "\t$text\t" $cellTags
		}

		incr col
	    }

	    #
	    # Insert the item into the body text widget
	    #
	    if {[llength $insertArgs] != 0} {
		eval [list $w insert $line.0] $insertArgs
	    }

	    #
	    # Embed the message widgets displaying multiline elements
	    #
	    foreach {col text font pixels alignment} $multilineData {
		findTabs $win $line $col $col tabIdx1 tabIdx2
		set msgScript [list ::tablelist::displayText $win $key \
			       $col $text $font $pixels $alignment]
		$w window create $tabIdx2 -pady $padY -create $msgScript
	    }

	} else {
	    foreach text [strToDispStr $dispItem] \
		    colFont $data(colFontList) \
		    colTags $data(colTagsList) \
		    {pixels alignment} $data(colList) {
		if {$data($col-hide) && !$canElide} {
		    incr col
		    continue
		}

		#
		# Build the list of tags to be applied to the cell
		#
		if {$hasRowFont} {
		    set cellFont $data($key-font)
		} else {
		    set cellFont $colFont
		}
		set cellTags $colTags
		if {$cellTagRefCount != 0} {
		    if {[info exists data($key,$col-font)]} {
			set cellFont $data($key,$col-font)
			lappend cellTags cell-font-$data($key,$col-font)
		    }
		    foreach opt {-background -foreground} {
			if {[info exists data($key,$col$opt)]} {
			    lappend cellTags cell$opt-$data($key,$col$opt)
			}
		    }
		}

		#
		# Insert the text and the label or window
		# (if any) into the body text widget
		#
		appendComplexElem $win $key $row $col $text $pixels \
				  $alignment $snipStr $cellFont $cellTags $line

		incr col
	    }
	}

	if {$rowTagRefCount != 0} {
	    foreach opt {-background -foreground -font} {
		if {[info exists data($key$opt)]} {
		    $w tag add row$opt-$data($key$opt) $line.0 $line.end
		}
	    }
	}

	if {[info exists data($key-hide)]} {
	    $w tag add hiddenRow $line.0 $line.end+1c
	}
    }

    set data(keyList) [eval [list lreplace $data(keyList) \
	$firstDescRow $lastDescRow] $descKeyList]

    if {$sortAllItems} {
	#
	# Validate the key -> row mapping
	#
	set data(keyToRowMapValid) 1
	if {[info exists data(mapId)]} {
	    after cancel $data(mapId)
	    unset data(mapId)
	}
    }

    #
    # Invalidate the list of row indices indicating the non-hidden rows
    #
    set data(nonHiddenRowList) {-1}

    #
    # Select the cells that were selected before
    #
    foreach {key col} $selCells {
	set row [keyToRow $win $key]
	cellSelection $win set $row $col $row $col
    }

    #
    # Disable the body text widget if it was disabled before
    #
    if {$data(isDisabled)} {
	$w tag add disabled 1.0 end
	$w tag configure select -borderwidth 0
    }

    #
    # Update anchorRow and activeRow
    #
    foreach type {anchor active} {
	upvar 0 ${type}Key key2
	if {[string compare $key2 ""] != 0} {
	    set data(${type}Row) [keyToRow $win $key2]
	}
    }

    #
    # Bring the "most important" row into view if appropriate
    #
    if {$editCol >= 0} {
	set editRow [keyToRow $win $editKey]
	if {$editRow >= $firstDescRow && $editRow <= $lastDescRow} {
	    doEditCell $win $editRow $editCol 1
	}
    } else {
	set selRows [curSelection $win]
	if {[llength $selRows] == 1} {
	    set selRow [lindex $selRows 0]
	    if {$selRow >= $firstDescRow && $selRow <= $lastDescRow} {
		seeRow $win $selRow
	    }
	} elseif {[string compare [focus -lastfor $w] $w] == 0} {
	    if {$data(activeRow) >= $firstDescRow &&
		$data(activeRow) <= $lastDescRow} {
		seeRow $win $data(activeRow)
	    }
	}
    }

    #
    # Adjust the elided text and restore the stripes in the body text widget
    #
    adjustElidedText $win
    makeStripes $win
    updateColorsWhenIdle $win
    adjustSepsWhenIdle $win
    updateVScrlbarWhenIdle $win

    #
    # Work around a Tk bug on Mac OS X Aqua
    #
    variable winSys
    if {[string compare $winSys "aqua"] == 0} {
	foreach col $data(arrowColList) {
	    set canvas [list $data(hdrTxtFrCanv)$col]
	    after idle "lower $canvas; raise $canvas"
	}
    }

    return ""
}

#------------------------------------------------------------------------------
# tablelist::sortChildren
#
# Sorts the children of a given parent within the tablelist widget win,
# recursively.
#------------------------------------------------------------------------------
proc tablelist::sortChildren {win parentKey sortProc itemListName} {
    upvar $itemListName itemList ::tablelist::ns${win}::data data

    set childKeyList $data($parentKey-children)
    if {[llength $childKeyList] == 0} {
	return ""
    }

    #
    # Build and sort the list of child items
    #
    set childItemList {}
    foreach childKey $childKeyList {
	lappend childItemList [lindex $data(itemList) [keyToRow $win $childKey]]
    }
    set childItemList [eval $sortProc [list $childItemList]]

    #
    # Update the lists and invoke the procedure recursively for the children
    #
    set data($parentKey-children) {}
    foreach item $childItemList {
	lappend itemList $item
	set childKey [lindex $item end]
	lappend data($parentKey-children) $childKey

	sortChildren $win $childKey $sortProc itemList
    }
}

#------------------------------------------------------------------------------
# tablelist::sortList
#
# Sorts the specified list by the current sort columns of the tablelist widget
# win, using their current sort orders.
#------------------------------------------------------------------------------
proc tablelist::sortList {win list} {
    upvar ::tablelist::ns${win}::data data
    set sortColList $data(sortColList)
    set sortOrderList {}
    foreach col $sortColList {
	lappend sortOrderList $data($col-sortOrder)
    }

    if {[llength $sortColList] == 1 && [lindex $sortColList 0] == -1} {
	if {[string compare $data(-sortcommand) ""] == 0} {
	    return -code error "value of the -sortcommand option is empty"
	}

	#
	# Sort the list
	#
	set order [lindex $sortOrderList 0]
	return [lsort -$order -command $data(-sortcommand) $list]
    } else {
	#
	# Sort the list based on the specified columns
	#
	set sortColCount [llength $sortColList]
	for {set idx [expr {$sortColCount - 1}]} {$idx >= 0} {incr idx -1} {
	    set col [lindex $sortColList $idx]
	    set order [lindex $sortOrderList $idx]

	    if {[string compare $data($col-sortmode) "command"] == 0} {
		if {![info exists data($col-sortcommand)]} {
		    return -code error "value of the -sortcommand option for\
					column $col is missing or empty"
		}

		set list [lsort -$order -index $col -command \
			  $data($col-sortcommand) $list]
	    } elseif {[string compare $data($col-sortmode) "asciinocase"]
		== 0} {
		if {$::tk_version < 8.5} {
		    set list [lsort -$order -index $col -command \
			      compareNoCase $list]
		} else {
		    set list [lsort -$order -index $col -ascii -nocase $list]
		}
	    } else {
		set list [lsort -$order -index $col -$data($col-sortmode) $list]
	    }
	}

	return $list
    }
}

#------------------------------------------------------------------------------
# tablelist::compareNoCase
#
# Compares the given strings in a case-insensitive manner.
#------------------------------------------------------------------------------
proc tablelist::compareNoCase {str1 str2} {
    return [string compare [string tolower $str1] [string tolower $str2]]
}
