bind TreeCtrlFileList <Double-ButtonPress-1> {
    TreeCtrl::FileListEditCancel %W
    TreeCtrl::DoubleButton1 %W %x %y
    break
}
bind TreeCtrlFileList <Control-ButtonPress-1> {
    set TreeCtrl::Priv(selectMode) toggle
    TreeCtrl::FileListButton1 %W %x %y
    break
}
bind TreeCtrlFileList <Shift-ButtonPress-1> {
    set TreeCtrl::Priv(selectMode) add
    TreeCtrl::FileListButton1 %W %x %y
    break
}
bind TreeCtrlFileList <ButtonPress-1> {
    set TreeCtrl::Priv(selectMode) set
    TreeCtrl::FileListButton1 %W %x %y
    break
}
bind TreeCtrlFileList <Button1-Motion> {
    TreeCtrl::FileListMotion1 %W %x %y
    break
}
bind TreeCtrlFileList <Button1-Leave> {
    TreeCtrl::FileListLeave1 %W %x %y
    break
}
bind TreeCtrlFileList <ButtonRelease-1> {
    TreeCtrl::FileListRelease1 %W %x %y
    break
}

proc TreeCtrl::FileListButton1 {T x y} {
    variable Priv
    focus $T
    set id [$T identify $x $y]
    set marquee 0
    set Priv(buttonMode) ""
    FileListEditCancel $T
    # Click outside any item
    if {$id eq ""} {
	set marquee 1

	# Click in header
    } elseif {[lindex $id 0] eq "header"} {
	ButtonPress1 $T $x $y

	# Click in item
    } else {
	foreach {where item arg1 arg2 arg3 arg4} $id {}
	switch $arg1 {
	    button -
	    line {
		ButtonPress1 $T $x $y
	    }
	    column {
		set ok 0
		# Clicked an element
		if {[llength $id] == 6} {
		    set E [lindex $id 5]
		    foreach list $Priv(sensitive,$T) {
			set C [lindex $list 0]
			set S [lindex $list 1]
			set eList [lrange $list 2 end]
			if {$arg2 != [$T column index $C]} continue
			if {[$T item style set $item $C] ne $S} continue
			if {[lsearch -exact $eList $E] == -1} continue
			set ok 1
			break
		    }
		}
		if {$ok} {
		    set Priv(drag,motion) 0
		    set Priv(drag,x) [$T canvasx $x]
		    set Priv(drag,y) [$T canvasy $y]
		    set Priv(drop) ""
		    set Priv(drag,wasSel) [$T selection includes $item]
		    set Priv(drag,E) $E
		    $T activate $item
		    if {$Priv(selectMode) eq "add"} {
			BeginExtend $T $item
		    } elseif {$Priv(selectMode) eq "toggle"} {
			BeginToggle $T $item
		    } elseif {![$T selection includes $item]} {
			BeginSelect $T $item
		    }

		    # Changing the selection might change the list
		    if {[$T index $item] eq ""} return

		    # Click selected item to drag
		    if {[$T selection includes $item]} {
			set Priv(buttonMode) drag
		    }
		} else {
		    set marquee 1
		}
	    }
	}
    }
    if {$marquee} {
	set Priv(buttonMode) marquee
	if {$Priv(selectMode) ne "set"} {
	    set Priv(selection) [$T selection get]
	} else {
	    $T selection clear
	    set Priv(selection) {}
	}
	MarqueeBegin $T $x $y
    }
    return
}
proc TreeCtrl::FileListMotion1 {T x y} {
    variable Priv
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	"resize" -
	"header" {
	    Motion1 $T $x $y
	}
	"drag" -
	"marquee" {
	    FileListAutoScanCheck $T $x $y
	    FileListMotion $T $x $y
	}
    }
}
proc TreeCtrl::FileListMotion {T x y} {
    variable Priv
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	"resize" -
	"header" {
	    Motion1 $T $x $y
	}
	"marquee" {
	    MarqueeUpdate $T $x $y
	    set select $Priv(selection)
	    set deselect {}

	    # Check items covered by the marquee
	    foreach list [$T marque identify] {
		set item [lindex $list 0]

		# Check covered columns in this item
		foreach sublist [lrange $list 1 end] {
		    set column [lindex $sublist 0]
		    set ok 0

		    # Check covered elements in this column
		    foreach E [lrange $sublist 1 end] {
			foreach sList $Priv(sensitive,$T) {
			    set sC [lindex $sList 0]
			    set sS [lindex $sList 1]
			    set sEList [lrange $sList 2 end]
			    if {$column != [$T column index $sC]} continue
			    if {[$T item style set $item $sC] ne $sS} continue
			    if {[lsearch -exact $sEList $E] == -1} continue
			    set ok 1
			    break
			}
		    }
		    # Some sensitive elements in this column are covered
		    if {$ok} {

			# Toggle selected status
			if {$Priv(selectMode) eq "toggle"} {
			    set i [lsearch -exact $Priv(selection) $item]
			    if {$i == -1} {
				lappend select $item
			    } else {
				set i [lsearch -exact $select $item]
				set select [lreplace $select $i $i]
			    }
			} else {
			    lappend select $item
			}
		    }
		}
	    }
	    $T selection modify $select all
	}
	"drag" {
	    # Detect initial mouse movement
	    if {!$Priv(drag,motion)} {
		set Priv(selection) [$T selection get]
		set Priv(drop) ""
		$T dragimage clear
		# For each selected item, add some elements to the dragimage
		foreach I $Priv(selection) {
		    foreach list $Priv(dragimage,$T) {
			set C [lindex $list 0]
			set S [lindex $list 1]
			if {[$T item style set $I $C] eq $S} {
			    eval $T dragimage add $I $C [lrange $list 2 end]
			}
		    }
		}
		set Priv(drag,motion) 1
		# Don't generate the event if it wasn't installed
		if {[lsearch -exact [$T notify eventnames] Drag] != -1} {
		    $T notify generate <Drag-begin> [list T $T]
		}
	    }

	    # Find the element under the cursor
	    set drop ""
	    set id [$T identify $x $y]
	    set ok 0
	    if {($id ne "") && ([lindex $id 0] eq "item") && ([llength $id] == 6)} {
		set item [lindex $id 1]
		set column [lindex $id 3]
		set E [lindex $id 5]
		foreach list $Priv(sensitive,$T) {
		    set C [lindex $list 0]
		    set S [lindex $list 1]
		    set eList [lrange $list 2 end]
		    if {$column != [$T column index $C]} continue
		    if {[$T item style set $item $C] ne $S} continue
		    if {[lsearch -exact $eList $E] == -1} continue
		    set ok 1
		    break
		}
	    }
	    if {$ok} {
		# If the item is not in the pre-drag selection
		# (i.e. not being dragged) and it is a directory,
		# see if we can drop on it
		if {[lsearch -exact $Priv(selection) $item] == -1} {
		    if {[lindex [$T item index $item] 1] < $Priv(DirCnt,$T)} {
			set drop $item
			# We can drop if dragged item isn't an ancestor
			foreach item2 $Priv(selection) {
			    if {[$T item isancestor $item2 $item]} {
				set drop ""
				break
			    }
			}
		    }
		}
	    }

	    # Select the directory under the cursor (if any) and deselect
	    # the previous drop-directory (if any)
	    $T selection modify $drop $Priv(drop)
	    set Priv(drop) $drop

	    # Show the dragimage in its new position
	    set x [expr {[$T canvasx $x] - $Priv(drag,x)}]
	    set y [expr {[$T canvasy $y] - $Priv(drag,y)}]
	    $T dragimage offset $x $y
	    $T dragimage visible yes
	}
    }
    return
}
proc TreeCtrl::FileListLeave1 {T x y} {
    variable Priv
    # This gets called when I click the mouse on Unix, and buttonMode is unset
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	"header" {
	    Leave1 $T $x $y
	}
    }
    return
}
proc TreeCtrl::FileListRelease1 {T x y} {
    variable Priv
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	"resize" -
	"header" {
	    Release1 $T $x $y
	}
	"marquee" {
	    AutoScanCancel $T
	    MarqueeEnd $T $x $y
	}
	"drag" {
	    AutoScanCancel $T

	    # Some dragging occurred
	    if {$Priv(drag,motion)} {
		$T dragimage visible no
		if {$Priv(drop) ne ""} {
		    $T selection modify {} $Priv(drop)
		    if {[lsearch -exact [$T notify eventnames] Drag] != -1} {
			$T notify generate <Drag-receive> \
			    [list T $T I $Priv(drop) l $Priv(selection)]
		    }
		}
		if {[lsearch -exact [$T notify eventnames] Drag] != -1} {
		    $T notify generate <Drag-end> [list T $T]
		}

	    } elseif {$Priv(selectMode) eq "toggle"} {
		# don't rename

		# Clicked/released a selected item, but didn't drag
	    } elseif {$Priv(drag,wasSel)} {
		set I [$T index active]
		set E $Priv(drag,E)
		set S [$T item style set $I 0]
		if {[lsearch -exact $Priv(edit,$T) $E] != -1} {
		    FileListEditCancel $T
		    set Priv(editId,$T) \
			[after 900 [list ::TreeCtrl::FileListEdit $T $I $S $E]]
		}
	    }
	}
    }
    set Priv(buttonMode) ""
    return
}
proc TreeCtrl::FileListEdit {T I S E} {
    variable Priv
    array unset Priv editId,$T
    set lines [$T item element cget $I 0 $E -lines]
    if {$lines eq ""} {
	set lines [$T element cget $E -lines]
    }

    # Scroll item into view
    $T see $I ; update

    # Multi-line edit
    if {$lines ne "1"} {
	scan [$T item bbox $I 0] "%d %d %d %d" x1 y1 x2 y2
	set padx [$T style layout $S $E -padx]
	if {[llength $padx] == 2} {
	    set padw [lindex $padx 0]
	    set pade [lindex $padx 1]
	} else {
	    set pade [set padw $padx]
	}
	TextExpanderOpen $T $I 0 $E [expr {$x2 - $x1 - $padw - $pade}]

	# Single-line edit
    } else {
	EntryExpanderOpen $T $I 0 $E
    }
    return
}
proc TreeCtrl::FileListEditCancel {T} {
    variable Priv
    if {[info exists Priv(editId,$T)]} {
	after cancel $Priv(editId,$T)
	array unset Priv editId,$T
    }
    return
}

