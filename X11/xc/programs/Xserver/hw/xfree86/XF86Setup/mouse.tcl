# $XConsortium: mouse.tcl /main/6 1996/10/28 05:42:22 kaleb $
#
#
#
#
# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/mouse.tcl,v 3.25 1998/04/26 16:04:34 robin Exp $
#
# Copyright 1996 by Joseph V. Moss <joe@XFree86.Org>
#
# See the file "LICENSE" for information regarding redistribution terms,
# and for a DISCLAIMER OF ALL WARRANTIES.
#

#
# Mouse configuration routines
#

set mseTypeList [concat $SupportedMouseTypes { Xqueue OSMouse } ]

set msePatterns [list {tty[0-9A-Za-o]*} cua* *bm *mse* *mouse* \
                      ps*x psm* m320 pms* com* gpmdata lms* kdmouse logi msm]
set mseDevices ""
foreach pat $msePatterns {
	if ![catch {glob /dev/$pat}] {
		eval lappend mseDevices [glob /dev/$pat]
	}
}
if [info exists Pointer(RealDev)] {
	lappend mseDevices $Pointer(RealDev)
}
set mseDevices [lrmdups $mseDevices]

proc Mouse_proto_select { win } {
	global mseType baudRate chordMiddle clearDTR clearRTS sampleRate
	global mseRes mseButtons mseDeviceSelected messages

	set w [winpathprefix $win]
	set canv $w.mouse.mid.right.canvas
	$canv itemconfigure mbut  -fill white
	$canv itemconfigure coord -fill black
	if {[lsearch -exact {BusMouse Xqueue OSMouse PS/2 IMPS/2
			     ThinkingMousePS/2 MouseManPlusPS/2 GlidePointPS/2 
			     NetMousePS/2 NetScrollPS/2 SysMouse} \
			     $mseType] == -1} {
		foreach rate {1200 2400 4800 9600} {
			$w.mouse.brate.$rate configure -state normal
		}
		if { ![string compare MouseMan $mseType] } {
			$w.mouse.brate.2400 configure -state disabled
			$w.mouse.brate.4800 configure -state disabled
			if { $baudRate == 2400 || $baudRate == 4800 } {
				set baudRate 1200
			}
		}
	} else {
		foreach rate {1200 2400 4800 9600} {
			$w.mouse.brate.$rate configure -state disabled
		}
	}
	if { ![string compare MMHitTab $mseType] } {
		$w.mouse.srate.title configure -text $messages(mouse.1)
		$w.mouse.srate.scale configure -to 1200 -tickinterval 200 \
			-resolution 20
		$w.mouse.srate.scale configure -state normal
	} else {
		$w.mouse.srate.title configure -text $messages(mouse.2)
		$w.mouse.srate.scale configure -to 150 -tickinterval 25 \
			-resolution 1
		if {[lsearch -exact \
				{MouseMan BusMouse Xqueue OSMouse} \
				$mseType] == -1} {
			$w.mouse.srate.scale configure -state normal
		} else {
			$w.mouse.srate.scale configure -state disabled
			set sampleRate 0
		}
	}
	if { ![string compare MouseSystems $mseType] } {
		$w.mouse.flags.dtr configure -state normal
		$w.mouse.flags.rts configure -state normal
	} else {
		$w.mouse.flags.dtr configure -state disabled
		$w.mouse.flags.rts configure -state disabled
		set clearDTR [set clearRTS 0]
	}
	if {[lsearch -exact {Microsoft MouseMan} $mseType] == -1} {
		$w.mouse.chdmid configure -state disabled
		set chordMiddle 0
	} else {
		$w.mouse.chdmid configure -state normal
	}
	if { !$mseDeviceSelected } {
		$w.mouse.device.entry delete 0 end
		$w.mouse.device.entry insert 0 \
			[Mouse_defaultdevice $mseType]
		Mouse_setlistbox $w $w.mouse.device.list.lb
	}
#	Mouse_setsettings $win
}

