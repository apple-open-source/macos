# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/setuplib.tcl,v 3.23 1999/04/05 07:13:01 dawes Exp $
#
# Copyright 1996 by Joseph V. Moss <joe@XFree86.Org>
#
# See the file "LICENSE" for information regarding redistribution terms,
# and for a DISCLAIMER OF ALL WARRANTIES.
#
# $XConsortium: setuplib.tcl /main/3 1996/10/25 10:21:33 kaleb $

#
# Library of routines used by XF86Setup
#

# Initialize all the variables that hold configuration info

proc initconfig {xwinhome} {
	global Files ServerFlags Module Keyboard Pointer
	global "Monitor_Primary Monitor" "Device_Primary Card"
	global DeviceIDs MonitorIDs
	global Scrn_Accel Scrn_Mono Scrn_VGA2 Scrn_VGA16 Scrn_SVGA
	global pc98 pc98_EGC

	set fontdir  "$xwinhome/lib/X11/fonts"
	set Files(FontPath)	[list $fontdir/misc:unscaled \
		  $fontdir/75dpi:unscaled $fontdir/100dpi:unscaled \
		  $fontdir/Type1 $fontdir/Speedo  \
		  $fontdir/misc $fontdir/75dpi $fontdir/100dpi ]
	set Files(RGBPath)		$xwinhome/lib/X11/rgb
	set Files(ModulePath)		""
	set Files(LogFile)		""


	if !$pc98 {
	    set Module(Load)	vga
	} else {
	    if !$pc98_EGC {
		set Module(Load)	pegc
	    } else {
		set Module(Load)	egc
	    }
	}

	set ServerFlags(NoTrapSignals)			""
	set ServerFlags(DontZap)			""
	set ServerFlags(DontZoom)			""
	set ServerFlags(DisableVidModeExtension)	""
	set ServerFlags(AllowNonLocalXvidtune)		""
	set ServerFlags(DisableModInDev)		""
	set ServerFlags(AllowNonLocalModInDev)		""

	set Keyboard(Protocol)		Standard
	set Keyboard(ServerNumLock)	""
	foreach key { AutoRepeat LeftAlt RightAlt ScrollLock
			RightCtl XLeds VTSysReq VTInit
			XkbKeycodes XkbTypes XkbCompat
			XkbSymbols XkbGeometry XkbKeymap
			Panix106 } {
		set Keyboard($key) ""
	}
	if !$pc98 {
		set Keyboard(XkbDisable)	""
		set Keyboard(XkbRules)		xfree86
		set Keyboard(XkbModel)		pc101
		set Keyboard(XkbLayout)		us
		set Keyboard(XkbVariant)	""
		set Keyboard(XkbOptions)	""
		set Pointer(Protocol)		Microsoft
		set Pointer(Emulate3Buttons)	""
	} else {
		set Keyboard(XkbDisable)	""
		set Keyboard(XkbRules)		xfree98
		set Keyboard(XkbModel)		pc98
		set Keyboard(XkbLayout)		nec/jp
		set Keyboard(XkbVariant)	""
		set Keyboard(XkbOptions)	""
		set Pointer(Protocol)		BusMouse
		set Pointer(Emulate3Buttons)	1
	}

#	set Pointer(Device)		/dev/mouse
	set Pointer(Device)		""
	set Pointer(BaudRate)		""
	set Pointer(Emulate3Timeout)	""
	set Pointer(ChordMiddle)	""
	set Pointer(SampleRate)		""
	set Pointer(Resolution)		""
	set Pointer(Buttons)		""
	set Pointer(ClearDTR)		""
	set Pointer(ClearRTS)		""
	set Pointer(AlwaysCore)		""

	if !$pc98_EGC {
	    set id				"Primary Monitor"
	    set Monitor_${id}(VendorName)	Unknown
	    set Monitor_${id}(ModelName)	Unknown
	    set Monitor_${id}(HorizSync)	31.5
	    set Monitor_${id}(VertRefresh)	60
	    set Monitor_${id}(Gamma)	""
	    set MonitorIDs [list $id]
	} else {
	    set id				"Primary Monitor"
	    set Monitor_${id}(VendorName)	Unknown
	    set Monitor_${id}(ModelName)	Unknown
	    set Monitor_${id}(HorizSync)	24.8
	    set Monitor_${id}(VertRefresh)	56.4
	    set Monitor_${id}(Gamma)	""
	    set MonitorIDs [list $id]
	}
	set id				"Primary Card"
	set Device_${id}(VendorName)	Unknown
	set Device_${id}(BoardName)	Unknown
	if !$pc98 {
		set Device_${id}(Server)	SVGA
	} else {
		if !$pc98_EGC {
			set Device_${id}(Server)	PEGC
		} else {
			set Device_${id}(Server)	EGC
		}
		set uname [exec uname]
		if {$uname == {FreeBSD}} {
			set Pointer(Device)	/dev/mse0
		} elseif {$uname == {NetBSD}} {
			set Pointer(Device)	/dev/lms0
		}
	}
	foreach key {Chipset Ramdac DacSpeed Clocks ClockChip
			ClockProg Options VideoRam BIOSBase Membase
			IOBase DACBase POSBase COPBase VGABase
			Instance Speedup S3MNAdjust S3MClk S3RefClock
			ExtraLines} {
		set Device_${id}($key) ""
	}
	set DeviceIDs [list $id]

	set Scrn_Accel(Driver)		"Accel"
	set Scrn_Accel(Device)		"Primary Card"
	set Scrn_Accel(Monitor)		"Primary Monitor"
	set Scrn_Accel(ScreenNo)	""
	set Scrn_Accel(BlankTime)	""
	set Scrn_Accel(StandbyTime)	""
	set Scrn_Accel(SuspendTime)	""
	set Scrn_Accel(OffTime)		""
	set Scrn_Accel(DefaultColorDepth)	""

	if !$pc98 {
		array set Scrn_Mono  [array get Scrn_Accel]
		array set Scrn_VGA2  [array get Scrn_Accel]
	}
	array set Scrn_VGA16 [array get Scrn_Accel]
	array set Scrn_SVGA  [array get Scrn_Accel]

	if !$pc98 {
		set Scrn_Mono(Driver)		"Mono"
		set Scrn_VGA2(Driver)		"VGA2"
	}
	set Scrn_SVGA(Driver)		"SVGA"
	set Scrn_VGA16(Driver)		"VGA16"

	if !$pc98 {
		set Scrn_Mono(Depth,1)		1
		set Scrn_VGA2(Depth,1)		1
	}
	set Scrn_VGA16(Depth,4)		4

	foreach depth {4 8 15 16 24 32} {
		set Scrn_SVGA(Depth,$depth)	$depth
		set Scrn_Accel(Depth,$depth)	$depth
	}
}