# Same as TreeCtrl::AutoScanCheck, but calls FileListMotion and
# FileListAutoScanCheckAux
proc TreeCtrl::FileListAutoScanCheck {T x y} {
    variable Priv
    scan [$T contentbox] "%d %d %d %d" x1 y1 x2 y2
    set margin [winfo pixels $T [$T cget -scrollmargin]]
    if {($x < $x1 + $margin) || ($x >= $x2 - $margin) ||
	($y < $y1 + $margin) || ($y >= $y2 - $margin)} {
	if {![info exists Priv(autoscan,afterId,$T)]} {
	    if {$y >= $y2 - $margin} {
		$T yview scroll 1 units
		set delay [$T cget -yscrolldelay]
	    } elseif {$y < $y1 + $margin} {
		$T yview scroll -1 units
		set delay [$T cget -yscrolldelay]
	    } elseif {$x >= $x2 - $margin} {
		$T xview scroll 1 units
		set delay [$T cget -xscrolldelay]
	    } elseif {$x < $x1 + $margin} {
		$T xview scroll -1 units
		set delay [$T cget -xscrolldelay]
	    }
	    set count [scan $delay "%d %d" d1 d2]
	    if {[info exists Priv(autoscan,scanning,$T)]} {
		if {$count == 2} {
		    set delay $d2
		}
	    } else {
		if {$count == 2} {
		    set delay $d1
		}
		set Priv(autoscan,scanning,$T) 1
	    }
	    switch $Priv(buttonMode) {
		"drag" -
		"marquee" {
		    FileListMotion $T $x $y
		}
	    }
	    set Priv(autoscan,afterId,$T) [after $delay [list TreeCtrl::FileListAutoScanCheckAux $T]]
	}
	return
    }
    AutoScanCancel $T
    return
}

