proc DemoInternetOptions {} {

	set T .f2.f1.t

	set height [font metrics [$T cget -font] -linespace]
	if {$height < 18} {
		set height 18
	}
	$T configure -showroot no -showbuttons no -showlines no -itemheight $height \
		-selectmode browse

	InitPics internet-*

	$T column configure 0 -text "Internet Options"

	$T state define check
	$T state define radio
	$T state define on

	$T element create e1 image -image {
		internet-check-on {check on}
		internet-check-off {check}
		internet-radio-on {radio on}
		internet-radio-off {radio}
	}
	$T element create e2 text -fill [list $::SystemHighlightText {selected focus}]
	$T element create e3 rect -fill [list $::SystemHighlight {selected focus}] -showfocus yes

	set S [$T style create s1]
	$T style elements $S {e3 e1 e2}
	$T style layout $S e1 -padx {0 4} -expand ns
	$T style layout $S e2 -expand ns
	$T style layout $S e3 -union [list e2] -iexpand ns -ipadx 2

	set parentList [list root {} {} {} {} {} {}]
	set parent root
	foreach {depth setting text option group} {
		0 print "Printing" "" ""
			1 off "Print background colors and images" "o1" ""
		0 search "Search from Address bar" "" ""
			1 search "When searching" "" ""
				2 off "Display results, and go to the most likely sites" "o2" "r1"
				2 off "Do not search from the Address bar" "o3" "r1"
				2 off "Just display the results in the main window" "o4" "r1"
				2 on "Just go to the most likely site" "o5" "r1"
		0 security "Security" "" ""
			1 on "Check for publisher's certificate revocation" "o5" ""
			1 off "Check for server certificate revocation (requires restart)" "o6" ""
	} {
		set item [$T item create]
		$T item style set $item 0 s1
		$T item element configure $item 0 e2 -text $text
		set ::Option(option,$item) $option
		set ::Option(group,$item) $group
		if {($setting eq "on") || ($setting eq "off")} {
			set ::Option(setting,$item) $setting
			if {$group eq ""} {
				$T item state set $item check
				if {$setting eq "on"} {
					$T item state set $item on
				}
			} else {
				if {$setting eq "on"} {
					set ::Option(current,$group) $item
					$T item state set $item on
				}
				$T item state set $item radio
			}
		} else {
			$T item element configure $item 0 e1 -image internet-$setting
		}
		$T item lastchild [lindex $parentList $depth] $item
		incr depth
		set parentList [lreplace $parentList $depth $depth $item]
	}

	bind TreeCtrlOption <Double-ButtonPress-1> {
		TreeCtrl::DoubleButton1 %W %x %y
	}
	bind TreeCtrlOption <ButtonPress-1> {
		TreeCtrl::OptionButton1 %W %x %y
		break
	}
	bind TreeCtrlOption <Button1-Motion> {
		TreeCtrl::OptionMotion1 %W %x %y
		break
	}
	bind TreeCtrlOption <Button1-Leave> {
		TreeCtrl::OptionLeave1 %W %x %y
		break
	}
	bind TreeCtrlOption <ButtonRelease-1> {
		TreeCtrl::OptionRelease1 %W %x %y
		break
	}

	bindtags $T [list $T TreeCtrlOption TreeCtrl [winfo toplevel $T] all]

	return
}
proc TreeCtrl::OptionButton1 {T x y} {
	variable Priv
	focus $T
	set id [$T identify $x $y]
	if {[lindex $id 0] eq "header"} {
		ButtonPress1 $T $x $y
	} elseif {$id eq ""} {
		set Priv(buttonMode) ""
	} else {
		set Priv(buttonMode) ""
		set item [lindex $id 1]
		$T selection modify $item all
		$T activate $item
		if {$::Option(option,$item) eq ""} return
		set group $::Option(group,$item)
		# a checkbutton
		if {$group eq ""} {
			$T item state set $item ~on
			if {$::Option(setting,$item) eq "on"} {
				set setting off
			} else {
				set setting on
			}
			set ::Option(setting,$item) $setting
		# a radiobutton
		} else {
			set current $::Option(current,$group)
			if {$current eq $item} return
			$T item state set $current !on
			$T item state set $item on
			set ::Option(setting,$item) on
			set ::Option(current,$group) $item
		}
	}
	return
}


