# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/keyboard.tcl,v 3.12 1999/04/05 07:12:59 dawes Exp $
#
# Copyright 1996 by Joseph V. Moss <joe@XFree86.Org>
#
# See the file "LICENSE" for information regarding redistribution terms,
# and for a DISCLAIMER OF ALL WARRANTIES.
#
# $XConsortium: keyboard.tcl /main/2 1996/10/25 10:21:16 kaleb $

#
# Keyboard configuration routines
#

proc Keyboard_create_widgets { win } {
	global XKBComponents XKBavailable XKBhandle
	global pc98 messages

	set w [winpathprefix $win]
        if !$pc98 {
	    frame $w.keyboard -width 640 -height 420 \
		    -relief ridge -borderwidth 5
	} else {
	    frame $w.keyboard -width 640 -height 400 \
		    -relief ridge -borderwidth 5
	}

	frame $w.keyboard.xkb
	label $w.keyboard.xkb.text -text $messages(keyboard.4)
	frame $w.keyboard.xkb.geom
	label $w.keyboard.xkb.geom.title -text $messages(keyboard.1)
	combobox $w.keyboard.xkb.geom.cbox -bd 2 -width 30
	eval [list $w.keyboard.xkb.geom.cbox linsert end] \
		$XKBComponents(models,descriptions)
	pack  $w.keyboard.xkb.geom.title $w.keyboard.xkb.geom.cbox -side top
	frame $w.keyboard.xkb.lang
	label $w.keyboard.xkb.lang.title -text $messages(keyboard.2)
	combobox $w.keyboard.xkb.lang.cbox -bd 2 -width 30
	eval [list $w.keyboard.xkb.lang.cbox linsert end] \
		$XKBComponents(layouts,descriptions)
	pack  $w.keyboard.xkb.lang.title $w.keyboard.xkb.lang.cbox -side top
	frame $w.keyboard.xkb.vari
	label $w.keyboard.xkb.vari.title -text $messages(keyboard.6)
	combobox $w.keyboard.xkb.vari.cbox -bd 2 -width 30
	$w.keyboard.xkb.vari.cbox linsert end "<None>"
	eval [list $w.keyboard.xkb.vari.cbox linsert end] \
		$XKBComponents(variants,descriptions)
	pack  $w.keyboard.xkb.vari.title $w.keyboard.xkb.vari.cbox -side top
	pack $w.keyboard.xkb.text -side top -expand yes -fill both -padx 6m
	pack $w.keyboard.xkb.geom -side top -expand yes -fill both -padx 6m
	pack $w.keyboard.xkb.lang -side top -expand yes -fill both -padx 6m
	pack $w.keyboard.xkb.vari -side top -expand yes -fill both -padx 6m
	if { $XKBavailable } {
	    bind $w.keyboard.xkb.geom.cbox.popup.list <ButtonRelease-1> \
		"+Keyboard_loadsettings $win noload"
	    bind $w.keyboard.xkb.geom.cbox.popup <Return> \
		"+Keyboard_loadsettings $win noload"
	    bind $w.keyboard.xkb.lang.cbox.popup.list <ButtonRelease-1> \
		"+Keyboard_loadsettings $win noload"
	    bind $w.keyboard.xkb.lang.cbox.popup <Return> \
		"+Keyboard_loadsettings $win noload"
	    xkbview $w.keyboard.xkb.graphic -height 100 -kbd $XKBhandle \
		-dbl 1 -bd 6 -relief ridge
	} else {
	    frame $w.keyboard.xkb.graphic
	}
	pack $w.keyboard.xkb.graphic  -side top -expand yes -fill x
	if { $XKBavailable } {
	    button $w.keyboard.xkb.apply -text $messages(keyboard.3) \
		-command "Keyboard_loadsettings $win load"
	    pack $w.keyboard.xkb.apply -side top -expand yes -fill both
	}
	label $w.keyboard.xkb.message -text "" -foreground black
	pack $w.keyboard.xkb.message  -side top -expand yes -fill x

	frame $w.keyboard.options -relief groove -bd 4
	Keyboard_create_options_widgets $win
	pack $w.keyboard.xkb -side left -fill both -expand yes -pady 10m
	pack $w.keyboard.options -side left -fill both -expand no
	Keyboard_initsettings $win
}

