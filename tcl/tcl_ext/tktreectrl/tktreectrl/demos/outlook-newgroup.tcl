#
# Demo: Outlook Express newsgroup messages
#
proc DemoOutlookNewsgroup {} {

	global Message

	InitPics outlook-*

	set T .f2.f1.t

	set height [font metrics [$T cget -font] -linespace]
	if {$height < 18} {
		set height 18
	}
	$T configure -itemheight $height -selectmode browse \
		-showroot no -showrootbutton no -showbuttons yes -showlines no

	$T column configure 0 -image outlook-clip -tag clip
	$T column configure 1 -image outlook-arrow -tag arrow
	$T column configure 2 -image outlook-watch -tag watch
	$T column configure 3 -text Subject -width 250 -tag subject
	$T column configure 4 -text From -width 150 -tag from
	$T column configure 5 -text Sent -width 150 -tag sent
	$T column configure 6 -text Size -width 60 -justify right -tag size

	# Would be nice if I could specify a column -tag too
	$T configure -treecolumn 3

	# State for a read message
	$T state define read

	# State for a message with unread descendants
	$T state define unread

	$T element create elemImg image -image {
		outlook-read-2Sel {selected read unread !open}
		outlook-read-2 {read unread !open}
		outlook-readSel {selected read}
		outlook-read {read}
		outlook-unreadSel {selected}
		outlook-unread {}
	}
	$T element create elemTxt text -fill [list $::SystemHighlightText {selected focus}] \
		-font [list "[$T cget -font] bold" {read unread !open} "[$T cget -font] bold" {!read}] -lines 1
	$T element create sel.e rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] -open e -showfocus yes
	$T element create sel.w rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] -open w -showfocus yes
	$T element create sel.we rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] -open we -showfocus yes

	# Image + text
	set S [$T style create s1]
	$T style elements $S {sel.e elemImg elemTxt}
	$T style layout $S elemImg -expand ns
	$T style layout $S elemTxt -padx {2 6} -squeeze x -expand ns
	$T style layout $S sel.e -union [list elemTxt] -iexpand nes -ipadx {2 0}

	# Text
	set S [$T style create s2.we]
	$T style elements $S {sel.we elemTxt}
	$T style layout $S elemTxt -padx 6 -squeeze x -expand ns
	$T style layout $S sel.we -detach yes -iexpand es

	# Text
	set S [$T style create s2.w]
	$T style elements $S {sel.w elemTxt}
	$T style layout $S elemTxt -padx 6 -squeeze x -expand ns
	$T style layout $S sel.w -detach yes -iexpand es

	set msgCnt 100

	set thread 0
	set Message(count,0) 0
	for {set i 1} {$i < $msgCnt} {incr i} {
		$T item create
		while 1 {
			set j [expr {int(rand() * $i)}]
			if {$j == 0} break
			if {[$T depth $j] == 5} continue
			if {$Message(count,$Message(thread,$j)) == 15} continue
			break
		}
		$T item lastchild $j $i

		set Message(read,$i) [expr rand() * 2 > 1]
		if {$j == 0} {
			set Message(thread,$i) [incr thread]
			set Message(seconds,$i) [expr {[clock seconds] - int(rand() * 500000)}]
			set Message(seconds2,$i) $Message(seconds,$i)
			set Message(count,$thread) 1
		} else {
			set Message(thread,$i) $Message(thread,$j)
			set Message(seconds,$i) [expr {$Message(seconds2,$j) + int(rand() * 10000)}]
			set Message(seconds2,$i) $Message(seconds,$i)
			set Message(seconds2,$j) $Message(seconds,$i)
			incr Message(count,$Message(thread,$j))
		}
	}

	for {set i 1} {$i < $msgCnt} {incr i} {
		set subject "This is thread number $Message(thread,$i)"
		set from somebody@somewhere.net
		set sent [clock format $Message(seconds,$i) -format "%d/%m/%y %I:%M %p"]
		set size [expr {1 + int(rand() * 10)}]KB

		# This message has been read
		if {$Message(read,$i)} {
			$T item state set $i read
		}

		# This message has unread descendants
		if {[AnyUnreadDescendants $T $i]} {
			$T item state set $i unread
		}

		if {[$T item numchildren $i]} {
			$T item hasbutton $i yes

			# Collapse some messages
			if {rand() * 2 > 1} {
				$T collapse $i
			}
		}

		$T item style set $i 3 s1 4 s2.we 5 s2.we 6 s2.w
		$T item text $i 3 $subject 4 $from 5 $sent 6 $size
	}

	# Do something when the selection changes
	$T notify bind $T <Selection> {

		# One item is selected
		if {[%T selection count] == 1} {
			if {[info exists Message(afterId)]} {
				after cancel $Message(afterId)
			}
			set Message(afterId,item) [lindex [%T selection get] 0]
			set Message(afterId) [after 500 MessageReadDelayed]
		}
	}

	return
}