proc ::TreeCtrl::FileListAutoScanCheckAux {T} {
    variable Priv
    unset Priv(autoscan,afterId,$T)
    set x [winfo pointerx $T]
    set y [winfo pointery $T]
    set x [expr {$x - [winfo rootx $T]}]
    set y [expr {$y - [winfo rooty $T]}]
    FileListAutoScanCheck $T $x $y
    return
}

proc ::TreeCtrl::EntryOpen {T item column element} {

    variable Priv

    set Priv(entry,$T,item) $item
    set Priv(entry,$T,column) $column
    set Priv(entry,$T,element) $element
    set Priv(entry,$T,focus) [focus]

    # Get window coords of the Element
    scan [$T item bbox $item $column $element] "%d %d" x y

    # Get the font used by the Element
    set font [$T item element actual $item $column $element -font]

    # Get the text used by the Element. Could check master Element too.
    set text [$T item element cget $item $column $element -text]

    # Create the Entry widget if needed
    if {[winfo exists $T.entry]} {
	$T.entry delete 0 end
    } else {
	entry $T.entry -borderwidth 1 -highlightthickness 0 -relief solid

	# Accept edit when we lose the focus
	bind $T.entry <FocusOut> {
	    if {[winfo ismapped %W]} {
		TreeCtrl::EntryClose [winfo parent %W] 1
	    }
	}

	# Accept edit on <Return>
	bind $T.entry <KeyPress-Return> {
	    TreeCtrl::EntryClose [winfo parent %W] 1
	    focus $TreeCtrl::Priv(entry,[winfo parent %W],focus)
	}

	# Cancel edit on <Escape>
	bind $T.entry <KeyPress-Escape> {
	    TreeCtrl::EntryClose [winfo parent %W] 0
	    focus $TreeCtrl::Priv(entry,[winfo parent %W],focus)
	}
    }

    # Pesky MouseWheel
    $T notify bind $T.entry <Scroll> {
	TreeCtrl::EntryClose %T 0
	focus $TreeCtrl::Priv(entry,%T,focus)
    }

    $T.entry configure -font $font
    $T.entry insert end $text
    $T.entry selection range 0 end

    set ebw [$T.entry cget -borderwidth]
    if 1 {
	set ex [expr {$x - $ebw - 1}]
	place $T.entry -x $ex -y [expr {$y - $ebw - 1}] \
	    -bordermode outside
    } else {
	set hw [$T cget -highlightthickness]
	set bw [$T cget -borderwidth]
	set ex [expr {$x - $bw - $hw - $ebw - 1}]
	place $T.entry -x $ex -y [expr {$y - $bw - $hw - $ebw - 1}]
    }

    # Make the Entry as wide as the text plus "W" but keep it within the
    # TreeCtrl borders
    set width [font measure $font ${text}W]
    set width [expr {$width + ($ebw + 1) * 2}]
    scan [$T contentbox] "%d %d %d %d" left top right bottom
    if {$ex + $width > $right} {
	set width [expr {$right - $ex}]
    }
    scan [$T item bbox $item $column] "%d %d %d %d" left top right bottom
    if {$ex + $width > $right} {
	set width [expr {$right - $ex}]
    }
    place configure $T.entry -width $width

    focus $T.entry

    return
}

