#
# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/texts/local_text.tcl,v 1.2 1998/04/05 15:25:34 robin Exp $
#
proc Make_popup_help { file } {
	global pc98 pc98_EGC messages

	set win .help_$file
	# get 'message' variable.
	source [Find_local_text help_$file.tcl]

	catch {destroy $win}
        toplevel $win -bd 5 -relief ridge
        wm title $win "Help"
	if !$pc98_EGC {
		wm geometry $win 590x430+30+20
	} else {
		$win configure -height 350
		wm geometry $win 590x350+30+20
	}
	text $win.text -takefocus 0 -width 90 -height 30 \
		-yscrollcommand [list $win.sb set]
	scrollbar $win.sb -command [list $win.text yview]
	bind $win <Prior> \
		"$win.text yview scroll -1 unit ; break ;"
	bind $win <Next> \
		"$win.text yview scroll  1 unit ; break ;"
        $win.text insert 0.0 $message
	$win.text configure -state disabled
        button $win.ok -text $messages(phase2.13) \
		-command "destroy $win"
	pack $win.ok -side bottom
	pack $win.text -side left -fill both -expand yes
        pack $win.text $win.ok
	focus $win.ok
}

proc append_helppath { newlang } {
	upvar rootdir r
	upvar lang l
	global helppath

	set l $newlang
	if [file isdirectory $r/$l] {
		lappend helppath $r/$l
	}
}

proc Make_local_directory {} {
	global locale helppath XF86Setup_library
	set rootdir $XF86Setup_library/texts
	if {[info exists locale]} {
		append_helppath $locale
		while {[set pt [string last . $lang]] != -1} {
			append_helppath [string range $lang 0 [expr $pt-1]]
		}
		if {[set pt [string first _ $lang]] != -1} {
			append_helppath [string range $lang 0 [expr $pt-1]]
		}
	}
	append_helppath generic
}

proc Read_locale {} {
	global env Xwinhome locale
	set locale_dir $Xwinhome/lib/X11/locale
	set locale {}
	if {[info exists env(LANG)]} {
		if {[file readable $locale_dir/locale.alias]} {
			set cmd {[ \t]+/ {print $2;}}
			set cmd "/^$env(LANG)$cmd";
			set locale [exec awk $cmd $locale_dir/locale.alias]
		}
		if {$locale == {} } {
			set locale $env(LANG)
		}
	}
}

proc Find_local_text {filename} {
	global helppath
	foreach path $helppath {
		if [file exists $path/$filename] {
			if {[file readable $path/$filename] && \
				[file isfile $path/$filename]} {
				return $path/$filename
			}
		}
	}
	return /dev/null
}

proc Card_popup_help { win } {
	global cardDetail
	Make_popup_help card
	if {$cardDetail != "std"} {
		pack .help_card.sb -side right -fill y
	}
}

proc Done_popup_help { win } {
	Make_popup_help done
}

proc Keyboard_popup_help { win } {
	Make_popup_help keyboard
}

proc Monitor_popup_help { win } {
	Make_popup_help monitor
}

proc Other_popup_help { win } {
	Make_popup_help other
}

proc Intro_popup_help { win } {
	Make_popup_help intro
}

proc Mouse_popup_help { win } {
	Make_popup_help mouse
	pack .help_mouse.sb -side right -fill y
}
		
proc Modeselection_popup_help { win } {
	Make_popup_help modeselect
}

Read_locale
Make_local_directory
source [Find_local_text messages.tcl]
source [Find_local_text message_proc.tcl]