proc Keyboard_create_options_widgets { win } {
	global XKBComponents keyboardXkbOpts messages

	set w [winpathprefix $win]
	set numopts [llength $XKBComponents(options,names)]
	if { $numopts == 0 } {
		set keyboardXkbOpts(noopts) -1
		return
	}

	label $w.keyboard.options.title -text $messages(keyboard.5)
	pack  $w.keyboard.options.title -fill x \
		-expand no -pady 3m -side top
	frame $w.keyboard.options.line -relief sunken -height 2 -bd 3
	pack  $w.keyboard.options.line -fill x \
		-expand no -pady 3m -side top

	set canv $w.keyboard.options.canvas
	canvas $canv
	frame $canv.list
	set id [$canv create window 0 0 -window $canv.list -anchor nw]
	for {set idx 0} { $idx < $numopts } {incr idx} {
		set name [lindex $XKBComponents(options,names) $idx]
		set desc [lindex $XKBComponents(options,descriptions) $idx]
		set tmp [split $name :]
		set value $idx
		if { [llength $tmp] != 2 } {
		    set next ""
		    if { [expr $idx+1] < $numopts } {
			set next [lindex $XKBComponents(options,names) \
					[expr $idx+1] ]
		    }
		    if { [string match $name:* $next] } {
			label $canv.list.$name -text $desc \
				-relief ridge -bd 3
			pack $canv.list.$name -fill both -expand no
			set tmp [list $name default]
			set value -1
			set desc $messages(keyboard.7)
			set keyboardXkbOpts($name) -1
		    } else {
			checkbutton $canv.list.$name \
				-variable keyboardXkbOpts($name) \
				-text $desc -highlightthickness 1 \
				-offvalue -1 -onvalue $idx -anchor w \
				-relief sunken -bd 1
			pack $canv.list.$name -fill both -expand no
			set keyboardXkbOpts($name) -1
		        continue
		    }
		}
		set group  [lindex $tmp 0]
		set option [lindex $tmp 1]
		radiobutton $canv.list.$group-$option \
			-variable keyboardXkbOpts($group) -value $value \
			-text $desc -highlightthickness 1 -anchor w
		pack $canv.list.$group-$option -fill both -expand no -padx 3
	}
	update idletasks
	pack $canv -fill y -expand no -side left
	scrollbar $w.keyboard.options.sb -command "$canv yview" -relief sunken
	set bbox [$canv bbox $id]
	$canv configure -yscrollcommand "$w.keyboard.options.sb set" \
		-scrollregion $bbox -width [lindex $bbox 2]
	if { [winfo reqheight $canv.list] > [winfo height $w]*.7 } {
		pack $w.keyboard.options.sb -side left -fill y -expand yes
	}
}