proc Mouse_create_widgets { win } {
	global mseType mseDevices baudRate sampleRate mseTypeList clearDTR
	global emulate3Buttons emulate3Timeout chordMiddle clearRTS
	global pc98_EGC mseRes mseButtons messages

	set w [winpathprefix $win]
        if !$pc98_EGC {
	    frame $w.mouse -width 640 -height 420 \
		    -relief ridge -borderwidth 5
	} else {
	    frame $w.mouse -width 640 -height 400 \
		    -relief ridge -borderwidth 5
	}
	frame $w.mouse.top
	frame $w.mouse.mid -relief sunken -borderwidth 3
	frame $w.mouse.bot
	pack $w.mouse.top -side top
	pack $w.mouse.mid -side top -fill x -expand yes
	pack $w.mouse.bot -side top

	label $w.mouse.top.title -text $messages(mouse.3)
	frame $w.mouse.type
	pack $w.mouse.top.title $w.mouse.type -in $w.mouse.top -side top
	set i 0
	foreach Type $mseTypeList {
		set type [string tolower $Type]
		radiobutton $w.mouse.type.$type -text $Type \
			-width 16 \
			-indicatoron false \
			-variable mseType -value $Type \
			-highlightthickness 1 \
			-command [list Mouse_proto_select $win]
		grid $w.mouse.type.$type -column [expr $i % 6] \
			-row [expr $i / 6]
		incr i
	}

	frame $w.mouse.mid.left
	pack $w.mouse.mid.left -side left -fill x -fill y
	frame $w.mouse.device
	pack $w.mouse.device -in $w.mouse.mid.left -side top \
		-pady 3m -padx 3m
	label $w.mouse.device.title -text $messages(mouse.5)
	entry $w.mouse.device.entry -bd 2
	bind $w.mouse.device.entry <Return> \
                "[list Mouse_setlistbox $win $w.mouse.device.list.lb]; \
		focus $w.mouse.em3but"
	frame $w.mouse.device.list
	if !$pc98_EGC {
		listbox $w.mouse.device.list.lb -height 6 \
			-yscroll [list $w.mouse.device.list.sb set]
	} else {
		listbox $w.mouse.device.list.lb -height 4 \
			-yscroll [list $w.mouse.device.list.sb set]
	}
	eval [list $w.mouse.device.list.lb insert end] $mseDevices
        bind  $w.mouse.device.list.lb <Return> \
                "[list Mouse_setentry $win $w.mouse.device.list.lb]; \
		focus $w.mouse.em3but"
        bind  $w.mouse.device.list.lb <ButtonRelease-1> \
                [list Mouse_setentry $win $w.mouse.device.list.lb]
	scrollbar $w.mouse.device.list.sb \
		 -command [list $w.mouse.device.list.lb yview]
 
	pack $w.mouse.device.list.lb -side left -expand yes -fill both
	pack $w.mouse.device.list.sb -side left -expand yes -fill y

	pack $w.mouse.device.title $w.mouse.device.entry \
		$w.mouse.device.list -side top -fill x

	frame $w.mouse.mid.left.buttons
	pack $w.mouse.mid.left.buttons -in $w.mouse.mid.left \
		-side top -fill x -pady 2m
	checkbutton $w.mouse.em3but -text $messages(mouse.6) \
		-indicatoron no -variable emulate3Buttons \
		-command [list Mouse_set_em3but $win]
	checkbutton $w.mouse.chdmid -text $messages(mouse.7) \
		-indicatoron no -variable chordMiddle \
		-command [list Mouse_set_chdmid $win]
	pack $w.mouse.em3but $w.mouse.chdmid -in $w.mouse.mid.left.buttons \
		-side top -fill x -padx 3m -anchor w

	frame $w.mouse.resolution
	pack  $w.mouse.resolution -in $w.mouse.mid.left -side top -pady 2m
	label $w.mouse.resolution.title -text $messages(mouse.19)
	pack  $w.mouse.resolution.title -side top
	frame $w.mouse.resolution.left
	frame $w.mouse.resolution.mid
	frame $w.mouse.resolution.right
	pack  $w.mouse.resolution.left $w.mouse.resolution.mid \
		$w.mouse.resolution.right -side left -expand yes -fill x
	radiobutton $w.mouse.resolution.200 -text $messages(mouse.20) \
		-variable mseRes -value 200
	pack $w.mouse.resolution.200 -side top -fill x -anchor w \
		-in $w.mouse.resolution.left
	radiobutton $w.mouse.resolution.100 -text $messages(mouse.21) \
		-variable mseRes -value 100
	pack $w.mouse.resolution.100 -side top -fill x -anchor w \
		-in $w.mouse.resolution.mid
	radiobutton $w.mouse.resolution.50 -text $messages(mouse.22) \
		-variable mseRes -value 50
	pack $w.mouse.resolution.50 -side top -fill x -anchor w \
		-in $w.mouse.resolution.right

	frame $w.mouse.mid.mid
	pack $w.mouse.mid.mid -side left -fill x -fill y

	frame $w.mouse.brate
#	pack  $w.mouse.brate -in $w.mouse.mid.left -side top -pady 2m
	pack  $w.mouse.brate -in $w.mouse.mid.mid -side top -pady 2m
	label $w.mouse.brate.title -text $messages(mouse.8)
	pack  $w.mouse.brate.title -side top
	frame $w.mouse.brate.left
	frame $w.mouse.brate.right
	pack  $w.mouse.brate.left $w.mouse.brate.right -side left \
		-expand yes -fill x
	foreach rate { 1200 2400 4800 9600 } {
		radiobutton $w.mouse.brate.$rate -text $rate \
			-variable baudRate -value $rate
		pack $w.mouse.brate.$rate -side top -anchor w \
			-in $w.mouse.brate.[expr $rate<4800?"left":"right"]
	}

	frame $w.mouse.flags
	pack $w.mouse.flags -in $w.mouse.mid.mid -side top \
		-fill x -pady 3m
	label $w.mouse.flags.title -text $messages(mouse.10)
	checkbutton $w.mouse.flags.dtr -text $messages(mouse.11) \
		-width 14 -indicatoron no -variable clearDTR
	checkbutton $w.mouse.flags.rts -text $messages(mouse.12) \
		-width 14 -indicatoron no -variable clearRTS
	pack $w.mouse.flags.title $w.mouse.flags.dtr $w.mouse.flags.rts \
		-side top -fill x -padx 3m -anchor w

	frame $w.mouse.buttons
	pack $w.mouse.buttons -in $w.mouse.mid.mid -side top \
		-pady 3m
	label $w.mouse.buttons.title -text $messages(mouse.23)
	pack $w.mouse.buttons.title -side top
	frame $w.mouse.buttons.left
	frame $w.mouse.buttons.mid
	frame $w.mouse.buttons.right
	pack  $w.mouse.buttons.left $w.mouse.buttons.mid \
		$w.mouse.buttons.right -side left -expand yes -fill x
	radiobutton $w.mouse.buttons.3 -text 3 \
		-variable mseButtons -value 3
	pack $w.mouse.buttons.3 -side top -fill x -anchor w \
		-in $w.mouse.buttons.left
	radiobutton $w.mouse.buttons.4 -text 4 \
		-variable mseButtons -value 4
	pack $w.mouse.buttons.4 -side top -fill x -anchor w \
		-in $w.mouse.buttons.mid
	radiobutton $w.mouse.buttons.5 -text 5 \
		-variable mseButtons -value 5
	pack $w.mouse.buttons.5 -side top -fill x -anchor w \
		-in $w.mouse.buttons.right

	frame $w.mouse.srate
	pack $w.mouse.srate -in $w.mouse.mid -side left -fill y -expand yes
	label $w.mouse.srate.title -text $messages(mouse.13)
	scale $w.mouse.srate.scale -orient vertical -from 0 -to 150 \
		-tickinterval 25 -variable sampleRate -state disabled
	pack $w.mouse.srate.title -side top
	pack $w.mouse.srate.scale -side top -fill y -expand yes

	frame $w.mouse.em3tm
	pack $w.mouse.em3tm -in $w.mouse.mid -side left -fill y -expand yes
	label $w.mouse.em3tm.title -text $messages(mouse.14)
	scale $w.mouse.em3tm.scale -orient vertical -from 0 -to 1000 \
		-tickinterval 250 -variable emulate3Timeout -resolution 5
	pack $w.mouse.em3tm.title -side top
	pack $w.mouse.em3tm.scale -side top -fill y -expand yes

	frame $w.mouse.mid.right
	pack $w.mouse.mid.right -side left
	set canv $w.mouse.mid.right.canvas
	if !$pc98_EGC {
	    set canvHeight 4i
	    set canvRect4Height 3.75i
	    set canvTextHeight 2.25i
	} else {
	    set canvHeight 2i
	    set canvRect4Height 1.75i
	    set canvTextHeight 1.50i
	}
	canvas $canv -width 1.5i -height 3i -highlightthickness 0 \
  			-takefocus 0
	$canv create rectangle 0.1i 1i 1.3i 2.5i -fill white \
  			-tag {mbut mbut4}
	$canv create rectangle 0.1i 0.25i 0.5i 1i -fill white \
  			-tag {mbut mbut1}
	$canv create rectangle 0.5i 0.25i 0.9i 1i -fill white \
  			-tag {mbut mbut2}
	$canv create rectangle 0.9i 0.25i 1.3i 1i -fill white \
  			-tag {mbut mbut3}
	$canv create text 0.7i 2.20i -tag coord

	button $w.mouse.mid.right.apply -text $messages(mouse.15) \
		-command [list Mouse_setsettings $win]
	pack $canv $w.mouse.mid.right.apply -side top
	if $pc98_EGC {
	    pack forget $w.mouse.flag
	    pack $w.mouse.flags -in $w.mouse.mid.right -side top \
		    -fill x -pady 3m
	}

	label $w.mouse.bot.mesg -text $messages(mouse.16) \
		-foreground [$w.mouse.top.title cget -foreground]
	pack $w.mouse.bot.mesg

	Mouse_getsettings $w
}