# Like EntryOpen, but Entry widget expands/shrinks during typing
proc ::TreeCtrl::EntryExpanderOpen {T item column element} {

    variable Priv

    set Priv(entry,$T,item) $item
    set Priv(entry,$T,column) $column
    set Priv(entry,$T,element) $element
    set Priv(entry,$T,focus) [focus]

    # Get window coords of the Element
    scan [$T item bbox $item $column $element] "%d %d" x y

    # Get the font used by the Element
    set font [$T item element actual $item $column $element -font]

    set Priv(entry,$T,font) $font

    # Get the text used by the Element. Could check master Element too.
    set text [$T item element cget $item $column $element -text]

    # Create the Entry widget if needed
    if {[winfo exists $T.entry]} {
	$T.entry delete 0 end
    } else {
	entry $T.entry -borderwidth 1 -highlightthickness 0 \
	    -selectborderwidth 0 -relief solid

	# Accept edit when we lose the focus
	bind $T.entry <FocusOut> {
	    if {[winfo ismapped %W]} {
		TreeCtrl::EntryClose [winfo parent %W] 1
	    }
	}

	# Accept edit on <Return>
	bind $T.entry <KeyPress-Return> {
	    TreeCtrl::EntryClose [winfo parent %W] 1
	    focus $TreeCtrl::Priv(entry,[winfo parent %W],focus)
	}

	# Cancel edit on <Escape>
	bind $T.entry <KeyPress-Escape> {
	    TreeCtrl::EntryClose [winfo parent %W] 0
	    focus $TreeCtrl::Priv(entry,[winfo parent %W],focus)
	}

	# Resize as user types
	bind $T.entry <KeyPress> {
	    after idle [list TreeCtrl::EntryExpanderKeypress [winfo parent %W]]
	}
    }

    # Pesky MouseWheel
    $T notify bind $T.entry <Scroll> {
	TreeCtrl::EntryClose %T 0
	focus $TreeCtrl::Priv(entry,%T,focus)
    }

    $T.entry configure -font $font -background [$T cget -background]
    $T.entry insert end $text
    $T.entry selection range 0 end

    set ebw [$T.entry cget -borderwidth]
    set ex [expr {$x - $ebw - 1}]
    place $T.entry -x $ex -y [expr {$y - $ebw - 1}] \
	-bordermode outside

    # Make the Entry as wide as the text plus "W" but keep it within the
    # TreeCtrl borders
    set width [font measure $font ${text}W]
    set width [expr {$width + ($ebw + 1) * 2}]
    scan [$T contentbox] "%d %d %d %d" left top right bottom
    if {$ex + $width > $right} {
	set width [expr {$right - $ex}]
    }
    place configure $T.entry -width $width

    focus $T.entry

    return
}