proc Keyboard_initsettings { win } {
	global Keyboard XKBComponents keyboardXkbOpts

	set w [winpathprefix $win]
	set model	$Keyboard(XkbModel)
	set layout	$Keyboard(XkbLayout)
	set variant	$Keyboard(XkbVariant)
	set options	$Keyboard(XkbOptions)

	set tmp [xkb_getrulesprop]
	if { [llength $tmp] == 5 } {
		set model	[lindex $tmp 1]
		set layout	[lindex $tmp 2]
		set variant	[lindex $tmp 3]
		set options	[lindex $tmp 4]
	}
	set geomidx [lsearch -exact $XKBComponents(models,names) $model]
	if { $geomidx == -1 } {set geomidx 0}
	set langidx [lsearch -exact $XKBComponents(layouts,names) $layout]
	if { $langidx == -1 } {set langidx 0}
	set variidx [lsearch -exact $XKBComponents(variants,names) $variant]
	incr variidx

	$w.keyboard.xkb.geom.cbox einsert 0 \
		[lindex $XKBComponents(models,descriptions) $geomidx]
	$w.keyboard.xkb.lang.cbox einsert 0 \
		[lindex $XKBComponents(layouts,descriptions) $langidx]
	if { $variidx } {
	    $w.keyboard.xkb.vari.cbox einsert 0 \
		[lindex $XKBComponents(variants,descriptions) $variidx]
	}
	$w.keyboard.xkb.geom.cbox econfig -state disabled
	$w.keyboard.xkb.lang.cbox econfig -state disabled
	$w.keyboard.xkb.vari.cbox econfig -state disabled
	$w.keyboard.xkb.geom.cbox lselection set $geomidx
	$w.keyboard.xkb.lang.cbox lselection set $langidx
	$w.keyboard.xkb.vari.cbox lselection set $variidx
	$w.keyboard.xkb.geom.cbox activate $geomidx
	$w.keyboard.xkb.lang.cbox activate $langidx
	$w.keyboard.xkb.vari.cbox activate $variidx
	$w.keyboard.xkb.geom.cbox see $geomidx
	$w.keyboard.xkb.lang.cbox see $langidx
	$w.keyboard.xkb.vari.cbox see $variidx

	set optlist [split $options ,]
	set namelist $XKBComponents(options,names)
	set cl $w.keyboard.options.canvas.list
	foreach opt $optlist {
		set tmp [split $opt :]
		set idx [lsearch -exact $namelist $opt]
		if { [llength $tmp] != 2 } {
			if { [winfo exists $cl.$opt-default] } {
				set keyboardXkbOpts($opt) -1
			} else {
				set keyboardXkbOpts($opt) $idx
			}
		} else {
			set keyboardXkbOpts([lindex $tmp 0]) $idx
		}
	}
}

proc Keyboard_activate { win } {
	set w [winpathprefix $win]
	pack $w.keyboard -side top -fill both -expand yes
}

proc Keyboard_deactivate { win } {
	set w [winpathprefix $win]
	pack forget $w.keyboard
	Keyboard_loadsettings $win setvars
}

proc Keyboard_loadsettings { win loadflag } {
	global XKBComponents XKBrules Keyboard XKBhandle keyboardXkbOpts
	global messages

	set w [winpathprefix $win]
	if { $loadflag == "load" } {
		$w.keyboard.xkb.message configure \
			-text $messages(keyboard.9)
	}
	if { $loadflag == "noload" } {
		$w.keyboard.xkb.message configure \
			-text $messages(keyboard.10)
	}
	update
	set geom_idx [$w.keyboard.xkb.geom.cbox curselection]
	set lang_idx [$w.keyboard.xkb.lang.cbox curselection]
	set vari_idx [$w.keyboard.xkb.vari.cbox curselection]
	incr vari_idx -1
	set geom [lindex $XKBComponents(models,names)  $geom_idx]
	set lang [lindex $XKBComponents(layouts,names) $lang_idx]
	if { $vari_idx == -1 } {
		set vari ""
	} else {
		set vari [lindex $XKBComponents(variants,names) $vari_idx]
	}
	set opts ""
	foreach key [array names keyboardXkbOpts] {
		if { $keyboardXkbOpts($key) == -1 } continue
		set opt [lindex $XKBComponents(options,names) \
					$keyboardXkbOpts($key)]
		if { ![string length $opts] } {
			set opts $opt
		} else {
			append opts ",$opt"
		}
	}

	if { $loadflag != "setvars" } {
		set comp [xkb_resolvecomponents \
			$XKBrules $geom $lang $vari $opts]
		set notloaded [catch {eval xkb_load $comp $loadflag} kbd]
		if { $notloaded } {
			$w.keyboard.xkb.message configure \
				-text $messages(keyboard.8)
			bell
			after 1000
		} else {
			xkb_free $XKBhandle
			set XKBhandle $kbd
			$w.keyboard.xkb.graphic configure -kbd $kbd
			if { $loadflag != "noload" } {
				xkb_setrulesprop $XKBrules \
					$geom $lang $vari $opts
				set Keyboard(XkbModel)		$geom
				set Keyboard(XkbLayout)		$lang
				set Keyboard(XkbVariant)	$vari
				set Keyboard(XkbOptions)	$opts
			}
		}
	} else {
		set Keyboard(XkbModel)		$geom
		set Keyboard(XkbLayout)		$lang
		set Keyboard(XkbVariant)	$vari
		set Keyboard(XkbOptions)	$opts
	}
	focus $w
	$w.keyboard.xkb.message configure -text ""
}