proc Mouse_activate { win } {
	global mseHelpShown

	set w [winpathprefix $win]
	pack $w.mouse -side top -fill both -expand yes

	set canv $w.mouse.mid.right.canvas
	bind $win <ButtonPress>	    [list $canv itemconfigure mbut%b -fill black]
	bind $win <ButtonRelease>   [list $canv itemconfigure mbut%b -fill white]
	bind $win <ButtonPress-4>   [list $canv itemconfigure mbut4 -fill black;
				   $canv itemconfigure coord -fill white]
	bind $win <ButtonRelease-4> [list $canv itemconfigure mbut4 -fill white;
				   $canv itemconfigure coord -fill black]
	bind $win <Motion>          [list $canv itemconfigure coord -text (%X,%Y)]

	$canv itemconfigure mbut  -fill white
	$canv itemconfigure coord -fill black

	set ifcmd {if { [string compare [focus] %s.mouse.device.entry]
			!= 0 } { %s %s } }
			
	bind $win a [format $ifcmd $w Mouse_setsettings $win ]
	bind $win b [format $ifcmd $w Mouse_nextbaudrate $win ]
	bind $win c [format $ifcmd $w $w.mouse.chdmid invoke ]
	bind $win d [format $ifcmd $w $w.mouse.flags.dtr invoke ]
	bind $win e [format $ifcmd $w $w.mouse.em3but invoke ]
	bind $win l [format $ifcmd $w Mouse_nextresolution $win ]
	bind $win n [format $ifcmd $w Mouse_selectentry $win ]
	bind $win p [format $ifcmd $w Mouse_nextprotocol $win ]
	bind $win r [format $ifcmd $w $w.mouse.flags.rts invoke ]
	bind $win s [format $ifcmd $w Mouse_incrsamplerate $win ]
	bind $win t [format $ifcmd $w Mouse_increm3timeout $win ]
	bind $win 3 [format $ifcmd $w $w.mouse.buttons.3 invoke ]
	bind $win 4 [format $ifcmd $w $w.mouse.buttons.4 invoke ]
	bind $win 5 [format $ifcmd $w $w.mouse.buttons.5 invoke ]
	if ![info exists mseHelpShown] {
		Mouse_popup_help $win
		set mseHelpShown yes
	}
}