# Write a XF86Config file to the given fd

proc writeXF86Config {filename args} {
	global Files Module ServerFlags Keyboard Pointer UseLoader
	global MonitorIDs DeviceIDs MonitorStdModes SelectedMonitorModes
	global Scrn_Accel Scrn_Mono Scrn_VGA2 Scrn_VGA16 Scrn_SVGA
	global pc98 pc98_EGC haveSelectedModes DefaultColorDepth

	set generic_vga [ expr {[lsearch -exact $args -generic] >= 0} ]
	#puts stderr "generic_vga = $generic_vga"
	check_tmpdirs
	set fd [open $filename w]
	puts $fd "# XF86Config auto-generated by XF86Setup"
	puts $fd "#"
	puts $fd "# Copyright (c) 1996 by The XFree86 Project, Inc.\n"
	puts $fd "#"
	puts $fd "# Permission is hereby granted, free of charge, to any person obtaining a"
	puts $fd {# copy of this software and associated documentation files (the "Software"),}
	puts $fd "# to deal in the Software without restriction, including without limitation"
	puts $fd "# the rights to use, copy, modify, merge, publish, distribute, sublicense,"
	puts $fd "# and/or sell copies of the Software, and to permit persons to whom the"
	puts $fd "# Software is furnished to do so, subject to the following conditions:"
	puts $fd "#"
	puts $fd "# The above copyright notice and this permission notice shall be included in"
	puts $fd "# all copies or substantial portions of the Software."
	puts $fd "#"
	puts $fd {# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR}
	puts $fd "# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,"
	puts $fd "# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL"
	puts $fd "# THE XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,"
	puts $fd "# WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF"
	puts $fd "# OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE"
	puts $fd "# SOFTWARE."
	puts $fd "#"
	puts $fd "# Except as contained in this notice, the name of the XFree86 Project shall"
	puts $fd "# not be used in advertising or otherwise to promote the sale, use or other"
	puts $fd "# dealings in this Software without prior written authorization from the"
	puts $fd "# XFree86 Project."
	puts $fd "#"
	puts $fd ""
	puts $fd "# See 'man XF86Config' for info on the format of this file"

	puts $fd ""
	puts $fd {Section "Files"}
	puts $fd "   RgbPath    \"$Files(RGBPath)\""
	foreach path $Files(FontPath) {
		puts $fd "   FontPath   \"$path\""
	}
	puts $fd "EndSection"

	if { $UseLoader && ($generic_vga || [llength $Module(Load)]) } {
		puts $fd ""
		puts $fd {Section "Module"}
		if $generic_vga {
			if !$pc98 {
				puts $fd {   Load "vga"}
			} else {
			    if !$pc98_EGC {
				puts $fd {   Load "pegc"}
			    } else {
				puts $fd {   Load "egc"}
			    }
			}
		} else {
			foreach module $Module(Load) {
				puts $fd "   Load \"$module\""
			}
		}
		puts $fd {   Load "extmod"}
		puts $fd "EndSection"
	}

	puts $fd ""
	puts $fd {Section "ServerFlags"}
	foreach key {NoTrapSignals DontZap DontZoom} {
		if { [string length $ServerFlags($key)] } {
			puts "   $ServerFlags($key)"
		}
	}
	puts $fd "EndSection"

	puts $fd ""
	puts $fd {Section "Keyboard"}
	puts $fd "   Protocol        \"$Keyboard(Protocol)\""
	set xkbvars ""
	foreach key [array names Keyboard {Xkb[ABCE-Z]*}] {
		append xkbvars $Keyboard($key)
	}
	if { ![string length $xkbvars]
			&& [string length $Keyboard(ServerNumLock)] } {
		puts $fd "   ServerNumLock"
	}
	foreach key { AutoRepeat LeftAlt RightAlt ScrollLock RightCtl XLeds } {
		if { [string length $Keyboard($key)] } {
			puts $fd [format "   %-15s %s" $key $Keyboard($key)]
		}
	}
	foreach key { XkbKeycodes XkbTypes XkbCompat
		XkbSymbols XkbGeometry XkbKeymap
		XkbRules XkbModel XkbLayout XkbVariant XkbOptions} {
		if { [string length $Keyboard($key)] } {
			puts $fd [format "   %-15s \"%s\"" \
				$key $Keyboard($key)]
		}
	}
	#if { [string length $Keyboard(XkbDisable)] } {
	#	puts $fd "   XkbDisable"
	#}
	if { [string length $Keyboard(VTInit)] } {
		puts $fd "   VTInit         \"$Keyboard(VTInit)\""
	}
	if { [string length $Keyboard(VTSysReq)] } {
		puts $fd "   VTSysReq"
	}
	puts $fd "EndSection"

	puts $fd ""
	puts $fd {Section "Pointer"}
	puts $fd "   Protocol        \"$Pointer(Protocol)\""
	if { [lsearch -exact $args -realdevice] >= 0 
		    && [info exists Pointer(RealDev)] } {
		set realdev $Pointer(RealDev)
	} else {
		set realdev ""
	}
	if { [string length $realdev] } {
		if {[info exists Pointer(OldLink)]
			&& ![string compare [readlink $Pointer(OldLink)] $realdev]} {
		    puts $fd "   Device          \"$Pointer(OldLink)\""
		} else {
		    puts $fd "   Device          \"$realdev\""
		}
	} elseif { [string length $Pointer(Device)] } {
		puts $fd "   Device          \"$Pointer(Device)\""
	}
	foreach key {BaudRate Emulate3Timeout SampleRate Resolution Buttons} {
		if { [string length $Pointer($key)] && $Pointer($key) } {
			puts $fd [format "   %-15s %s" $key $Pointer($key)]
		}
	}
	foreach key {Emulate3Buttons ChordMiddle ClearDTR ClearRTS} {
		if { [string length $Pointer($key)] } {
			puts $fd "   $key"
		}
	}
	puts $fd "EndSection"

	set modeNames ""
	foreach id $MonitorIDs {
	    global Monitor_$id
	    puts $fd ""
	    puts $fd {Section "Monitor"}
	    puts $fd "   Identifier      \"$id\""
	    if { [string length [set Monitor_${id}(VendorName)]] } {
		puts $fd "   VendorName      \"[set Monitor_${id}(VendorName)]\""
	    }
	    if { [string length [set Monitor_${id}(ModelName)]] } {
		puts $fd "   ModelName       \"[set Monitor_${id}(ModelName)]\""
	    }
	    puts $fd "   HorizSync       [set Monitor_${id}(HorizSync)]"
	    puts $fd "   VertRefresh     [set Monitor_${id}(VertRefresh)]"
	    if { [string length [set Monitor_${id}(Gamma)]] } {
		puts $fd "   Gamma           [set Monitor_${id}(Gamma)]"
	    }
	    set modepattern "None"
	    if { [lsearch -exact $args -vgamode] >= 0 } {
		if !$pc98_EGC {
		    set modepattern " 640x480*"
		} else {
		    set modepattern " 640x400*"
		}
	    }
	    if { [lsearch -exact $args -defaultmodes] >= 0 } {
		set modepattern "*"
	    }
	    if { [string compare None $modepattern] != 0} {
		if { $haveSelectedModes <= 0 } {
#		    puts stderr "No selected modes"
		    foreach desc [lsort -decreasing \
				      [array names MonitorStdModes $modepattern]] {
			set modeline $MonitorStdModes($desc)
			puts $fd "# $desc"
			set id [format "%dx%d" \
				    [lindex $modeline 1] [lindex $modeline 5]]
			puts $fd [format "   Modeline  %-11s %s" \
				      "\"$id\""  $modeline]
			lappend modeNames $id
		    }
		} else {
		    foreach desc [lsort -decreasing \
				[array names SelectedMonitorModes $modepattern]] {
			set modeline $SelectedMonitorModes($desc)
			if ![string match \#removed $modeline] {
			    puts $fd "# $desc"
			    set id [format "%dx%d" \
					[lindex $modeline 1] [lindex $modeline 5]]
			    puts $fd [format "   Modeline  %-11s %s" \
					  "\"$id\""  $modeline]
			    lappend modeNames $id
			}
		    }
		}
	    } else {
		set dispof [lsearch -exact $args -displayof]
		if { $dispof >= 0 } {
			set dispwin [lindex $args [expr $dispof+1]]
			set modelist [xf86vid_getallmodelines \
				-displayof $dispwin]

		} else {
			set modelist [xf86vid_getallmodelines]
		}
		foreach modeline $modelist {
			set id [format "%dx%d" \
			    [lindex $modeline 1] [lindex $modeline 5]]
			puts $fd [format "   Modeline  %-11s %s" \
			    "\"$id\""  $modeline]
			lappend modeNames $id
		}
	    }
	    puts $fd "EndSection"
	}

	set modesList [lrmdups -decreasing -command mode_compare $modeNames]

	foreach id $DeviceIDs {
	    global Device_$id
	    puts $fd ""
	    puts $fd {Section "Device"}
	    puts $fd "   Identifier      \"$id\""
	    foreach key {VendorName BoardName Ramdac Speedup} {
		if { [string length [set Device_${id}($key)]] } {
			puts $fd [format "   %-15s \"%s\"" \
				$key [set Device_${id}($key)] ]
		}
	    }
	    if { !$generic_vga } {
		set chipset [set Device_${id}(Chipset)]
		if { [string length $chipset] } {
		    puts $fd "   Chipset         \"$chipset\""
		}
	    } else {
		puts $fd "   Chipset         \"generic\""
	    }
	    foreach opt [set Device_${id}(Options)] {
		puts $fd "   Option          \"$opt\""
	    }
	    if { [lsearch -exact $args -noclocks] < 0 } {
		set clockchip [set Device_${id}(ClockChip)]
		if { [string length $clockchip] } {
			puts $fd "   ClockChip       \"$clockchip\""
		}
		set clocks    [set Device_${id}(Clocks)]
		if { [string length $clocks] } {
			puts $fd "   Clocks          $clocks"
		}
		set clockprog [set Device_${id}(ClockProg)]
		if { [string length $clockprog] } {
			puts $fd "   ClockProg       $clockprog"
		}
	    }
	    foreach key {DacSpeed VideoRam BIOSBase Membase IOBase
			DACBase POSBase COPBase VGABase Instance
			S3MNAdjust S3MClk S3RefClock} {
		if { [string length [set Device_${id}($key)]] } {
			puts $fd [format "   %-15s %s" \
				$key [set Device_${id}($key)] ]
		}
	    }
	    if { [string length [set Device_${id}(ExtraLines)]] } {
		puts $fd [set Device_${id}(ExtraLines)]
	    }
	    puts $fd "EndSection"
	}

	foreach drvr {Accel SVGA VGA16 VGA2 Mono} {

		if $pc98 {
			if {![string compare $drvr "Mono"] || \
			    ![string compare $drvr "VGA2"]} \
				continue
		}
		if { [string compare $drvr [set Scrn_${drvr}(Driver)]] } \
			continue
		puts $fd ""
		puts $fd {Section "Screen"}
		puts $fd "   Driver          \"$drvr\""
		puts $fd "   Device          \"[set Scrn_${drvr}(Device)]\""
		puts $fd "   Monitor         \"[set Scrn_${drvr}(Monitor)]\""
		if { ![string compare $drvr "Accel"] ||
		     ![string compare $drvr "SVGA"] } {
			puts $fd "   DefaultColorDepth $DefaultColorDepth"
		}
		foreach key {ScreenNo BlankTime StandbyTime SuspendTime
			     OffTime } {
			if { [string length [set Scrn_${drvr}($key)]] } {
				puts $fd [format "   %-15s %s" \
					$key [set Scrn_${drvr}($key)] ]
			}
		}
		foreach depth {1 4 8 15 16 24 32} {
		    if [info exists Scrn_${drvr}(Depth,$depth)] {
			puts $fd {   SubSection "Display"}
			puts $fd "      Depth        $depth"
			puts -nonewline $fd "      Modes       "
			foreach mode $modesList {
			    puts -nonewline $fd " \"$mode\""
			}
			puts $fd ""
		        if [info exists Scrn_${drvr}(Visual,$depth)] {
			    puts $fd [format "      Visual       \"%s\"" \
				[set Scrn_${drvr}(Visual,$depth)] ]
			}
		        if { [lsearch -exact $args -vgamode] < 0 } {
		            if [info exists Scrn_${drvr}(Virtual,$depth)] {
			        puts $fd [format "      Virtual       %s" \
				    [set Scrn_${drvr}(Virtual,$depth)] ]
			    }
		        }
			foreach key {ViewPort White Black Weight
				InvertVCLK EarlySC BlankDelay} {
			    if [info exists Scrn_${drvr}($key,$depth)] {
				puts $fd [format "      %-12s %s" \
					$key [set Scrn_${drvr}($key,$depth)] ]
			    }
			}
			puts $fd "   EndSubSection"
		    }
		}
		puts $fd "EndSection"
	}
	close $fd
}


# compare two mode names of the form 0000x000
#   horizontal width is compared first, then height
proc mode_compare { itemA itemB } {
	set Alist [split $itemA x]
	set Blist [split $itemB x]
	if { [lindex $Alist 0] == [lindex $Blist 0] } {
		return [expr [lindex $Alist 1] - [lindex $Blist 1]]
	}
	return [expr [lindex $Alist 0] - [lindex $Blist 0]]
}

proc set_resource_defaults {} {
	# Colors chosen to work with the VGA16 server
	option add *background			grey
	option add *foreground			blue
	option add *selectColor			cyan
	option add *selectForeground		black
	option add *disabledForeground		""	;# stippled
	option add *highlightBackground		grey
	option add *font			fixed
	option add *Frame.highlightThickness	0
	option add *Listbox.exportSelection	no
}

proc create_main_window { w } {
	toplevel $w
	global pc98_EGC

        if !$pc98_EGC {
	    $w configure -height 480 -width 640 -highlightthickness 0
	} else {
	    $w configure -height 400 -width 640 -highlightthickness 0
	}
	pack propagate $w no
	wm geometry $w +0+0
        if !$pc98_EGC {
	    #wm minsize $w 640 480
	} else {
	    #wm minsize $w 640 400
	}
}

proc set_default_arrow_bindings { } {
	foreach class {Button Checkbutton Radiobutton Entry} {
		bind $class <Key-Down>	{focus [findfocuswindow %W y 15]}
		bind $class <Key-Right>	{focus [findfocuswindow %W x 15]}
		bind $class <Key-Up>	{focus [findfocuswindow %W y -15]}
		bind $class <Key-Left>	{focus [findfocuswindow %W x -15]}
	}
}

proc start_server { server configfile outfile } {
	global env TmpDir Xwinhome serverNumber UseLoader pc98

	if { ![info exists serverNumber] } {
		set serverNumber 7
	} else {
		incr serverNumber
	}
	set env(DISPLAY) [set disp :$serverNumber]

	if !$pc98 {
	    if { $UseLoader } {
	        if { ![string compare $server VGA16] } {
	    	    set pid [exec $Xwinhome/bin/XF86_LOADER $disp -bpp 4 \
		        -allowMouseOpenFail -xf86config $configfile \
		        -bestRefresh >& $TmpDir/$outfile & ]
	        } else {
		    set pid [exec $Xwinhome/bin/XF86_LOADER $disp \
		        -allowMouseOpenFail -xf86config $configfile \
		        -bestRefresh >& $TmpDir/$outfile & ]
	        }
	    } else {
	        set pid [exec $Xwinhome/bin/XF86_$server $disp \
		        -allowMouseOpenFail -xf86config $configfile \
		        -bestRefresh >& $TmpDir/$outfile & ]
	    }
	} else {
	    if { $UseLoader } {
	        if { ![string compare $server EGC] } {
	    	    set pid [exec $Xwinhome/bin/XF98_LOADER $disp -bpp 4\
		        -allowMouseOpenFail -xf86config $configfile \
		        -bestRefresh >& $TmpDir/$outfile & ]
		} else {
	    	    set pid [exec $Xwinhome/bin/XF98_LOADER $disp\
		        -allowMouseOpenFail -xf86config $configfile \
		        -bestRefresh >& $TmpDir/$outfile & ]
		}
	    } else {
	        if { ![string compare $server EGC] } {
	            set pid [exec $Xwinhome/bin/XF98_$server $disp -bpp 4 \
		            -allowMouseOpenFail -xf86config $configfile \
		            -bestRefresh >& $TmpDir/$outfile & ]
		} else {
	            set pid [exec $Xwinhome/bin/XF98_$server $disp \
		            -allowMouseOpenFail -xf86config $configfile \
		            -bestRefresh >& $TmpDir/$outfile & ]
		}
	    }
	}

	sleep 9
	set trycount 0
	while { [incr trycount] < 10 } {
		if ![process_running $pid] {
			return 0
		}
		sleep 2
		if [server_running $disp] {
			return $pid
		}
	}
	return -1
}

proc clear_scrn {} {
	global NoCurses

	if { $NoCurses } {
		set blank \n\n\n\n\n
		puts $blank$blank$blank$blank$blank
		flush stdout
	} else {
		#catch {exec $Dialog --clear >&@stdout} ret
	}
}

proc mesg { text {buttontype okay} {title ""} } {
	global NoCurses message  messages

	if { $NoCurses } {
		puts -nonewline $text
		flush stdout
		if { [string compare $buttontype yesno] == 0 } {
			puts -nonewline " "
			flush stdout
			gets stdin response
			return [string match {[yY]} [string index $response 0]]
		}
		if { [string compare $buttontype okay] == 0 } {
			puts -nonewline $messages(setuplib.1)
			flush stdout
			gets stdin response
		}
		return
	}
	# else
	set textlist [split $text \n]
	set height [expr [llength $textlist]+2]
	if { [string compare $buttontype info] != 0 } {
		incr height 2
	}
	set width 0
	foreach line $textlist {
		if { [string length $line] > $width } {
			set width [string length $line]
		}
	}
	incr width 6
	set minwidth	12
	set scrninfo	[stdscr info]
	set scrnht	[lindex $scrninfo 1]
	set scrnwid	[lindex $scrninfo 0]
	switch -- $buttontype {
		yesno		{
				  set buttons "Yes No"
				  if [info exists message] {
					append buttons " Help"
				  }
				  incr width 2
				  set minwidth 25
				}
		okay		{
				  set buttons "Okay Cancel"
				  if [info exists message] {
					append buttons " help"
				  }
				}
		okayonly	{ set buttons "Okay" }
		default		{ set buttons "" }
	}
	if { $width < $minwidth } {
		set width $minwidth
	}
	if { $width > [expr $scrnwid - 2] } {
		set width [expr $scrnwid - 2]
	}
	if { $height > [expr $scrnht] } {
		set height $scrnht
	}
	set begx [expr ($scrnwid - $width)/2]
	set begy [expr ($scrnht - $height)/2]
	set mesgwin mesgwin[info level]
	curses newwin $mesgwin $height $width $begy $begx
	stdscr update
	$mesgwin configure -border line
	if [string length $title] {
		$mesgwin configure -title $title
	}

	if [string length $buttons] {
		$mesgwin text $text
		eval $mesgwin buttons $buttons
		do {
			$mesgwin update
			set sel [$mesgwin activate]
			if ![string compare $sel Help] {
				mesg $message okayonly
			}
		} while { ![string compare $sel Help] }
		if ![string compare $buttontype okayonly] {
			return 1
		}
		if { [string compare $sel [lindex $buttons 0]] == 0 } {
			return 1
		} else {
			return 0
		}
	} else {
		$mesgwin text $text
		$mesgwin update
	}
	return 1
}

proc save_state {} {
	global env XF86SetupDir TmpDir StateFileName PID
	set StateFileName "$TmpDir/state"
	check_tmpdirs
	set fd [open $StateFileName w]

	global NoCurses Confname ConfigFile UseConfigFile UseLoader StartServer
	global pc98

	puts $fd [list set NoCurses $NoCurses]
	puts $fd [list set Confname $Confname]
	puts $fd [list set ConfigFile $ConfigFile]
	puts $fd [list set UseConfigFile $UseConfigFile]
	puts $fd [list set UseLoader $UseLoader]
	puts $fd [list set StartServer $StartServer]
	puts $fd [list set XF86SetupDir $XF86SetupDir]
	puts $fd [list set TmpDir $TmpDir]
	puts $fd [list set PID $PID]
	global DeviceIDs MonitorIDs haveSelectedModes DefaultColorDepth
	puts $fd [list set DeviceIDs $DeviceIDs]
	puts $fd [list set MonitorIDs $MonitorIDs]
	puts $fd [list set DefaultColorDepth $DefaultColorDepth]
	puts $fd [list set haveSelectedModes $haveSelectedModes]
	if !$pc98 {
		set arrlist [list Files Module ServerFlags Keyboard Pointer \
			SelectedMonitorModes MonitorStdModes \
			Scrn_Accel Scrn_Mono Scrn_VGA2 Scrn_VGA16 Scrn_SVGA]
	} else {
		set arrlist [list Files Module ServerFlags Keyboard Pointer \
			SelectedMonitorModes MonitorStdModes \
			Scrn_Accel Scrn_VGA16 Scrn_SVGA]
	}
	foreach devid $DeviceIDs {
		lappend arrlist Device_$devid
	}
	foreach monid $MonitorIDs {
		lappend arrlist Monitor_$monid
	}
	foreach arrname $arrlist {
		global $arrname
		puts $fd [list array set $arrname [array get $arrname]]
	}
	close $fd
}

proc shutdown {args} {
	global env ExitStatus

	catch {destroy .}
	catch {server_running -close $env(DISPLAY)}
	if { [string length $args] } {
		set ExitStatus $args
	}
}

proc check_tmpdirs {} {
	global TmpDir XF86SetupDir messages

	file lstat $XF86SetupDir xfsdir
	file lstat $TmpDir tmpdir
	if {       [string compare $xfsdir(type) directory]
		|| [string compare $tmpdir(type) directory]
		|| $xfsdir(uid)  != [getuid]
		|| $tmpdir(uid)  != [getuid]
		|| $xfsdir(mode)&0077
		|| $tmpdir(mode)&0077 } \
	{
		mesg "$messages(setuplib.2)$TmpDir$messages(setuplib.3)"
		shutdown 1
		exit 1
	}
}