proc MessageReadDelayed {} {

	global Message

	set T .f2.f1.t

	unset Message(afterId)
	set I $Message(afterId,item)
	if {![$T selection includes $I]} return

	# This message is not read
	if {!$Message(read,$I)} {

		# Read the message
		$T item state set $I read
		set Message(read,$I) 1

		# Check ancestors (except root)
		foreach I2 [lrange [$T item ancestors $I] 0 end-1] {

			# This ancestor has no more unread descendants
			if {![AnyUnreadDescendants $T $I2]} {
				$T item state set $I2 !unread
			}
		}
	}
}

# Alternate implementation which does not rely on run-time states
proc DemoOutlookNewsgroup2 {} {

	global Message

	InitPics outlook-*

	set T .f2.f1.t

	set height [font metrics [$T cget -font] -linespace]
	if {$height < 18} {
		set height 18
	}
	$T configure -itemheight $height -selectmode browse \
		-showroot no -showrootbutton no -showbuttons yes -showlines no

	$T column configure 0 -image outlook-clip -tag clip
	$T column configure 1 -image outlook-arrow -tag arrow
	$T column configure 2 -image outlook-watch -tag watch
	$T column configure 3 -text Subject -width 250 -tag subject
	$T column configure 4 -text From -width 150 -tag from
	$T column configure 5 -text Sent -width 150 -tag sent
	$T column configure 6 -text Size -width 60 -justify right -tag size

	$T configure -treecolumn 3

	$T element create image.unread image -image outlook-unread
	$T element create image.read image -image outlook-read
	$T element create image.read2 image -image outlook-read-2
	$T element create text.read text -fill [list $::SystemHighlightText {selected focus}] \
		-lines 1
	$T element create text.unread text -fill [list $::SystemHighlightText {selected focus}] \
		-font [list "[$T cget -font] bold"] -lines 1
	$T element create sel.e rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] -open e -showfocus yes
	$T element create sel.w rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] -open w -showfocus yes
	$T element create sel.we rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] -open we -showfocus yes

	# Image + text
	set S [$T style create unread]
	$T style elements $S {sel.e image.unread text.unread}
	$T style layout $S image.unread -expand ns
	$T style layout $S text.unread -padx {2 6} -squeeze x -expand ns
	$T style layout $S sel.e -union [list text.unread] -iexpand nes -ipadx {2 0}

	# Image + text
	set S [$T style create read]
	$T style elements $S {sel.e image.read text.read}
	$T style layout $S image.read -expand ns
	$T style layout $S text.read -padx {2 6} -squeeze x -expand ns
	$T style layout $S sel.e -union [list text.read] -iexpand nes -ipadx {2 0}

	# Image + text
	set S [$T style create read2]
	$T style elements $S {sel.e image.read2 text.unread}
	$T style layout $S image.read2 -expand ns
	$T style layout $S text.unread -padx {2 6} -squeeze x -expand ns
	$T style layout $S sel.e -union [list text.unread] -iexpand nes -ipadx {2 0}

	# Text
	set S [$T style create unread.we]
	$T style elements $S {sel.we text.unread}
	$T style layout $S text.unread -padx 6 -squeeze x -expand ns
	$T style layout $S sel.we -detach yes -iexpand es

	# Text
	set S [$T style create read.we]
	$T style elements $S {sel.we text.read}
	$T style layout $S text.read -padx 6 -squeeze x -expand ns
	$T style layout $S sel.we -detach yes -iexpand es

	# Text
	set S [$T style create unread.w]
	$T style elements $S {sel.w text.unread}
	$T style layout $S text.unread -padx 6 -squeeze x -expand ns
	$T style layout $S sel.w -detach yes -iexpand es

	# Text
	set S [$T style create read.w]
	$T style elements $S {sel.w text.read}
	$T style layout $S text.read -padx 6 -squeeze x -expand ns
	$T style layout $S sel.w -detach yes -iexpand es

	set msgCnt 100

	set thread 0
	set Message(count,0) 0
	for {set i 1} {$i < $msgCnt} {incr i} {
		$T item create
		while 1 {
			set j [expr {int(rand() * $i)}]
			if {$j == 0} break
			if {[$T depth $j] == 5} continue
			if {$Message(count,$Message(thread,$j)) == 15} continue
			break
		}
		$T item lastchild $j $i

		set Message(read,$i) [expr rand() * 2 > 1]
		if {$j == 0} {
			set Message(thread,$i) [incr thread]
			set Message(seconds,$i) [expr {[clock seconds] - int(rand() * 500000)}]
			set Message(seconds2,$i) $Message(seconds,$i)
			set Message(count,$thread) 1
		} else {
			set Message(thread,$i) $Message(thread,$j)
			set Message(seconds,$i) [expr {$Message(seconds2,$j) + int(rand() * 10000)}]
			set Message(seconds2,$i) $Message(seconds,$i)
			set Message(seconds2,$j) $Message(seconds,$i)
			incr Message(count,$Message(thread,$j))
		}
	}

	for {set i 1} {$i < $msgCnt} {incr i} {
		set subject "This is thread number $Message(thread,$i)"
		set from somebody@somewhere.net
		set sent [clock format $Message(seconds,$i) -format "%d/%m/%y %I:%M %p"]
		set size [expr {1 + int(rand() * 10)}]KB
		if {$Message(read,$i)} {
			set style read
			set style2 read
		} else {
			set style unread
			set style2 unread
		}
		$T item style set $i 3 $style 4 $style2.we 5 $style2.we 6 $style2.w
		$T item text $i 3 $subject 4 $from 5 $sent 6 $size
		if {[$T item numchildren $i]} {
			$T item hasbutton $i yes
		}
	}

	$T notify bind $T <Selection> {
		if {[%T selection count] == 1} {
			set I [lindex [%T selection get] 0]
			if {!$Message(read,$I)} {
				if {[%T item isopen $I] || ![AnyUnreadDescendants %T $I]} {
					# unread ->read
					%T item style map $I subject read {text.unread text.read}
					%T item style map $I from read.we {text.unread text.read}
					%T item style map $I sent read.we {text.unread text.read}
					%T item style map $I size read.w {text.unread text.read}
				} else {
					# unread -> read2
					%T item style map $I subject read2 {text.unread text.unread}
				}
				set Message(read,$I) 1
				DisplayStylesInItem $I
			}
		}
	}

	$T notify bind $T <Expand-after> {
		if {$Message(read,%I) && [AnyUnreadDescendants %T %I]} {
			# read2 -> read
			%T item style map %I subject read {text.unread text.read}
			# unread -> read
			%T item style map %I from read.we {text.unread text.read}
			%T item style map %I sent read.we {text.unread text.read}
			%T item style map %I size read.w {text.unread text.read}
		}
	}

	$T notify bind $T <Collapse-after> {
		if {$Message(read,%I) && [AnyUnreadDescendants %T %I]} {
			# read -> read2
			%T item style map %I subject read2 {text.read text.unread}
			# read -> unread
			%T item style map %I from unread.we {text.read text.unread}
			%T item style map %I sent unread.we {text.read text.unread}
			%T item style map %I size unread.w {text.read text.unread}
		}
	}

	for {set i 1} {$i < $msgCnt} {incr i} {
		if {rand() * 2 > 1} {
			if {[$T item numchildren $i]} {
				$T collapse $i
			}
		}
	}

	return
}
proc AnyUnreadDescendants {T I} {

	global Message

	set itemList [$T item firstchild $I]
	while {[llength $itemList]} {
		# Pop
		set item [lindex $itemList end]
		set itemList [lrange $itemList 0 end-1]

		if {!$Message(read,$item)} {
			return 1
		}

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

	return 0
}