proc Mouse_deactivate { win } {
	set w [winpathprefix $win]
	pack forget $w.mouse

	bind $win <ButtonPress>		""
	bind $win <ButtonRelease>	""
	bind $win <ButtonPress-4>	""
	bind $win <ButtonRelease-4>	""
	bind $win <Motion>		""

	bind $win a			""
	bind $win b			""
	bind $win c			""
	bind $win d			""
	bind $win e			""
	bind $win n			""
	bind $win p			""
	bind $win r			""
	bind $win s			""
	bind $win t			""
}

proc Mouse_selectentry { win } {
	set w [winpathprefix $win]
	if { [ $w.mouse.device.entry cget -state] != "disabled" } {
		focus $w.mouse.device.entry
	}
}

proc Mouse_nextprotocol { win } {
	global mseType mseTypeList

	set w [winpathprefix $win]
	set idx [lsearch -exact $mseTypeList $mseType]
	do {
		incr idx
		if { $idx >= [llength $mseTypeList] } {
			set idx 0
		}
		set mseType [lindex $mseTypeList $idx]
		set mtype [string tolower $mseType]
	} while { [$w.mouse.type.$mtype cget -state] == "disabled" }
	Mouse_proto_select $w
}

proc Mouse_nextbaudrate { win } {
	global baudRate

	set w [winpathprefix $win]
	if { [$w.mouse.brate.$baudRate cget -state] == "disabled" } {
		return
	}
	do {
		set baudRate [expr $baudRate*2]
		if { $baudRate > 9600 } {
			set baudRate 1200
		}
	} while { [$w.mouse.brate.$baudRate cget -state] == "disabled" }
}

