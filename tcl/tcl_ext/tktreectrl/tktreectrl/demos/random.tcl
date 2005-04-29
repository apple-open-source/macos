set RandomN 500

#
# Demo: random N items
#
proc DemoRandom {} {

	set T .f2.f1.t

	InitPics folder-* small-*

	set height [font metrics [$T cget -font] -linespace]
	if {$height < 18} {
		set height 18
	}
	$T configure -itemheight $height -selectmode extended \
		-showroot yes -showrootbutton yes -showbuttons yes -showlines yes \
		-scrollmargin 16 -xscrolldelay "500 50" -yscrolldelay "500 50"

	$T column configure 0 -expand yes -text Item -itembackground {#e0e8f0 {}} -tag item
	$T column configure 1 -text Parent -justify center -itembackground {gray90 {}} -tag parent
	$T column configure 2 -text Depth -justify center -itembackground {linen {}} -tag depth

	$T element create e1 image -image {folder-open {open} folder-closed {}}
	$T element create e2 image -image small-file
	$T element create e3 text \
		-fill [list $::SystemHighlightText {selected focus}]
	$T element create e4 text -fill blue
	$T element create e6 text
	$T element create e5 rect -showfocus yes \
		-fill [list $::SystemHighlight {selected focus} gray {selected !focus}]

	$T style create s1
	$T style elements s1 {e5 e1 e3 e4}
	$T style layout s1 e1 -padx {0 4} -expand ns
	$T style layout s1 e3 -padx {0 4} -expand ns
	$T style layout s1 e4 -padx {0 6} -expand ns
	$T style layout s1 e5 -union [list e3] -iexpand ns -ipadx 2

	$T style create s2
	$T style elements s2 {e5 e2 e3}
	$T style layout s2 e2 -padx {0 4} -expand ns
	$T style layout s2 e3 -padx {0 4} -expand ns
	$T style layout s2 e5 -union [list e3] -iexpand ns -ipadx 2

	$T style create s3
	$T style elements s3 {e6}
	$T style layout s3 e6 -padx 6 -expand ns

	set ::TreeCtrl::Priv(sensitive,$T) {
		{item s1 e5 e1 e3}
		{item s2 e5 e2 e3}
	}
	set ::TreeCtrl::Priv(dragimage,$T) {
		{item s1 e1 e3}
		{item s2 e2 e3}
	}

	set clicks [clock clicks]
	for {set i 1} {$i < $::RandomN} {incr i} {
		$T item create
		while 1 {
			set j [expr {int(rand() * $i)}]
			if {[$T depth $j] < 5} break
		}
		if {rand() * 2 > 1} {
			$T collapse $i
		}
		if {rand() * 2 > 1} {
			$T item lastchild $j $i
		} else {
			$T item firstchild $j $i
		}
	}
	puts "created $::RandomN-1 items in [expr [clock clicks] - $clicks] clicks"
	set clicks [clock clicks]
	for {set i 0} {$i < $::RandomN} {incr i} {
		set numChildren [$T item numchildren $i]
		if {$numChildren}  {
			$T item hasbutton $i yes
			$T item style set $i 0 s1 1 s3 2 s3
			$T item complex $i \
				[list [list e3 -text "Item $i"] [list e4 -text "($numChildren)"]] \
				[list [list e6 -text "[$T item parent $i]"]] \
				[list [list e6 -text "[$T depth $i]"]]
		} else {
			$T item style set $i 1 s3 2 s3 0 s2
			$T item complex $i \
				[list [list e3 -text "Item $i"]] \
				[list [list e6 -text "[$T item parent $i]"]] \
				[list [list e6 -text "[$T depth $i]"]]
		}
	}
	puts "configured $::RandomN items in [expr [clock clicks] - $clicks] clicks"

	bind TreeCtrlRandom <Double-ButtonPress-1> {
		TreeCtrl::DoubleButton1 %W %x %y
		break
	}
	bind TreeCtrlRandom <Control-ButtonPress-1> {
		set TreeCtrl::Priv(selectMode) toggle
		TreeCtrl::RandomButton1 %W %x %y
		break
	}
	bind TreeCtrlRandom <Shift-ButtonPress-1> {
		set TreeCtrl::Priv(selectMode) add
		TreeCtrl::RandomButton1 %W %x %y
		break
	}
	bind TreeCtrlRandom <ButtonPress-1> {
		set TreeCtrl::Priv(selectMode) set
		TreeCtrl::RandomButton1 %W %x %y
		break
	}
	bind TreeCtrlRandom <Button1-Motion> {
		TreeCtrl::RandomMotion1 %W %x %y
		break
	}
	bind TreeCtrlRandom <Button1-Leave> {
		TreeCtrl::RandomLeave1 %W %x %y
		break
	}
	bind TreeCtrlRandom <ButtonRelease-1> {
		TreeCtrl::RandomRelease1 %W %x %y
		break
	}

	bindtags $T [list $T TreeCtrlRandom TreeCtrl [winfo toplevel $T] all]

	return
}

proc TreeCtrl::RandomButton1 {T x y} {
	variable Priv
	focus $T
	set id [$T identify $x $y]
	puts $id
	set Priv(buttonMode) ""

	# Click outside any item
	if {$id eq ""} {
		$T selection clear

	# Click in header
	} elseif {[lindex $id 0] eq "header"} {
		ButtonPress1 $T $x $y

	# Click in item
	} else {
		foreach {where item arg1 arg2 arg3 arg4} $id {}
		switch $arg1 {
			button {
				$T toggle $item
			}
			line {
				$T toggle $arg2
			}
			column {
				set ok 0
				# Clicked an element
				if {[llength $id] == 6} {
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
				if {!$ok} {
					$T selection clear
					return
				}

				set Priv(drag,motion) 0
				set Priv(drag,x) [$T canvasx $x]
				set Priv(drag,y) [$T canvasy $y]
				set Priv(drop) ""

				if {$Priv(selectMode) eq "add"} {
					BeginExtend $T $item
				} elseif {$Priv(selectMode) eq "toggle"} {
					BeginToggle $T $item
				} elseif {![$T selection includes $item]} {
					BeginSelect $T $item
				}
				$T activate $item

				if {[$T selection includes $item]} {
					set Priv(buttonMode) drag
				}
			}
		}
	}
	return
}
proc TreeCtrl::RandomMotion1 {T x y} {
	variable Priv
	switch $Priv(buttonMode) {
		"resize" -
		"header" {
			Motion1 $T $x $y
		}
		"drag" {
			RandomAutoScanCheck $T $x $y
			RandomMotion $T $x $y
		}
	}
	return
}
proc TreeCtrl::RandomMotion {T x y} {
	variable Priv
	switch $Priv(buttonMode) {
		"resize" -
		"header" {
			Motion1 $T $x $y
		}
		"drag" {
			# Detect initial mouse movement
			if {!$Priv(drag,motion)} {
				set Priv(selection) [$T selection get]
				set Priv(drop) ""
				$T dragimage clear
				# For each selected item, add 2nd and 3rd elements of
				# column "item" to the dragimage
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
			}

			# Find the item under the cursor
			set cursor X_cursor
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
				if {[lsearch -exact $Priv(sensitive,$T) $E] != -1} {
					set ok 1
				}
			}
			if {$ok} {
				# If the item is not in the pre-drag selection
				# (i.e. not being dragged) see if we can drop on it
				if {[lsearch -exact $Priv(selection) $item] == -1} {
					set drop $item
					# We can drop if dragged item isn't an ancestor
					foreach item2 $Priv(selection) {
						if {[$T item isancestor $item2 $item]} {
							set drop ""
							break
						}
					}
					if {$drop ne ""} {
						scan [$T item bbox $drop] "%d %d %d %d" x1 y1 x2 y2
						if {$y < $y1 + 3} {
							set cursor top_side
							set Priv(drop,pos) prevsibling
						} elseif {$y >= $y2 - 3} {
							set cursor bottom_side
							set Priv(drop,pos) nextsibling
						} else {
							set cursor ""
							set Priv(drop,pos) lastchild
						}
					}
				}
			}

			if {[$T cget -cursor] ne $cursor} {
				$T configure -cursor $cursor
			}

			# Select the item under the cursor (if any) and deselect
			# the previous drop-item (if any)
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
proc TreeCtrl::RandomLeave1 {T x y} {
	variable Priv
	# This is called when I do ButtonPress-1 on Unix for some reason,
	# and buttonMode is undefined.
	if {![info exists Priv(buttonMode)]} return
	switch $Priv(buttonMode) {
		"header" {
			Leave1 $T $x $y
		}
	}
	return
}
proc TreeCtrl::RandomRelease1 {T x y} {
	variable Priv
	switch $Priv(buttonMode) {
		"resize" -
		"header" {
			Release1 $T $x $y
		}
		"drag" {
			AutoScanCancel $T
			$T dragimage visible no
			$T selection modify {} $Priv(drop)
			$T configure -cursor ""
			if {$Priv(drop) ne ""} {
				RandomDrop $T $Priv(drop) $Priv(selection) $Priv(drop,pos)
			}
		}
	}
	set Priv(buttonMode) ""
	return
}

proc RandomDrop {T target source pos} {
	set parentList {}
	switch -- $pos {
		lastchild { set parent $target }
		prevsibling { set parent [$T item parent $target] }
		nextsibling { set parent [$T item parent $target] }
	}
	foreach item $source {

		# Ignore any item whose ancestor is also selected
		set ignore 0
		foreach ancestor [$T item ancestors $item] {
			if {[lsearch -exact $source $ancestor] != -1} {
				set ignore 1
				break
			}
		}
		if {$ignore} continue

		# Update the old parent of this moved item later
		if {[lsearch -exact $parentList $item] == -1} {
			lappend parentList [$T item parent $item]
		}

		# Add to target
		$T item $pos $target $item

		# Update text: parent
		$T item element configure $item parent e6 -text $parent

		# Update text: depth
		$T item element configure $item depth e6 -text [$T depth $item]

		# Recursively update text: depth
		set itemList [$T item firstchild $item]
		while {[llength $itemList]} {
			# Pop
			set item [lindex $itemList end]
			set itemList [lrange $itemList 0 end-1]

			$T item element configure $item depth e6 -text [$T depth $item]

			set item2 [$T item nextsibling $item]
			if {$item2 ne ""} {
				# Push
				lappend itemList $item2
			}
			set item2 [$T item firstchild $item]
			if {$item2 ne ""} {
				# Push
				lappend itemList $item2
			}
		}
	}

	# Update items that lost some children
	foreach item $parentList {
		set numChildren [$T item numchildren $item]
		if {$numChildren == 0} {
			$T item hasbutton $item no
			$T item style map $item item s2 {e3 e3}
		} else {
			$T item element configure $item item e4 -text "($numChildren)"
		}
	}

	# Update the target that gained some children
	if {[$T item style set $parent 0] ne "s1"} {
		$T item hasbutton $parent yes
		$T item style map $parent item s1 {e3 e3}
	}
	set numChildren [$T item numchildren $parent]
	$T item element configure $parent item e4 -text "($numChildren)"
	return
}

# Same as TreeCtrl::AutoScanCheck, but calls RandomMotion and
# RandomAutoScanCheckAux
proc TreeCtrl::RandomAutoScanCheck {T x y} {
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
					RandomMotion $T $x $y
				}
			}
			set Priv(autoscan,afterId,$T) [after $delay [list TreeCtrl::RandomAutoScanCheckAux $T]]
		}
		return
	}
	AutoScanCancel $T
	return
}

proc TreeCtrl::RandomAutoScanCheckAux {T} {
	variable Priv
	unset Priv(autoscan,afterId,$T)
	set x [winfo pointerx $T]
	set y [winfo pointery $T]
	set x [expr {$x - [winfo rootx $T]}]
	set y [expr {$y - [winfo rooty $T]}]
	RandomAutoScanCheck $T $x $y
	return
}

#
# Demo: random N items, button images
#
proc DemoRandom2 {} {

	set T .f2.f1.t

	DemoRandom

	InitPics mac-*

	$T configure -openbuttonimage mac-collapse -closedbuttonimage mac-expand \
		-showlines no

	return
}