proc ::TreeCtrl::EntryClose {T accept} {

    variable Priv

    place forget $T.entry
    update

    if {$accept && ([lsearch -exact [$T notify eventnames] Edit] != -1)} {
	$T notify generate <Edit-accept> \
	    [list T $T I $Priv(entry,$T,item) C $Priv(entry,$T,column) \
		 E $Priv(entry,$T,element) t [$T.entry get]]
    }

    $T notify bind $T.entry <Scroll> {}

    return
}

proc ::TreeCtrl::EntryExpanderKeypress {T} {

    variable Priv

    set font $Priv(entry,$T,font)
    set text [$T.entry get]
    set ebw [$T.entry cget -borderwidth]
    set ex [winfo x $T.entry]

    set width [font measure $font ${text}W]
    set width [expr {$width + ($ebw + 1) * 2}]

    scan [$T contentbox] "%d %d %d %d" left top right bottom
    if {$ex + $width > $right} {
	set width [expr {$right - $ex}]
    }

    place configure $T.entry -width $width

    return
}

proc ::TreeCtrl::TextOpen {T item column element {width 0} {height 0}} {
    variable Priv

    set Priv(text,$T,item) $item
    set Priv(text,$T,column) $column
    set Priv(text,$T,element) $element
    set Priv(text,$T,focus) [focus]

    # Get window coords of the Element
    scan [$T item bbox $item $column $element] "%d %d %d %d" x1 y1 x2 y2

    # Get the font used by the Element
    set font [$T item element actual $item $column $element -font]

    # Get the text used by the Element. Could check master Element too.
    set text [$T item element cget $item $column $element -text]

    # Create the Text widget if needed
    if {[winfo exists $T.text]} {
	$T.text delete 1.0 end
    } else {
	text $T.text -borderwidth 1 -highlightthickness 0 -relief solid

	# Accept edit when we lose the focus
	bind $T.text <FocusOut> {
	    if {[winfo ismapped %W]} {
		TreeCtrl::TextClose [winfo parent %W] 1
	    }
	}

	# Accept edit on <Return>
	bind $T.text <KeyPress-Return> {
	    TreeCtrl::TextClose [winfo parent %W] 1
	    focus $TreeCtrl::Priv(text,[winfo parent %W],focus)
	    break
	}

	# Cancel edit on <Escape>
	bind $T.text <KeyPress-Escape> {
	    TreeCtrl::TextClose [winfo parent %W] 0
	    focus $TreeCtrl::Priv(text,[winfo parent %W],focus)
	}
    }

    # Pesky MouseWheel
    $T notify bind $T.text <Scroll> {
	TreeCtrl::TextClose %T 0
	focus $TreeCtrl::Priv(text,%T,focus)
    }

    $T.text tag configure TAG -justify [$T element cget $element -justify]
    $T.text configure -font $font
    $T.text insert end $text
    $T.text tag add sel 1.0 end
    $T.text tag add TAG 1.0 end

    set tbw [$T.text cget -borderwidth]
    set tx [expr {$x1 - $tbw - 1}]
    place $T.text -x $tx -y [expr {$y1 - $tbw - 1}] \
	-width [expr {$x2 - $x1 + ($tbw + 1) * 2}] \
	-height [expr {$y2 - $y1 + ($tbw + 1) * 2}] \
	-bordermode outside

    focus $T.text

    return
}