proc Mouse_nextresolution { win } {
	global mseRes

	set w [winpathprefix $win]
	set mseRes [expr $mseRes/2]
	if { $mseRes < 50 } {
		set mseRes 200
	}
	$w.mouse.resolution.$mseRes invoke
}

proc Mouse_incrsamplerate { win } {
	global sampleRate

	set w [winpathprefix $win]
	if { [$w.mouse.srate.scale cget -state] == "disabled" } {
		return
	}

	set max [$w.mouse.srate.scale cget -to]
	set interval [expr [$w.mouse.srate.scale cget -tickinterval]/2.0]
	if { $sampleRate+$interval > $max } {
		set sampleRate 0
	} else {
		set sampleRate [expr $sampleRate+$interval]
	}
}

proc Mouse_increm3timeout { win } {
	global emulate3Timeout

	set w [winpathprefix $win]
	if { [$w.mouse.em3tm.scale cget -state] == "disabled" } {
		return
	}
	set max [$w.mouse.em3tm.scale cget -to]
	set interval [expr [$w.mouse.em3tm.scale cget -tickinterval]/2.0]
	if { $emulate3Timeout+$interval > $max } {
		set emulate3Timeout 0
	} else {
		set emulate3Timeout [expr $emulate3Timeout+$interval]
	}
}

proc Mouse_set_em3but { win } {
	global emulate3Buttons chordMiddle

	set w [winpathprefix $win]
	if { $emulate3Buttons } {
		$w.mouse.em3tm.scale configure -state normal
	} else {
		$w.mouse.em3tm.scale configure -state disabled
	}
	if { $chordMiddle && $emulate3Buttons } {
		$w.mouse.chdmid invoke
	}
}

