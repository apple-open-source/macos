#
# Demo: Help contents
#
proc DemoHelpContents {} {

	set T .f2.f1.t

	set height [font metrics [$T cget -font] -linespace]
	if {$height < 18} {
		set height 18
	}
	$T configure -showroot no -showbuttons no -showlines no -itemheight $height \
		-selectmode browse

	InitPics help-*

	$T column configure 0 -text "Help Contents"

	# Define a new item state
	$T state define mouseover

	$T element create e1 image -image help-page
	$T element create e2 image -image {help-book-open {open} help-book-closed {}}
	$T element create e3 text -fill [list $::SystemHighlightText {selected focus} blue {mouseover}] \
		-font [list "[$T cget -font] underline" {mouseover}]
	$T element create e4 rect -fill [list $::SystemHighlight {selected focus}] -showfocus yes

	# book
	set S [$T style create s1]
	$T style elements $S {e4 e1 e3}
	$T style layout $S e1 -padx {0 4} -expand ns
	$T style layout $S e3 -expand ns
	$T style layout $S e4 -union [list e3] -iexpand ns -ipadx 2

	# page
	set S [$T style create s2]
	$T style elements $S {e4 e2 e3}
	$T style layout $S e2 -padx {0 4} -expand ns
	$T style layout $S e3 -expand ns
	$T style layout $S e4 -union [list e3] -iexpand ns -ipadx 2

	set parentList [list root {} {} {} {} {} {}]
	set parent root
	foreach {depth style text} {
		0 s1 "Welcome to Help"
		0 s2 "Introducing Windows 98"
			1 s2 "How to Use Help"
				2 s1 "Find a topic"
				2 s1 "Get more out of help"
			1 s2 "Register Your Software"
				2 s1 "Registering Windows 98 online"
			1 s2 "What's New in Windows 98"
				2 s1 "Innovative, easy-to-use features"
				2 s1 "Improved reliability"
				2 s1 "A faster operating system"
				2 s1 "True Web integration"
				2 s1 "More entertaining and fun"
			1 s2 "If You're New to Windows 98"
				2 s2 "Tips for Macintosh Users"
					3 s1 "Why does the mouse have two buttons?"
	} {
		set item [$T item create]
		$T item style set $item 0 $style
		$T item element configure $item 0 e3 -text $text
		$T collapse $item
		$T item lastchild [lindex $parentList $depth] $item
		incr depth
		set parentList [lreplace $parentList $depth $depth $item]
	}

	bind TreeCtrlHelp <Double-ButtonPress-1> {
		if {[lindex [%W identify %x %y] 0] eq "header"} {
			TreeCtrl::DoubleButton1 %W %x %y
		} else {
			TreeCtrl::HelpButton1 %W %x %y
		}
		break
	}
	bind TreeCtrlHelp <ButtonPress-1> {
		TreeCtrl::HelpButton1 %W %x %y
		break
	}
	bind TreeCtrlHelp <Button1-Motion> {
		TreeCtrl::HelpMotion1 %W %x %y
		break
	}
	bind TreeCtrlHelp <Button1-Leave> {
		TreeCtrl::HelpLeave1 %W %x %y
		break
	}
	bind TreeCtrlHelp <ButtonRelease-1> {
		TreeCtrl::HelpRelease1 %W %x %y
		break
	}
	bind TreeCtrlHelp <Motion> {
		TreeCtrl::HelpMotion %W %x %y
	}
	bind TreeCtrlHelp <Leave> {
		TreeCtrl::HelpMotion %W %x %y
	}
	bind TreeCtrlHelp <KeyPress-Return> {
		if {[llength [%W selection get]] == 1} {
			%W toggle [%W selection get]
		}
		break
	}

	set ::TreeCtrl::Priv(help,prev) ""
	bindtags $T [list $T TreeCtrlHelp TreeCtrl [winfo toplevel $T] all]

	return
}