# Like TextOpen, but Text widget expands/shrinks during typing
proc ::TreeCtrl::TextExpanderOpen {T item column element width} {

    variable Priv

    set Priv(text,$T,item) $item
    set Priv(text,$T,column) $column
    set Priv(text,$T,element) $element
    set Priv(text,$T,focus) [focus]

    # Get window coords of the Element
    scan [$T item bbox $item $column $element] "%d %d %d %d" x1 y1 x2 y2

    set Priv(text,$T,center) [expr {$x1 + ($x2 - $x1) / 2}]

    # Get the font used by the Element
    set font [$T item element actual $item $column $element -font]

    # Get the text used by the Element. Could check master Element too.
    set text [$T item element cget $item $column $element -text]

    set justify [$T element cget $element -justify]

    # Create the Text widget if needed
    if {[winfo exists $T.text]} {
	$T.text delete 1.0 end
    } else {
	text $T.text -borderwidth 1 -highlightthickness 0 \
	    -selectborderwidth 0 -relief solid

	# Accept edit when we lose the focus
	bind $T.text <FocusOut> {
	    if {[winfo ismapped %W]} {
		TreeCtrl::TextClose [winfo parent %W] 1
	    }
	}

	# Accept edit on <Return>
	bind $T.text <KeyPress-Return> {
	    TreeCtrl::TextClose [winfo parent %W] 1
	    focus $TreeCtrl::Priv(text,[winfo parent %W],focus)
	    break
	}

	# Cancel edit on <Escape>
	bind $T.text <KeyPress-Escape> {
	    TreeCtrl::TextClose [winfo parent %W] 0
	    focus $TreeCtrl::Priv(text,[winfo parent %W],focus)
	}

	# Resize as user types
	bind $T.text <KeyPress> {
	    after idle TreeCtrl::TextExpanderKeypress [winfo parent %W]
	}
    }

    # Pesky MouseWheel
    $T notify bind $T.text <Scroll> {
	TreeCtrl::TextClose %T 0
	focus $TreeCtrl::Priv(text,%T,focus)
    }

    $T.text tag configure TAG -justify $justify
    $T.text configure -font $font -background [$T cget -background]
    $T.text insert end $text
    $T.text tag add sel 1.0 end
    $T.text tag add TAG 1.0 end

    set Priv(text,$T,font) $font
    set Priv(text,$T,justify) $justify
    set Priv(text,$T,width) $width

    scan [textlayout $font $text -justify $justify -width $width] \
	"%d %d" width height

    set tbw [$T.text cget -borderwidth]
    set tx [expr {$x1 - $tbw - 1}]
    place $T.text -x $tx -y [expr {$y1 - $tbw - 1}] \
	-width [expr {$width + ($tbw + 1) * 2}] \
	-height [expr {$height + ($tbw + 1) * 2}] \
	-bordermode outside

    focus $T.text

    return
}

proc ::TreeCtrl::TextClose {T accept} {

    variable Priv

    place forget $T.text
    update

    if {$accept && ([lsearch -exact [$T notify eventnames] Edit] != -1)} {
	$T notify generate <Edit-accept> \
	    [list T $T I $Priv(text,$T,item) C $Priv(text,$T,column) \
		 E $Priv(text,$T,element) t [$T.text get 1.0 end-1c]]
    }

    $T notify bind $T.text <Scroll> {}

    return
}

proc ::TreeCtrl::TextExpanderKeypress {T} {

    variable Priv

    set font $Priv(text,$T,font)
    set justify $Priv(text,$T,justify)
    set width $Priv(text,$T,width)
    set center $Priv(text,$T,center)

    set text [$T.text get 1.0 end-1c]

    scan [textlayout $font $text -justify $justify -width $width] \
	"%d %d" width height

    set tbw [$T.text cget -borderwidth]
    place configure $T.text \
	-x [expr {$center - $width / 2 - $tbw - 1}] \
	-width [expr {$width + ($tbw + 1) * 2}] \
	-height [expr {$height + ($tbw + 1) * 2}]

    $T.text tag add TAG 1.0 end

    return
}