proc Mouse_set_chdmid { win } {
	global emulate3Buttons chordMiddle

	set w [winpathprefix $win]
	if { $chordMiddle && $emulate3Buttons } {
		$w.mouse.em3but invoke
	}
}

proc Mouse_setsettings { win } {
	global mseType baudRate sampleRate clearDTR Pointer
	global emulate3Buttons emulate3Timeout chordMiddle clearRTS
	global mseRes mseButtons mseDeviceSelected messages

	set w [winpathprefix $win]
	$w.mouse.bot.mesg configure -foreground black \
		-text $messages(mouse.4)
	$win configure -cursor watch
	update idletasks
	set mseDeviceSelected 1
	set msedev [$w.mouse.device.entry get]
	set em3but off
	set chdmid off
	if $emulate3Buttons {set em3but on}
	if $chordMiddle {set chdmid on}
	set flags ""
	if $clearDTR {lappend flags ClearDTR}
	if $clearRTS {lappend flags ClearRTS}
	if { "$mseType" == "MouseSystems" } {lappend flags ReOpen}
	if [string length $msedev] {
		set Pointer(RealDev) $msedev
		check_tmpdirs
		unlink $Pointer(Device)
		if [link $Pointer(RealDev) $Pointer(Device)] {
			lappend flags ReOpen
		}
	}
	check_tmpdirs
	set result [catch { eval [list xf86misc_setmouse \
		$msedev $mseType $baudRate $sampleRate $mseRes $mseButtons \
		$em3but $emulate3Timeout $chdmid] $flags } ]
	if { $result } {
		bell -displayof $w
	} else {
		set Pointer(Protocol) $mseType
		if { [$w.mouse.brate.1200 cget -state] == "disabled" } {
			set Pointer(BaudRate) ""
		} else {
			set Pointer(BaudRate) $baudRate
		}
		if { [$w.mouse.srate.scale cget -state] == "disabled"
				|| $sampleRate == 0 } {
			set Pointer(SampleRate) ""
		} else {
			set Pointer(SampleRate) $sampleRate
		}
		set Pointer(Resolution) $mseRes
		set Pointer(Buttons) $mseButtons
		set Pointer(Emulate3Buttons) [expr $emulate3Buttons?"ON":""]
		set Pointer(Emulate3Timeout) \
			[expr $emulate3Buttons?$emulate3Timeout:""]
		set Pointer(ChordMiddle) [expr $chordMiddle?"ON":""]
		set Pointer(ClearDTR) [expr $clearDTR?"ON":""]
		set Pointer(ClearRTS) [expr $clearRTS?"ON":""]
	}
	$w.mouse.bot.mesg configure \
		-text $messages(mouse.9) \
		-foreground [$w.mouse.top.title cget -foreground]
	$win configure -cursor top_left_arrow
}

