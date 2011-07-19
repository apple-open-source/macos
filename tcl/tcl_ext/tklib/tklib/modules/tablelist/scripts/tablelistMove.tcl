#==============================================================================
# Contains the implementation of the tablelist move and movecolumn subcommands.
#
# Copyright (c) 2003-2010  Csaba Nemethi (E-mail: csaba.nemethi@t-online.de)
#==============================================================================

#------------------------------------------------------------------------------
# tablelist::moveRow
#
# Processes the tablelist move subcommand.
#------------------------------------------------------------------------------
proc tablelist::moveRow {win source target {withDescendants 1}} {
    upvar ::tablelist::ns${win}::data data
    if {$data(isDisabled) || $data(itemCount) == 0} {
	return ""
    }

    #
    # Adjust the indices to fit within the existing items and check them
    #
    if {$source > $data(lastRow)} {
	set source $data(lastRow)
    } elseif {$source < 0} {
	set source 0
    }
    if {$target > $data(itemCount)} {
	set target $data(itemCount)
    } elseif {$target < 0} {
	set target 0
    }
    if {$target == $source} {
	return -code error \
	       "cannot move item with index \"$source\" before itself"
    }

    set sourceItem [lindex $data(itemList) $source]
    set sourceKey [lindex $sourceItem end]
    if {$target == [nodeRow $win $sourceKey end 1]} {
	return ""
    }

    set parentKey $data($sourceKey-parent)
    set parentEndRow [nodeRow $win $parentKey end 1]
    if {($target <= [keyToRow $win $parentKey] || $target > $parentEndRow) &&
	$withDescendants} {
	return -code error \
	       "cannot move item with index \"$source\" outside its parent"
    }

    if {$target != $parentEndRow} {
	set targetKey [lindex $data(keyList) $target]
	if {[string compare $data($targetKey-parent) $parentKey] != 0 &&
	    $withDescendants} {
	    return -code error \
		   "cannot move item with index \"$source\" outside its parent"
	}
    }

    #
    # Save some data of the edit window if present
    #
    if {[set editCol $data(editCol)] >= 0} {
	set editRow $data(editRow)
	set editKey $data(editKey)
	saveEditData $win
    }

    #
    # Build the list of column indices of the selected cells
    # within the source line and then delete that line
    #
    set w $data(body)
    set selectedCols {}
    set line [expr {$source + 1}]
    set textIdx [expr {double($line)}]
    variable canElide
    variable elide
    for {set col 0} {$col < $data(colCount)} {incr col} {
	if {$data($col-hide) && !$canElide} {
	    continue
	}

	if {[lsearch -exact [$w tag names $textIdx] select] >= 0} {
	    lappend selectedCols $col
	}
	set textIdx [$w search $elide "\t" $textIdx+1c $line.end]+1c
    }
    $w delete [expr {double($source + 1)}] [expr {double($source + 2)}]

    #
    # Insert the source item before the target one
    #
    set target1 $target
    if {$source < $target} {
	incr target1 -1
    }
    set targetLine [expr {$target1 + 1}]
    $w insert $targetLine.0 "\n"
    set snipStr $data(-snipstring)
    set dispItem [lrange $sourceItem 0 $data(lastCol)]
    if {$data(hasFmtCmds)} {
	set dispItem [formatItem $win $sourceKey $source $dispItem]
    }
    set col 0
    foreach text [strToDispStr $dispItem] \
	    colTags $data(colTagsList) \
	    {pixels alignment} $data(colList) {
	if {$data($col-hide) && !$canElide} {
	    incr col
	    continue
	}

	#
	# Build the list of tags to be applied to the cell
	#
	set cellFont [getCellFont $win $sourceKey $col]
	set cellTags $colTags
	foreach opt {-background -foreground -font} {
	    if {[info exists data($sourceKey,$col$opt)]} {
		lappend cellTags cell$opt-$data($sourceKey,$col$opt)
	    }
	}

	#
	# Append the text and the labels or window (if
	# any) to the target line of the body text widget
	#
	appendComplexElem $win $sourceKey $source $col $text $pixels \
			  $alignment $snipStr $cellFont $cellTags $targetLine

	incr col
    }
    foreach opt {-background -foreground -font} {
	if {[info exists data($sourceKey$opt)]} {
	    $w tag add row$opt-$data($sourceKey$opt) \
		       $targetLine.0 $targetLine.end
	}
    }
    if {[info exists data($sourceKey-hide)]} {
	$w tag add hiddenRow $targetLine.0 $targetLine.end+1c
    }

    #
    # Update the item list and the key -> row mapping
    #
    set data(itemList) [lreplace $data(itemList) $source $source]
    set data(keyList) [lreplace $data(keyList) $source $source]
    if {$target == $data(itemCount)} {
	lappend data(itemList) $sourceItem	;# this works much faster
	lappend data(keyList) $sourceKey	;# this works much faster
    } else {
	set data(itemList) [linsert $data(itemList) $target1 $sourceItem]
	set data(keyList) [linsert $data(keyList) $target1 $sourceKey]
    }
    if {$source < $target} {
	for {set row $source} {$row < $targetLine} {incr row} {
	    set key [lindex $data(keyList) $row]
	    set data($key-row) $row
	}
    } else {
	for {set row $target} {$row <= $source} {incr row} {
	    set key [lindex $data(keyList) $row]
	    set data($key-row) $row
	}
    }

    #
    # Update the tree information
    #
    set parentKey $data($sourceKey-parent)
    set sourceChildIdx \
	[lsearch -exact $data($parentKey-children) $sourceKey]
    set data($parentKey-children) \
	[lreplace $data($parentKey-children) $sourceChildIdx $sourceChildIdx]
    if {$target == $parentEndRow} {
	set lastChildRow [nodeRow $win $parentKey end 0]
	set targetKey [lindex $data(keyList) $lastChildRow]
    } else {
	set targetKey [lindex $data(keyList) $target1]
    }
    ### set parentKey $data($targetKey-parent)
    set targetChildIdx \
	[lsearch -exact $data($parentKey-children) $targetKey]
    if {$targetChildIdx == [llength $data($parentKey-children)]} {
	lappend data($parentKey-children) $sourceKey
    } else {
	set data($parentKey-children) \
	    [linsert $data($parentKey-children) $targetChildIdx $sourceKey]
    }

    #
    # Update the list variable if present
    #
    if {$data(hasListVar)} {
	upvar #0 $data(-listvariable) var
	trace vdelete var wu $data(listVarTraceCmd)
	set var [lreplace $var $source $source]
	set pureSourceItem [lrange $sourceItem 0 $data(lastCol)]
	if {$target == $data(itemCount)} {
	    lappend var $pureSourceItem		;# this works much faster
	} else {
	    set var [linsert $var $target1 $pureSourceItem]
	}
	trace variable var wu $data(listVarTraceCmd)
    }

    #
    # Update anchorRow and activeRow if needed
    #
    if {$data(anchorRow) == $source} {
	set data(anchorRow) $target1
    }
    if {$data(activeRow) == $source} {
	set data(activeRow) $target1
    }

    #
    # Invalidate the list of row indices indicating the non-hidden rows
    #
    set data(nonHiddenRowList) {-1}

    #
    # Select those source elements that were selected before
    #
    foreach col $selectedCols {
	cellSelection $win set $target1 $col $target1 $col
    }

    #
    # Restore the edit window if it was present before
    #
    if {$editCol >= 0} {
	if {$editRow == $source} {
	    doEditCell $win $target1 $editCol 1
	} else {
	    set data(editRow) [keyToRow $win $editKey]
	}
    }

    if {$withDescendants} {
	#
	# Move the source item's descendants
	#
	set sourceDescCount [descCount $win $sourceKey]
	if {$source < $target} {
	    for {set n 0} {$n < $sourceDescCount} {incr n} {
		moveRow $win $source $target 0
	    }
	} else {
	    for {set n 0} {$n < $sourceDescCount} {incr n} {
		moveRow $win [incr source] [incr target] 0
	    }
	}

	#
	# Adjust the elided text, restore the stripes in the body
	# text widget, and redisplay the line numbers (if any)
	#
	adjustElidedText $win
	makeStripes $win
	showLineNumbersWhenIdle $win
	updateColorsWhenIdle $win
	adjustSepsWhenIdle $win
	updateVScrlbarWhenIdle $win
    }

    return ""
}

