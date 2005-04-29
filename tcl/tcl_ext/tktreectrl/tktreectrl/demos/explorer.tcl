set Dir [file dirname [info library]]

proc DemoExplorerAux {scriptDir scriptFile} {

	set T .f2.f1.t

	set clicks [clock clicks]
	set globDirs [glob -nocomplain -types d -dir $::Dir *]
	set clickGlobDirs [expr {[clock clicks] - $clicks}]

	set clicks [clock clicks]
	set list [lsort -dictionary $globDirs]
	set clickSortDirs [expr {[clock clicks] - $clicks}]

	set clicks [clock clicks]
	foreach file $list $scriptDir
	set clickAddDirs [expr {[clock clicks] - $clicks}]

	set clicks [clock clicks]
	set globFiles [glob -nocomplain -types f -dir $::Dir *]
	set clickGlobFiles [expr {[clock clicks] - $clicks}]

	set clicks [clock clicks]
	set list [lsort -dictionary $globFiles]
	set clickSortFiles [expr {[clock clicks] - $clicks}]

	set clicks [clock clicks]
	foreach file $list $scriptFile
	set clickAddFiles [expr {[clock clicks] - $clicks}]

	set gd [ClicksToSeconds $clickGlobDirs]
	set sd [ClicksToSeconds $clickSortDirs]
	set ad [ClicksToSeconds $clickAddDirs]
	set gf [ClicksToSeconds $clickGlobFiles]
	set sf [ClicksToSeconds $clickSortFiles]
	set af [ClicksToSeconds $clickAddFiles]
	puts "dirs([llength $globDirs]) glob/sort/add $gd/$sd/$ad    files([llength $globFiles]) glob/sort/add $gf/$sf/$af"

	set ::TreeCtrl::Priv(DirCnt,$T) [llength $globDirs]

	return
}

#
# Demo: explorer files
#
proc DemoExplorerDetails {} {

	set T .f2.f1.t

	set height [font metrics [$T cget -font] -linespace]
	if {$height < 18} {
		set height 18
	}
	$T configure -showroot no -showbuttons no -showlines no -itemheight $height \
		-selectmode extended -xscrollincrement 20 \
		-scrollmargin 16 -xscrolldelay "500 50" -yscrolldelay "500 50"

	InitPics small-*

	$T column configure 0 -text Name -tag name -width 200 \
		-arrow up -arrowpad 6
	$T column configure 1 -text Size -tag size -justify right -width 60 \
		-arrowside left -arrowgravity right
	$T column configure 2 -text Type -tag type -width 120
	$T column configure 3 -text Modified -tag modified -width 120

	$T element create e1 image -image {small-folderSel {selected} small-folder {}}
	$T element create e2 text -fill [list $::SystemHighlightText {selected focus}] \
		-lines 1
	$T element create txtType text -lines 1
	$T element create txtSize text -datatype integer -format "%dKB" -lines 1
	$T element create txtDate text -datatype time -format "%d/%m/%y %I:%M %p" -lines 1
	$T element create e4 rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] -showfocus yes

	# image + text
	set S [$T style create styName -orient horizontal]
	$T style elements $S {e4 e1 e2}
	$T style layout $S e1 -expand ns
	$T style layout $S e2 -padx {2 0} -squeeze x -expand ns
	$T style layout $S e4 -union [list e2] -iexpand ns -ipadx 2

	# column 1: text
	set S [$T style create stySize]
	$T style elements $S txtSize
	$T style layout $S txtSize -padx 6 -squeeze x -expand ns

	# column 2: text
	set S [$T style create styType]
	$T style elements $S txtType
	$T style layout $S txtType -padx 6 -squeeze x -expand ns

	# column 3: text
	set S [$T style create styDate]
	$T style elements $S txtDate
	$T style layout $S txtDate -padx 6 -squeeze x -expand ns

	set ::TreeCtrl::Priv(edit,$T) {e2}
	set ::TreeCtrl::Priv(sensitive,$T) {
		{name styName e1 e2}
	}
	set ::TreeCtrl::Priv(dragimage,$T) {
		{name styName e1 e2}
	}

	$T notify bind $T <Edit-accept> {
		%T item text %I 0 %t
	}

	set scriptDir {
		set item [$T item create]
		$T item style set $item 0 styName 2 styType 3 styDate
		$T item complex $item \
			[list [list e2 -text [file tail $file]]] \
			[list] \
			[list [list txtType -text "Folder"]] \
			[list [list txtDate -data [file mtime $file]]]
		$T item lastchild root $item
	}

	set scriptFile {
		set item [$T item create]
		$T item style set $item 0 styName 1 stySize 2 styType 3 styDate
		switch [file extension $file] {
			.dll { set img small-dll }
			.exe { set img small-exe }
			.txt { set img small-txt }
			default { set img small-file }
		}
		set type [string toupper [file extension $file]]
		if {$type ne ""} {
			set type "[string range $type 1 end] "
		}
		append type "File"
		$T item complex $item \
			[list [list e1 -image [list ${img}Sel {selected} $img {}]] [list e2 -text [file tail $file]]] \
			[list [list txtSize -data [expr {[file size $file] / 1024 + 1}]]] \
			[list [list txtType -text $type]] \
			[list [list txtDate -data [file mtime $file]]]
		$T item lastchild root $item
	}

	DemoExplorerAux $scriptDir $scriptFile

	set ::SortColumn 0
	$T notify bind $T <Header-invoke> { ExplorerHeaderInvoke %T %C }

	bindtags $T [list $T TreeCtrlFileList TreeCtrl [winfo toplevel $T] all]

	return
}