proc Mouse_getsettings { win } {
	global mseType mseTypeList baudRate sampleRate clearDTR Pointer
	global emulate3Buttons emulate3Timeout chordMiddle clearRTS
	global mseRes mseButtons mseDeviceSelected SupportedMouseTypes

	set w [winpathprefix $win]
	set initlist	[xf86misc_getmouse]
	set initdev	[lindex $initlist 0]
	set inittype	[lindex $initlist 1]
	set initbrate	[lindex $initlist 2]
	set initsrate	[lindex $initlist 3]
	set initres	[lindex $initlist 4]
	set initbtn	[lindex $initlist 5]
	set initem3btn	[lindex $initlist 6]
	set initem3tm	[lindex $initlist 7]
	set initchdmid	[lindex $initlist 8]
	set initflags	[lrange $initlist 9 end]

	set mseDeviceSelected 1
	if [getuid] {
	    pack forget $w.mouse.device.title
	    pack forget $w.mouse.device.entry
	    pack forget $w.mouse.device.list
	} else {
	    if { [info exists Pointer(RealDev)] } {
		$w.mouse.device.entry insert 0 $Pointer(RealDev)
	    } else {
		set default [Mouse_defaultdevice $inittype]
		if { [string length $default] } {
		    $w.mouse.device.entry insert 0 $default
		    set mseDeviceSelected 0
		} else {
		    pack forget $w.mouse.device.title
		    pack forget $w.mouse.device.entry
		    pack forget $w.mouse.device.list
		}
	    }
	}
	Mouse_setlistbox $w $w.mouse.device.list.lb
	$w.mouse.brate.$initbrate invoke

	set chordMiddle     [expr [string compare $initchdmid on] == 0]
	set emulate3Buttons [expr [string compare $initem3btn on] == 0]
	set emulate3Timeout $initem3tm
	set sampleRate      $initsrate
	set mseRes          $initres
	if { $mseRes <= 0 } {
		set mseRes 100
	}
	$w.mouse.resolution.$mseRes invoke
	set mseButtons      $initbtn
	if { $mseButtons < 3 } {
		set mseButtons 3
	} elseif { $mseButtons > 5 } {
		set mseButtons 5
	}
	$w.mouse.buttons.$mseButtons invoke
	set clearDTR  [expr [string first $initflags ClearDTR] >= 0]
	set clearRTS  [expr [string first $initflags ClearRTS] >= 0]

	foreach mse $mseTypeList {
		$w.mouse.type.[string tolower $mse] \
			configure -state disabled
	}
	set mtype [string tolower $inittype]
	if { $mtype == "osmouse" || $mtype == "xqueue" } {
		$w.mouse.type.$mtype  configure -state normal
	} else {
		foreach mse $SupportedMouseTypes {
			$w.mouse.type.[string tolower $mse] \
				configure -state normal
		}
	}
	$w.mouse.type.$mtype invoke
}

proc Mouse_setentry { win lbox } {
	global Pointer mseDeviceSelected

	set w [winpathprefix $win]
	set idx [$lbox curselection]
	if ![string length $idx] {
		return
	}
	$w.mouse.device.entry delete 0 end
	set devname [$lbox get $idx]
	$w.mouse.device.entry insert end $devname
	set Pointer(RealDev) $devname
	set mseDeviceSelected 1
}

proc Mouse_setlistbox { win lbox } {
	global Pointer mseDevices

	set w [winpathprefix $win]
	set devname [$w.mouse.device.entry get]
	if ![string length $devname] {
		return
	}
	set Pointer(RealDev) $devname
	if { [set idx [lsearch $mseDevices $devname]] != -1 } {
		$lbox selection clear 0 end
		$lbox selection set $idx
		$lbox activate $idx
		$lbox see $idx
	}
}

proc Mouse_defaultdevice { mousetype } {
	global mseDevices

	switch $mousetype {
		PS/2	 { set idx [lsearch -regexp $mseDevices \
					{/dev/p[ms].*} ] }
		IMPS/2	 { set idx [lsearch -regexp $mseDevices \
					{/dev/p[ms].*} ] }
		ThikingMousePS/2 { set idx [lsearch -regexp $mseDevices \
					{/dev/p[ms].*} ] }
		MouseManPlusPS/2 { set idx [lsearch -regexp $mseDevices \
					{/dev/p[ms].*} ] }
		GlidePointPS/2	 { set idx [lsearch -regexp $mseDevices \
					{/dev/p[ms].*} ] }
		NetMousePS/2	 { set idx [lsearch -regexp $mseDevices \
					{/dev/p[ms].*} ] }
		NetScrollPS/2	 { set idx [lsearch -regexp $mseDevices \
					{/dev/p[ms].*} ] }
		BusMouse { set idx [lsearch -regexp $mseDevices \
					/dev/.*bm|/dev/mse.* ] }
		SysMouse { set idx [lsearch -regexp $mseDevices \
					/dev/sysmouse.* ] }
		OsMouse  -
		Xqueue	 { return "" }
		default	 { set idx [lsearch -regexp $mseDevices \
					/dev/cua*|/dev/tty* ] }
	}
	if { $idx == -1 } {
		set idx 0
	}
	return [lindex $mseDevices $idx]
}