# This is an alternate implementation that does not define a new item state
# to change the appearance of the item under the cursor.
proc DemoHelpContents2 {} {

	set T .f2.f1.t

	set height [font metrics [$T cget -font] -linespace]
	if {$height < 18} {
		set height 18
	}
	$T configure -showroot no -showbuttons no -showlines no -itemheight $height \
		-selectmode browse

	InitPics help-*

	$T column configure 0 -text "Help Contents"

	$T element create e1 image -image help-page
	$T element create e2 image -image {help-book-open {open} help-book-closed {}}
	$T element create e3 text -fill [list $::SystemHighlightText {selected focus}]
	$T element create e4 rect -fill [list $::SystemHighlight {selected focus}] -showfocus yes
	$T element create e5 text -fill [list $::SystemHighlightText {selected focus} blue {}] \
		-font "[$T cget -font] underline"

	# book
	set S [$T style create s1]
	$T style elements $S {e4 e1 e3}
	$T style layout $S e1 -padx {0 4} -expand ns
	$T style layout $S e3 -expand ns
	$T style layout $S e4 -union [list e3] -iexpand ns -ipadx 2

	# page
	set S [$T style create s2]
	$T style elements $S {e4 e2 e3}
	$T style layout $S e2 -padx {0 4} -expand ns
	$T style layout $S e3 -expand ns
	$T style layout $S e4 -union [list e3] -iexpand ns -ipadx 2

	# book (focus)
	set S [$T style create s1.f]
	$T style elements $S {e4 e1 e5}
	$T style layout $S e1 -padx {0 4} -expand ns
	$T style layout $S e5 -expand ns
	$T style layout $S e4 -union [list e5] -iexpand ns -ipadx {1 2}

	# page (focus)
	set S [$T style create s2.f]
	$T style elements $S {e4 e2 e5}
	$T style layout $S e2 -padx {0 4} -expand ns
	$T style layout $S e5 -expand ns
	$T style layout $S e4 -union [list e5] -iexpand ns -ipadx {1 2}

	set parentList [list root {} {} {} {} {} {}]
	set parent root
	foreach {depth style text} {
		0 s1 "Welcome to Help"
		0 s2 "Introducing Windows 98"
			1 s2 "How to Use Help"
				2 s1 "Find a topic"
				2 s1 "Get more out of help"
			1 s2 "Register Your Software"
				2 s1 "Registering Windows 98 online"
			1 s2 "What's New in Windows 98"
				2 s1 "Innovative, easy-to-use features"
				2 s1 "Improved reliability"
				2 s1 "A faster operating system"
				2 s1 "True Web integration"
				2 s1 "More entertaining and fun"
			1 s2 "If You're New to Windows 98"
				2 s2 "Tips for Macintosh Users"
					3 s1 "Why does the mouse have two buttons?"
	} {
		set item [$T item create]
		$T item style set $item 0 $style
		$T item element configure $item 0 e3 -text $text
		$T collapse $item
		$T item lastchild [lindex $parentList $depth] $item
		incr depth
		set parentList [lreplace $parentList $depth $depth $item]
	}

	bind TreeCtrlHelp <Double-ButtonPress-1> {
		if {[lindex [%W identify %x %y] 0] eq "header"} {
			TreeCtrl::DoubleButton1 %W %x %y
		} else {
			TreeCtrl::HelpButton1 %W %x %y
		}
		break
	}
	bind TreeCtrlHelp <ButtonPress-1> {
		TreeCtrl::HelpButton1 %W %x %y
		break
	}
	bind TreeCtrlHelp <Button1-Motion> {
		TreeCtrl::HelpMotion1 %W %x %y
		break
	}
	bind TreeCtrlHelp <Button1-Leave> {
		TreeCtrl::HelpLeave1 %W %x %y
		break
	}
	bind TreeCtrlHelp <ButtonRelease-1> {
		TreeCtrl::HelpRelease1 %W %x %y
		break
	}
	bind TreeCtrlHelp <Motion> {
		TreeCtrl::HelpMotion2 %W %x %y
	}
	bind TreeCtrlHelp <Leave> {
		TreeCtrl::HelpMotion2 %W %x %y
	}
	bind TreeCtrlHelp <KeyPress-Return> {
		if {[llength [%W selection get]] == 1} {
			%W toggle [%W selection get]
		}
		break
	}

	set ::TreeCtrl::Priv(help,prev) ""
	bindtags $T [list $T TreeCtrlHelp TreeCtrl [winfo toplevel $T] all]

	return
}