# Alternate implementation that doesn't rely on run-time styles
proc DemoInternetOptions_2 {} {

	set T .f2.f1.t

	set height [font metrics [$T cget -font] -linespace]
	if {$height < 18} {
		set height 18
	}
	$T configure -showroot no -showbuttons no -showlines no -itemheight $height \
		-selectmode browse

	InitPics internet-*

	$T column configure 0 -text "Internet Options"

	$T element create e1 image
	$T element create e2 text -fill [list $::SystemHighlightText {selected focus}]
	$T element create e3 rect -fill [list $::SystemHighlight {selected focus}] -showfocus yes

	set S [$T style create s1]
	$T style elements $S {e3 e1 e2}
	$T style layout $S e1 -padx {0 4} -expand ns
	$T style layout $S e2 -expand ns
	$T style layout $S e3 -union [list e2] -iexpand ns -ipadx 2

	set parentList [list root {} {} {} {} {} {}]
	set parent root
	foreach {depth setting text option group} {
		0 print "Printing" "" ""
			1 off "Print background colors and images" "o1" ""
		0 search "Search from Address bar" "" ""
			1 search "When searching" "" ""
				2 off "Display results, and go to the most likely sites" "o2" "r1"
				2 off "Do not search from the Address bar" "o3" "r1"
				2 off "Just display the results in the main window" "o4" "r1"
				2 on "Just go to the most likely site" "o5" "r1"
		0 security "Security" "" ""
			1 on "Check for publisher's certificate revocation" "o5" ""
			1 off "Check for server certificate revocation (requires restart)" "o6" ""
	} {
		set item [$T item create]
		$T item style set $item 0 s1
		$T item element configure $item 0 e2 -text $text
		set ::Option(option,$item) $option
		set ::Option(group,$item) $group
		if {$setting eq "on" || $setting eq "off"} {
			set ::Option(setting,$item) $setting
			if {$group eq ""} {
				set img internet-check-$setting
				$T item element configure $item 0 e1 -image $img
			} else {
				if {$setting eq "on"} {
					set ::Option(current,$group) $item
				}
				set img internet-radio-$setting
				$T item element configure $item 0 e1 -image $img
			}
		} else {
			$T item element configure $item 0 e1 -image internet-$setting
		}
		$T item lastchild [lindex $parentList $depth] $item
		incr depth
		set parentList [lreplace $parentList $depth $depth $item]
	}

	bind TreeCtrlOption <Double-ButtonPress-1> {
		TreeCtrl::DoubleButton1 %W %x %y
	}
	bind TreeCtrlOption <ButtonPress-1> {
		TreeCtrl::OptionButton1 %W %x %y
		break
	}
	bind TreeCtrlOption <Button1-Motion> {
		TreeCtrl::OptionMotion1 %W %x %y
		break
	}
	bind TreeCtrlOption <Button1-Leave> {
		TreeCtrl::OptionLeave1 %W %x %y
		break
	}
	bind TreeCtrlOption <ButtonRelease-1> {
		TreeCtrl::OptionRelease1 %W %x %y
		break
	}

	bindtags $T [list $T TreeCtrlOption TreeCtrl [winfo toplevel $T] all]

	return
}
proc TreeCtrl::OptionButton1_2 {T x y} {
	variable Priv
	focus $T
	set id [$T identify $x $y]
	if {[lindex $id 0] eq "header"} {
		ButtonPress1 $T $x $y
	} elseif {$id eq ""} {
		set Priv(buttonMode) ""
	} else {
		set Priv(buttonMode) ""
		set item [lindex $id 1]
		$T selection modify $item all
		$T activate $item
		if {$::Option(option,$item) eq ""} return
		set group $::Option(group,$item)
		# a checkbutton
		if {$group eq ""} {
			if {$::Option(setting,$item) eq "on"} {
				set setting off
			} else {
				set setting on
			}
			$T item element configure $item 0 e1 -image internet-check-$setting
			set ::Option(setting,$item) $setting
		# a radiobutton
		} else {
			set current $::Option(current,$group)
			if {$current eq $item} return
			$T item element configure $current 0 e1 -image internet-radio-off
			$T item element configure $item 0 e1 -image internet-radio-on
			set ::Option(setting,$item) on
			set ::Option(current,$group) $item
		}
	}
	return
}
proc TreeCtrl::OptionMotion1 {T x y} {
	variable Priv
	switch $Priv(buttonMode) {
		"resize" -
		"header" {
			Motion1 $T $x $y
		}
	}
	return
}
proc TreeCtrl::OptionLeave1 {T x y} {
	variable Priv
	# This is called when I do ButtonPress-1 on Unix for some reason,
	# and buttonMode is undefined.
	if {![info exists Priv(buttonMode)]} return
	switch $Priv(buttonMode) {
		"header" {
			$T column configure $Priv(column) -sunken no
		}
	}
	return
}
proc TreeCtrl::OptionRelease1 {T x y} {
	variable Priv
	switch $Priv(buttonMode) {
		"resize" -
		"header" {
			Release1 $T $x $y
		}
	}
	set Priv(buttonMode) ""
	return
}