proc ExplorerHeaderInvoke {T C} {
	global SortColumn
	if {$C == $SortColumn} {
		if {[$T column cget $SortColumn -arrow] eq "down"} {
			set order -increasing
			set arrow up
		} else {
			set order -decreasing
			set arrow down
		}
	} else {
		if {[$T column cget $SortColumn -arrow] eq "down"} {
			set order -decreasing
			set arrow down
		} else {
			set order -increasing
			set arrow up
		}
		$T column configure $SortColumn -arrow none
		set SortColumn $C
	}
	$T column configure $C -arrow $arrow
	set dirCount $::TreeCtrl::Priv(DirCnt,$T)
	set lastDir [expr {$dirCount - 1}]
	switch [$T column cget $C -tag] {
		name {
			if {$dirCount} {
				$T item sort root $order -last "root child $lastDir" -column $C -dictionary
			}
			if {$dirCount < [$T numitems] - 1} {
				$T item sort root $order -first "root child $dirCount" -column $C -dictionary
			}
		}
		size {
			if {$dirCount < [$T numitems] - 1} {
				$T item sort root $order -first "root child $dirCount" -column $C -integer -column name -dictionary
			}
		}
		type {
			if {$dirCount < [$T numitems] - 1} {
				$T item sort root $order -first "root child $dirCount" -column $C -dictionary -column name -dictionary
			}
		}
		modified {
			if {$dirCount} {
				$T item sort root $order -last "root child $dirCount prevsibling" -column $C -integer -column name -dictionary
			}
			if {$dirCount < [$T numitems] - 1} {
				$T item sort root $order -first "root child $dirCount" -column $C -integer -column name -dictionary
			}
		}
	}
	return
}