#------------------------------------------------------------------------------
# tablelist::moveCol
#
# Processes the tablelist movecolumn subcommand.
#------------------------------------------------------------------------------
proc tablelist::moveCol {win source target} {
    upvar ::tablelist::ns${win}::data data \
	  ::tablelist::ns${win}::attribs attribs
    if {$data(isDisabled)} {
	return ""
    }

    #
    # Check the indices
    #
    if {$target == $source} {
	return -code error \
	       "cannot move column with index \"$source\" before itself"
    } elseif {$target == $source + 1} {
	return ""
    }

    if {[winfo viewable $win]} {
	purgeWidgets $win
	update idletasks
	if {![winfo exists $win]} {		;# because of update idletasks
	    return ""
	}
    }

    #
    # Update the column list
    #
    set source3 [expr {3*$source}]
    set source3Plus2 [expr {$source3 + 2}]
    set target1 $target
    if {$source < $target} {
	incr target1 -1
    }
    set target3 [expr {3*$target1}]
    set sourceRange [lrange $data(-columns) $source3 $source3Plus2]
    set data(-columns) [lreplace $data(-columns) $source3 $source3Plus2]
    set data(-columns) [eval linsert {$data(-columns)} $target3 $sourceRange]

    #
    # Save some elements of data and attribs corresponding to source
    #
    array set tmpData [array get data $source-*]
    array set tmpData [array get data k*,$source-*]
    foreach specialCol {activeCol anchorCol editCol -treecolumn treeCol} {
	set tmpData($specialCol) $data($specialCol)
    }
    array set tmpAttribs [array get attribs $source-*]
    array set tmpAttribs [array get attribs k*,$source-*]
    set selCells [curCellSelection $win]
    set tmpRows [extractColFromCellList $selCells $source]

    #
    # Remove source from the list of stretchable columns
    # if it was explicitly specified as stretchable
    #
    if {[string compare $data(-stretch) "all"] != 0} {
	set sourceIsStretchable 0
	set stretchableCols {}
	foreach elem $data(-stretch) {
	    if {[string first $elem "end"] != 0 && $elem == $source} {
		set sourceIsStretchable 1
	    } else {
		lappend stretchableCols $elem
	    }
	}
	set data(-stretch) $stretchableCols
    }

    #
    # Build two lists of column numbers, needed
    # for shifting some elements of the data array
    #
    if {$source < $target} {
	for {set n $source} {$n < $target1} {incr n} {
	    lappend oldCols [expr {$n + 1}]
	    lappend newCols $n
	}
    } else {
	for {set n $source} {$n > $target} {incr n -1} {
	    lappend oldCols [expr {$n - 1}]
	    lappend newCols $n
	}
    }

    #
    # Remove the trace from the array element data(activeCol) because otherwise
    # the procedure moveColData won't work if the selection type is cell
    #
    trace vdelete data(activeCol) w [list tablelist::activeTrace $win]

    #
    # Move the elements of data and attribs corresponding
    # to the columns in oldCols to the elements corresponding
    # to the columns with the same indices in newCols
    #
    foreach oldCol $oldCols newCol $newCols {
	moveColData data data imgs $oldCol $newCol
	moveColAttribs attribs attribs $oldCol $newCol
	set selCells [replaceColInCellList $selCells $oldCol $newCol]
    }

    #
    # Move the elements of data and attribs corresponding
    # to source to the elements corresponding to target1
    #
    moveColData tmpData data imgs $source $target1
    moveColAttribs tmpAttribs attribs $source $target1
    set selCells [deleteColFromCellList $selCells $target1]
    foreach row $tmpRows {
	lappend selCells $row,$target1
    }

    #
    # If the column given by source was explicitly specified as
    # stretchable then add target1 to the list of stretchable columns
    #
    if {[string compare $data(-stretch) "all"] != 0 && $sourceIsStretchable} {
	lappend data(-stretch) $target1
	sortStretchableColList $win
    }

    #
    # Update the item list
    #
    set newItemList {}
    foreach item $data(itemList) {
	set sourceText [lindex $item $source]
	set item [lreplace $item $source $source]
	set item [linsert $item $target1 $sourceText]
	lappend newItemList $item
    }
    set data(itemList) $newItemList

    #
    # Update the list variable if present
    #
    condUpdateListVar $win

    #
    # Set up and adjust the columns, and rebuild
    # the lists of the column fonts and tag names
    #
    setupColumns $win $data(-columns) 0
    makeColFontAndTagLists $win
    makeSortAndArrowColLists $win
    adjustColumns $win {} 0

    #
    # Redisplay the items
    #
    redisplay $win 0 $selCells
    updateColorsWhenIdle $win

    #
    # Reconfigure the relevant column labels
    #
    foreach col [lappend newCols $target1] {
	reconfigColLabels $win imgs $col
    }

    #
    # Restore the trace set on the array element data(activeCol)
    # and enforce the execution of the activeTrace command
    #
    trace variable data(activeCol) w [list tablelist::activeTrace $win]
    set data(activeCol) $data(activeCol)

    return ""
}
