#
# Demo: Outlook Express folder list
#
proc DemoOutlookFolders {} {

	InitPics outlook-*

	set T .f2.f1.t

	set height [font metrics [$T cget -font] -linespace]
	if {$height < 18} {
		set height 18
	}
	$T configure -itemheight $height -selectmode browse \
		-showroot yes -showrootbutton no -showbuttons yes -showlines yes

	$T column configure 0 -text Folders

	$T element create e1 image
	$T element create e2 text -fill [list $::SystemHighlightText {selected focus}] \
		-lines 1
	$T element create e3 text -fill [list $::SystemHighlightText {selected focus}] \
		-font [list "[$T cget -font] bold"] -lines 1
	$T element create e4 text -fill blue
	$T element create e5 image -image outlook-folder
	$T element create e6 rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] \
		-showfocus yes

	# image + text
	set S [$T style create s1]
	$T style elements $S {e6 e1 e2}
	$T style layout $S e1 -expand ns
	$T style layout $S e2 -padx {4 0} -expand ns -squeeze x
	$T style layout $S e6 -union [list e2] -iexpand ns -ipadx 2

	# image + text + text
	set S [$T style create s2]
	$T style elements $S {e6 e1 e3 e4}
	$T style layout $S e1 -expand ns
	$T style layout $S e3 -padx 4 -expand ns -squeeze x
	$T style layout $S e4 -expand ns
	$T style layout $S e6 -union [list e3] -iexpand ns -ipadx 2

	# folder + text
	set S [$T style create s3]
	$T style elements $S {e6 e5 e2}
	$T style layout $S e5 -expand ns
	$T style layout $S e2 -padx {4 0} -expand ns -squeeze x
	$T style layout $S e6 -union [list e2] -iexpand ns -ipadx 2

	# folder + text + text
	set S [$T style create s4]
	$T style elements $S {e6 e5 e3 e4}
	$T style layout $S e5 -expand ns
	$T style layout $S e3 -padx 4 -expand ns -squeeze x
	$T style layout $S e4 -expand ns
	$T style layout $S e6 -union [list e3] -iexpand ns -ipadx 2

	$T item style set root 0 s1
	$T item complex root [list [list e1 -image outlook-main] [list e2 -text "Outlook Express"]]

	set parentList [list root {} {} {} {} {} {}]
	set parent root
	foreach {depth img text button unread} {
		0 local "Local Folders" yes 0
			1 inbox Inbox no 5
			1 outbox Outbox no 0
			1 sent "Sent Items" no 0
			1 deleted "Deleted Items" no 50
			1 draft Drafts no 0
			1 folder "Messages to Dad" no 0
			1 folder "Messages to Sis" no 0
			1 folder "Messages to Me" yes 0
				2 folder "2001" no 0
				2 folder "2000" no 0
				2 folder "1999" no 0
		0 server "news.gmane.org" yes 0
			1 group "gmane.comp.lang.lua.general" no 498
	} {
		set item [$T item create]
		$T item hasbutton $item $button
		if {[string equal $img folder]} {
			if {$unread} {
				$T item style set $item 0 s4
				$T item complex $item [list [list e3 -text $text] [list e4 -text "($unread)"]]
			} else {
				$T item style set $item 0 s3
				$T item complex $item [list [list e2 -text $text]]
			}
		} else {
			if {$unread} {
				$T item style set $item 0 s2
				$T item complex $item [list [list e1 -image outlook-$img] [list e3 -text $text] [list e4 -text "($unread)"]]
			} else {
				$T item style set $item 0 s1
				$T item complex $item [list [list e1 -image outlook-$img] [list e2 -text $text]]
			}
		}
		$T item lastchild [lindex $parentList $depth] $item
		incr depth
		set parentList [lreplace $parentList $depth $depth $item]
	}

	return
}