proc DemoExplorerLargeIcons {} {

	set T .f2.f1.t

	# Item height is 32 for icon, 4 padding, 3 lines of text
	set itemHeight [expr {32 + 4 + [font metrics [$T cget -font] -linespace] * 3}]

	$T configure -showroot no -showbuttons no -showlines no \
		-selectmode extended -wrap window -orient horizontal \
		-itemheight $itemHeight -showheader no \
		-scrollmargin 16 -xscrolldelay "500 50" -yscrolldelay "500 50"

	InitPics big-*

	$T column configure 0 -width 75

	$T element create elemImg image -image {big-folderSel {selected} big-folder {}}
	$T element create elemTxt text -fill [list $::SystemHighlightText {selected focus}] \
		-justify center -lines 1 -width 71 -wrap word
	$T element create elemSel rect -fill [list $::SystemHighlight {selected focus} gray {selected}] -showfocus yes

	# image + text
	set S [$T style create STYLE -orient vertical]
	$T style elements $S {elemSel elemImg elemTxt}
	$T style layout $S elemImg -expand we
	$T style layout $S elemTxt -pady {4 0} -padx 2 -squeeze x -expand we
	$T style layout $S elemSel -union [list elemTxt]

	set ::TreeCtrl::Priv(edit,$T) {elemTxt}
	set ::TreeCtrl::Priv(sensitive,$T) {
		{0 STYLE elemImg elemTxt}
	}
	set ::TreeCtrl::Priv(dragimage,$T) {
		{0 STYLE elemImg elemTxt}
	}

	$T notify bind $T <Edit-accept> {
		%T item text %I 0 %t
	}

	set scriptDir {
		set item [$T item create]
		$T item style set $item 0 STYLE
		$T item text $item 0 [file tail $file]
		$T item lastchild root $item
	}

	set scriptFile {
		set item [$T item create]
		$T item style set $item 0 STYLE
		switch [file extension $file] {
			.dll { set img big-dll }
			.exe { set img big-exe }
			.txt { set img big-txt }
			default { set img big-file }
		}
		set type [string toupper [file extension $file]]
		if {$type ne ""} {
			set type "[string range $type 1 end] "
		}
		append type "File"
		$T item complex $item \
			[list [list elemImg -image [list ${img}Sel {selected} $img {}]] [list elemTxt -text [file tail $file]]]
		$T item lastchild root $item
	}

	DemoExplorerAux $scriptDir $scriptFile

	$T activate [$T index "root firstchild"]

	$T notify bind $T <ActiveItem> {
		%T item element configure %p 0 elemTxt -lines {}
		%T item element configure %c 0 elemTxt -lines 3
	}

	bindtags $T [list $T TreeCtrlFileList TreeCtrl [winfo toplevel $T] all]

	return
}

# Tree is horizontal, wrapping occurs at right edge of window, each item
# is as wide as the smallest needed multiple of 110 pixels
proc DemoExplorerSmallIcons {} {
	set T .f2.f1.t
	DemoExplorerList
	$T configure -orient horizontal -xscrollincrement 0
	$T column configure 0 -width {} -stepwidth 110 -widthhack no
}

# Tree is vertical, wrapping occurs at bottom of window, each range has the
# same width (as wide as the longest item), xscrollincrement is by range
proc DemoExplorerList {} {

	set T .f2.f1.t

	set height [font metrics [$T cget -font] -linespace]
	if {$height < 18} {
		set height 18
	}
	$T configure -showroot no -showbuttons no -showlines no -itemheight $height \
		-selectmode extended -wrap window -showheader no \
		-scrollmargin 16 -xscrolldelay "500 50" -yscrolldelay "500 50"

	InitPics small-*

	$T column configure 0 -widthhack yes

	$T element create elemImg image -image {small-folderSel {selected} small-folder {}}
	$T element create elemTxt text -fill [list $::SystemHighlightText {selected focus}] \
		-lines 1
	$T element create elemSel rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] -showfocus yes

	# image + text
	set S [$T style create STYLE]
	$T style elements $S {elemSel elemImg elemTxt}
	$T style layout $S elemImg -expand ns
	$T style layout $S elemTxt -squeeze x -expand ns -padx {2 0}
	$T style layout $S elemSel -union [list elemTxt] -iexpand ns -ipadx 2

	set ::TreeCtrl::Priv(edit,$T) {elemTxt}
	set ::TreeCtrl::Priv(sensitive,$T) {
		{0 STYLE elemImg elemTxt}
	}
	set ::TreeCtrl::Priv(dragimage,$T) {
		{0 STYLE elemImg elemTxt}
	}

	$T notify bind $T <Edit-accept> {
		%T item text %I 0 %t
	}

	set scriptDir {
		set item [$T item create]
		$T item style set $item 0 STYLE
		$T item text $item 0 [file tail $file]
		$T item lastchild root $item
	}

	set scriptFile {
		set item [$T item create]
		$T item style set $item 0 STYLE
		switch [file extension $file] {
			.dll { set img small-dll }
			.exe { set img small-exe }
			.txt { set img small-txt }
			default { set img small-file }
		}
		set type [string toupper [file extension $file]]
		if {$type ne ""} {
			set type "[string range $type 1 end] "
		}
		append type "File"
		$T item complex $item \
			[list [list elemImg -image [list ${img}Sel {selected} $img {}]] [list elemTxt -text [file tail $file]]]
		$T item lastchild root $item
	}

	DemoExplorerAux $scriptDir $scriptFile

	$T activate [$T item firstchild root]

	bindtags $T [list $T TreeCtrlFileList TreeCtrl [winfo toplevel $T] all]

	return
}