proc TreeCtrl::HelpButton1 {w x y} {
	variable Priv
	focus $w
	set id [$w identify $x $y]
	set Priv(buttonMode) ""
	if {[lindex $id 0] eq "header"} {
		ButtonPress1 $w $x $y
	} elseif {[lindex $id 0] eq "item"} {
		set item [lindex $id 1]
		# didn't click an element
		if {[llength $id] != 6} return
		if {[$w selection includes $item]} {
			$w toggle $item
			return
		}
		if {[llength [$w selection get]]} {
			set item2 [$w selection get]
			$w collapse $item2
			foreach item2 [$w item ancestors $item2] {
				if {[$w compare $item != $item2]} {
					$w collapse $item2
				}
			}
		}
		$w selection modify $item all
		$w activate $item
		eval $w expand [$w item ancestors $item]
		$w toggle $item
	}
	return
}
proc TreeCtrl::HelpMotion1 {w x y} {
	variable Priv
	switch $Priv(buttonMode) {
		"resize" -
		"header" {
			Motion1 $w $x $y
		}
	}
	return
}
proc TreeCtrl::HelpLeave1 {w x y} {
	variable Priv
	# This is called when I do ButtonPress-1 on Unix for some reason,
	# and buttonMode is undefined.
	if {![info exists Priv(buttonMode)]} return
	switch $Priv(buttonMode) {
		"header" {
			$w column configure $Priv(column) -sunken no
		}
	}
	return
}
proc TreeCtrl::HelpRelease1 {w x y} {
	variable Priv
	switch $Priv(buttonMode) {
		"resize" -
		"header" {
			Release1 $w $x $y
		}
	}
	set Priv(buttonMode) ""
	return
}

proc TreeCtrl::HelpMotion {w x y} {
	variable Priv
	set id [$w identify $x $y]
	if {$id eq ""} {
	} elseif {[lindex $id 0] eq "header"} {
	} elseif {[lindex $id 0] eq "item"} {
		set item [lindex $id 1]
		if {[llength $id] == 6} {
			if {$item ne $Priv(help,prev)} {
				if {$Priv(help,prev) ne ""} {
					$w item state set $Priv(help,prev) !mouseover
				}
				$w item state set $item mouseover
				set Priv(help,prev) $item
			}
			return
		}
	}
	if {$Priv(help,prev) ne ""} {
		$w item state set $Priv(help,prev) !mouseover
		set Priv(help,prev) ""
	}
	return
}

# Alternate implementation doesn't rely on mouseover state
proc TreeCtrl::HelpMotion2 {w x y} {
	variable Priv
	set id [$w identify $x $y]
	if {[lindex $id 0] eq "header"} {
	} elseif {$id ne ""} {
		set item [lindex $id 1]
		if {[llength $id] == 6} {
			if {$item ne $Priv(help,prev)} {
				if {$Priv(help,prev) ne ""} {
					set style [$w item style set $Priv(help,prev) 0]
					set style [string trim $style .f]
					$w item style map $Priv(help,prev) 0 $style {e5 e3}
				}
				set style [$w item style set $item 0]
				$w item style map $item 0 $style.f {e3 e5}
				set Priv(help,prev) $item
			}
			return
		}
	}
	if {$Priv(help,prev) ne ""} {
		set style [$w item style set $Priv(help,prev) 0]
		set style [string trim $style .f]
		$w item style map $Priv(help,prev) 0 $style {e5 e3}
		set Priv(help,prev) ""
	}
	return
}

