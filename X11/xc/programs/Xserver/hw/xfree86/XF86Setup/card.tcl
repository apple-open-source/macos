# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/card.tcl,v 3.17 1999/04/05 07:12:58 dawes Exp $
#
# Copyright 1996 by Joseph V. Moss <joe@XFree86.Org>
#
# See the file "LICENSE" for information regarding redistribution terms,
# and for a DISCLAIMER OF ALL WARRANTIES.
#
# $XConsortium: card.tcl /main/5 1996/10/28 04:55:06 kaleb $

#
# Card configuration routines
#


proc Card_create_widgets { win } {
	global ServerList XF86Setup_library cardDevNum DeviceIDs
	global cardDetail cardReadmeWasSeen UseConfigFile cardDriverReadme
	global pc98_EGC messages

	set cardDriverReadme "NONE"
	set w [winpathprefix $win]
	set cardDevNum 0
        if !$pc98_EGC {
	    frame $w.card -width 640 -height 420 \
		    -relief ridge -borderwidth 5
	} else {
	    frame $w.card -width 640 -height 400 \
		    -relief ridge -borderwidth 5
	}
	frame $w.card.top
	pack  $w.card.top -side top -fill x -padx 5m
	if { [llength $DeviceIDs] > 1 } {
		label $w.card.title -text $messages(card.1) -anchor w
		pack  $w.card.title -side left -fill x -padx 5m -in $w.card.top
		combobox $w.card.cardselect -state disabled -bd 2
		pack  $w.card.cardselect -side left -in $w.card.top
		eval [list $w.card.cardselect linsert end] $DeviceIDs
		Card_cbox_setentry $w.card.cardselect [lindex $DeviceIDs 0]
		bind $w.card.cardselect.popup.list <ButtonRelease-1> \
			"+[list Card_cardselect $win]"
		bind $w.card.cardselect.popup.list <Return> \
			"+[list Card_cardselect $win]"
	} else {
		label $w.card.title -text $messages(card.2) -anchor w
		pack  $w.card.title -side left -fill x -padx 5m -in $w.card.top
	}

	frame $w.card.list
	scrollbar $w.card.list.sb -command [list $w.card.list.lb yview] \
		-repeatdelay 1200 -repeatinterval 800
	listbox   $w.card.list.lb -yscroll [list $w.card.list.sb set] \
		-setgrid true -height 20
	bind  $w.card.list.lb <Return> \
		[list Card_selected $win $w.card.list.lb]
	bind  $w.card.list.lb <ButtonRelease-1> \
		[list Card_selected $win $w.card.list.lb]
	eval  $w.card.list.lb insert 0 [xf86cards_getlist]
	pack  $w.card.list.lb -side left -fill both -expand yes
	pack  $w.card.list.sb -side left -fill y

	image create bitmap cardpic -foreground yellow -background black \
		-file $XF86Setup_library/pics/vidcard.xbm \
		-maskfile $XF86Setup_library/pics/vidcard.msk
	label $w.card.list.pic -image cardpic
	pack  $w.card.list.pic -side left -padx 3m -pady 3m


	frame $w.card.bot -borderwidth 5
	pack  $w.card.bot -side bottom -fill x
	label $w.card.bot.message
	pack  $w.card.bot.message -side top -fill x

	frame $w.card.buttons
	pack  $w.card.buttons -side bottom -fill x

	button $w.card.readme -text $messages(card.3) \
		-command [list Card_display_readme $win]
	pack  $w.card.readme -side left -expand yes \
		-in $w.card.buttons

	button $w.card.modebutton -text $messages(card.4) \
		-command [list Card_switchdetail $win]
	pack  $w.card.modebutton -side left -expand yes \
		-in $w.card.buttons

	frame $w.card.detail -bd 2 -relief sunken

	frame $w.card.server
	pack  $w.card.server -side top -fill x -in $w.card.detail
	label $w.card.server.title -text $messages(card.5)
	pack  $w.card.server.title -side left
	foreach serv $ServerList {
		set lcserv [string tolower $serv]
		radiobutton $w.card.server.$lcserv -indicatoron no \
			-text $serv -variable cardServer -value $serv \
			-command [list Card_set_cboxlists $win]
		pack $w.card.server.$lcserv -anchor w -side left \
			-expand yes -fill x
	}

	frame $w.card.detail.cboxen
	pack  $w.card.detail.cboxen -side top

	frame $w.card.chipset
	pack  $w.card.chipset -side left -expand yes -fill x \
		-in $w.card.detail.cboxen -padx 5m
	label $w.card.chipset.title -text $messages(card.7)
	combobox $w.card.chipset.cbox -state disabled -bd 2
	pack  $w.card.chipset.title $w.card.chipset.cbox

	frame $w.card.ramdac
	pack  $w.card.ramdac -side left -expand yes -fill x \
		-in $w.card.detail.cboxen -padx 5m
	label $w.card.ramdac.title -text $messages(card.8)
	combobox $w.card.ramdac.cbox -state disabled -bd 2
	pack  $w.card.ramdac.title $w.card.ramdac.cbox

	frame $w.card.clockchip
	pack  $w.card.clockchip -side left -expand yes -fill x \
		-in $w.card.detail.cboxen -padx 5m
	label $w.card.clockchip.title -text $messages(card.9)
	combobox $w.card.clockchip.cbox -state disabled -bd 2
	pack  $w.card.clockchip.title $w.card.clockchip.cbox

	set extr $w.card.extra
	frame $extr
	pack  $extr -side bottom -padx 5m \
		-fill x -expand yes -in $w.card.detail
	frame $extr.dacspeed
	if { $UseConfigFile } {
		pack  $extr.dacspeed -side left -fill x -expand yes
	}
	label $extr.dacspeed.title -text $messages(card.10)
	checkbutton $extr.dacspeed.probe -width 15 -text $messages(card.11) \
		-variable cardDacProbe -indicator off \
		-command [list Card_dacspeed $win] \
		-highlightthickness 0
	scale $extr.dacspeed.value -variable cardDacSpeed \
		-orient horizontal -from 60 -to 300 -resolution 5
	bind  $extr.dacspeed.value <ButtonPress> \
		"set cardDacProbe 0; [list Card_dacspeed $win]"
	pack  $extr.dacspeed.title -side top -fill x -expand yes
	pack  $extr.dacspeed.probe -side top -expand yes
	pack  $extr.dacspeed.value -side top -fill x -expand yes
	frame $extr.videoram
	pack  $extr.videoram -side left -fill x -expand yes
	label $extr.videoram.title -text $messages(card.12)
	pack  $extr.videoram.title -side top -fill x -expand yes
	radiobutton $extr.videoram.mprobed -indicator off -width 15 \
		-variable cardRamSize -value 0 -text $messages(card.13) \
		-highlightthickness 0
	pack  $extr.videoram.mprobed -side top -expand yes
	frame $extr.videoram.cols
	pack  $extr.videoram.cols -side top -fill x -expand yes
	frame $extr.videoram.col1
	frame $extr.videoram.col2
	pack  $extr.videoram.col1 $extr.videoram.col2 \
		-side left -fill x -expand yes \
		-in $extr.videoram.cols
	radiobutton $extr.videoram.m256k \
		-variable cardRamSize -value 256 -text $messages(card.14) \
		-highlightthickness 0
	radiobutton $extr.videoram.m512k \
		-variable cardRamSize -value 512 -text $messages(card.15) \
		-highlightthickness 0
	radiobutton $extr.videoram.m1m \
		-variable cardRamSize -value 1024 -text $messages(card.16) \
		-highlightthickness 0
	radiobutton $extr.videoram.m2m \
		-variable cardRamSize -value 2048 -text $messages(card.17) \
		-highlightthickness 0
	radiobutton $extr.videoram.m3m \
		-variable cardRamSize -value 3072 -text $messages(card.18) \
		-highlightthickness 0
	radiobutton $extr.videoram.m4m \
		-variable cardRamSize -value 4096 -text $messages(card.19) \
		-highlightthickness 0
	radiobutton $extr.videoram.m6m \
		-variable cardRamSize -value 6144 -text $messages(card.20) \
		-highlightthickness 0
	radiobutton $extr.videoram.m8m \
		-variable cardRamSize -value 8192 -text $messages(card.21) \
		-highlightthickness 0
	pack  $extr.videoram.m256k $extr.videoram.m512k \
	      $extr.videoram.m1m $extr.videoram.m2m \
		-side top -fill x -expand yes \
		-in $extr.videoram.col1
	pack  $extr.videoram.m3m $extr.videoram.m4m \
	      $extr.videoram.m6m $extr.videoram.m8m \
		-side top -fill x -expand yes \
		-in $extr.videoram.col2

	frame $w.card.options
	pack  $w.card.options -side bottom -fill x -in $w.card.detail \
		-pady 2m

	frame $w.card.options.list
	pack  $w.card.options.list -side top
	combobox $w.card.options.list.cbox -state disabled -width 80 -bd 2
	label $w.card.options.list.title -text $messages(card.22)
	$w.card.options.list.cbox.popup.list configure \
		-selectmode multiple
	pack  $w.card.options.list.title -side left
	pack  $w.card.options.list.cbox -fill x -expand yes -side left

	frame $w.card.options.text
	pack  $w.card.options.text -side top
	text  $w.card.options.text.text -yscroll [list $w.card.options.text.sb set] \
		-setgrid true -height 4 -background white
	scrollbar $w.card.options.text.sb -command \
		[list $w.card.options.text.text yview]
	label $w.card.options.text.title -text $messages(card.23)
	pack  $w.card.options.text.title -fill x -expand yes -side top
	pack  $w.card.options.text.text -fill x -expand yes -side left
	pack  $w.card.options.text.sb -side left -fill y

	$w.card.readme configure -state disabled
	for {set idx 0} {$idx < [llength $DeviceIDs]} {incr idx} {
		set cardReadmeWasSeen($idx)	0
	}
	if { $UseConfigFile } {
		set cardDetail		std
		Card_switchdetail $win
		#$w.card.modebutton configure -state disabled
	} else {
		set cardDetail		detail
		Card_switchdetail $win
	}
}

proc Card_activate { win } {
	set w [winpathprefix $win]
	Card_get_configvars $win
	pack $w.card -side top -fill both -expand yes
}

proc Card_deactivate { win } {
	set w [winpathprefix $win]
	pack forget $w.card
	Card_set_configvars $win
}

proc Card_dacspeed { win } {
	global cardDacSpeed cardDacProbe messages

	set w [winpathprefix $win]
	if { $cardDacProbe } {
		#$w.card.extra.dacspeed.probe configure -text "Probed: Yes"
		$w.card.extra.dacspeed.value configure \
			-foreground [option get $w.card background *] ;# -state disabled
	} else {
		#$w.card.extra.dacspeed.probe configure -text "Probed: No"
		$w.card.extra.dacspeed.value configure \
			-foreground [option get $w.card foreground *] -state normal
	}
}

proc Card_switchdetail { win } {
	global cardDetail cardDevNum messages

	set w [winpathprefix $win]
	if { $cardDetail == "std" } {
		set cardDetail detail
		$w.card.modebutton configure -text $messages(card.26)
		pack forget $w.card.list
		pack $w.card.detail -expand yes -side top -fill both
		$w.card.bot.message configure -text $messages(card.30)
	} else {
		set cardDetail std
		$w.card.modebutton configure -text $messages(card.27)
		pack forget $w.card.detail
		pack $w.card.list   -expand yes -side top -fill both
		$w.card.bot.message configure -text $messages(card.31)
	}
}

proc Card_cbox_setentry { cb text } {
	$cb econfig -state normal
	$cb edelete 0 end
	if [string length $text] {
		$cb einsert 0 $text
	}
	$cb econfig -state disabled
	set cblist [$cb lget 0 end]
	if { [string match *.options.cbox $cb] } {
		$cb lselection clear 0 end
		foreach option [split $text ,] {
			set idx [lsearch $cblist $option]
			if { $idx != -1 } {
				$cb see $idx
				$cb lselection set $idx
				$cb activate $idx
			}
		}
	} else {
		set idx [lsearch $cblist $text]
		if { $idx != -1 } {
			$cb see $idx
			$cb lselection clear 0 end
			$cb lselection set $idx
			$cb activate $idx
		}
	}
}

proc Card_selected { win lbox } {
	global cardServer cardReadmeWasSeen cardDevNum cardDriverReadme
	global pc98 Module messages

	set w [winpathprefix $win]
	if { ![string length [$lbox curselection]] } return
	set cardentry	[$lbox get [$lbox curselection]]
	set carddata	[xf86cards_getentry $cardentry]
	set cardDriverReadme ""
	$w.card.title configure -text "$messages(card.28)$cardentry"
	$w.card.options.text.text delete 0.0 end
	if { [lsearch [lindex $carddata 7] UNSUPPORTED] == -1 } {
	    #Card_cbox_setentry $w.card.chipset.cbox  [lindex $carddata 1]
	    set cardDriverReadmeRaw		      [lindex $carddata 1]
	    switch -glob $cardDriverReadmeRaw {
		    S3\ ViRGE	{ set cardDriverReadme "s3v" }
		    ET3*	{ set cardDriverReadme "et3000" }
		    ET4*	{ set cardDriverReadme "et4000" }
		    ET6*	{ set cardDriverReadme "et4000" }
		    CL*		{ set cardDriverReadme "cirrus" }
		    ARK*	{ set cardDriverReadme "ark" }
		    ct*		{ set cardDriverReadme "chips" }
		    mga*	{ set cardDriverReadme "mga" }
		    MGA*	{ set cardDriverReadme "mga" }
		    TVGA*	{ set cardDriverReadme "tvga8900" }
		    TGUI*	{ set cardDriverReadme "tvga8900" }
		    SIS*	{ set cardDriverReadme "sis" }
		    nv1*	{ set cardDriverReadme "nv" }
		    Oak*	{ set cardDriverReadme "oak" }
		    WD*		{ set cardDriverReadme "pvga1" }
		    ALG*	{ set cardDriverReadme "NONE" }
		    AP*		{ set cardDriverReadme "NONE" }
		    Avance*	{ set cardDriverReadme "NONE" }
		    Alliance*	{ set cardDriverReadme "NONE" }
	    }
	    set cardServer			      [lindex $carddata 2]
	    Card_cbox_setentry $w.card.ramdac.cbox    [lindex $carddata 3]
	    Card_cbox_setentry $w.card.clockchip.cbox [lindex $carddata 4]
	    $w.card.options.text.text insert 0.0      [lindex $carddata 6]
	    if { $cardReadmeWasSeen($cardDevNum) } {
	        $w.card.bot.message configure -text $messages(card.32)
	    } else {
	        $w.card.bot.message configure -text $messages(card.33)
	    }
	} else {
	    set cardServer			      VGA16
	    Card_cbox_setentry $w.card.chipset.cbox   generic
	    Card_cbox_setentry $w.card.ramdac.cbox    ""
	    Card_cbox_setentry $w.card.clockchip.cbox ""
	    $w.card.bot.message configure -text $messages(card.34)
	}
	Card_set_cboxlists $win cardselected
	if $pc98 {
	    switch $cardServer {
		EGC	{ set Module(Load) egc }
		PEGC	{ set Module(Load) pegc }
		GANBWAP	{ set Module(Load) ganbwap }
		NKVNEC	{ set Module(Load) nkvnec }
		WABS	{ set Module(Load) wabs }
		WABEP	{ set Module(Load) wabep }
		WSNA	{ set Module(Load) wsna }
		TGUI	{ set Module(Load) trident }
		MGA	{ set Module(Load) mga }
		NECS3	{ set Module(Load) s3nec }
		PWSKB	{ set Module(Load) s3pwskb }
		PWLB	{ set Module(Load) s3pwlb }
		GA968	{ set Module(Load) s3ga968 }
	    }
	}
}

proc Card_set_cboxlists { win args } {
	global CardChipSets CardRamDacs CardClockChips cardServer
	global CardReadmes cardReadmeWasSeen CardOptions Xwinhome
	global pc98

	set w [winpathprefix $win]
	$w.card.bot.message configure -text [make_message_card $args]
	if { [llength $CardReadmes($cardServer)] > 0 } {
		$w.card.readme configure -state normal
	} else {
		$w.card.readme configure -state disabled
	}
	$w.card.chipset.cbox ldelete 0 end
	if [llength $CardChipSets($cardServer)] {
		$w.card.chipset.cbox.button configure -state normal
		$w.card.chipset.cbox linsert end "<Probed>"
		eval [list $w.card.chipset.cbox linsert end] \
			$CardChipSets($cardServer)
	} else {
		$w.card.chipset.cbox.button configure -state disabled
	
	}
	set chipset [$w.card.chipset.cbox eget]
	if { [string length $chipset] && [lsearch \
			$CardChipSets($cardServer) $chipset] < 0} {
		Card_cbox_setentry $w.card.chipset.cbox ""
	}

	$w.card.ramdac.cbox ldelete 0 end
	if [llength $CardRamDacs($cardServer)] {
		$w.card.ramdac.cbox.button configure -state normal
		$w.card.ramdac.cbox linsert end "<Probed>"
		eval [list $w.card.ramdac.cbox linsert end] \
			$CardRamDacs($cardServer)
	} else {
		$w.card.ramdac.cbox.button configure -state disabled
	}
	set ramdac [$w.card.ramdac.cbox eget]
	if { [string length $ramdac] && [lsearch \
			$CardRamDacs($cardServer) $ramdac] < 0} {
		Card_cbox_setentry $w.card.ramdac.cbox ""
	}


	$w.card.clockchip.cbox ldelete 0 end
	if [llength $CardClockChips($cardServer)] {
		$w.card.clockchip.cbox.button configure -state normal
		$w.card.clockchip.cbox linsert end "<Probed>"
		eval [list $w.card.clockchip.cbox linsert end] \
			$CardClockChips($cardServer)
	} else {
		$w.card.clockchip.cbox.button configure -state disabled
	}
	set clockchip [$w.card.clockchip.cbox eget]
	if { [string length $clockchip] && [lsearch \
			$CardClockChips($cardServer) $clockchip] < 0} {
		Card_cbox_setentry $w.card.clockchip.cbox ""
	}

	$w.card.options.list.cbox ldelete 0 end
	if [llength $CardOptions($cardServer)] {
		$w.card.options.list.cbox.button configure -state normal
		eval [list $w.card.options.list.cbox linsert end] \
			$CardOptions($cardServer)
	} else {
		$w.card.options.list.cbox.button configure -state disabled
	}
	set options ""
	foreach option [split [$w.card.options.list.cbox eget] ,] {
		if { [string length $option] && [lsearch \
			$CardOptions($cardServer) $option] != -1} {
		    lappend options $option
		}
	}
	Card_cbox_setentry $w.card.options.list.cbox [join $options ,]
}

proc Card_display_readme { win } {
	global cardServer CardReadmes cardReadmeWasSeen cardDriverReadme
	global cardDevNum Xwinhome messages pc98_EGC

	set w [winpathprefix $win]
	catch {destroy .cardreadme}
        toplevel .cardreadme -bd 5 -relief ridge
	wm title .cardreadme "Chipset Specific README"
	wm geometry .cardreadme +30+30
	frame .cardreadme.file
        text .cardreadme.file.text -setgrid true \
		-xscroll ".cardreadme.horz.hsb set" \
		-yscroll ".cardreadme.file.vsb set"
	if $pc98_EGC {
		.cardreadme.file.text configure -height 20
	}
	if { ![string compare $cardServer "SVGA"] &&
	     [string length $cardDriverReadme] } {
		set readmeindex $cardServer-$cardDriverReadme
	} else {
		set readmeindex $cardServer
	}
	foreach file $CardReadmes($readmeindex) {
		set fd [open $Xwinhome/lib/X11/doc/$file r]
		.cardreadme.file.text insert end [read $fd]
		close $fd
	}
        .cardreadme.file.text configure -state disabled
	frame .cardreadme.horz
	scrollbar .cardreadme.horz.hsb -orient horizontal \
		-command ".cardreadme.file.text xview" \
		-repeatdelay 1200 -repeatinterval 800
	scrollbar .cardreadme.file.vsb \
		-command ".cardreadme.file.text yview" \
		-repeatdelay 1200 -repeatinterval 800
	button .cardreadme.ok -text $messages(card.29) \
		-command "destroy .cardreadme"
	focus .cardreadme.ok
        pack .cardreadme.file -side top -fill both
        pack .cardreadme.file.text -side left
        pack .cardreadme.file.vsb -side left -fill y
	#update idletasks
	#.cardreadme.horz configure -width [winfo width .cardreadme.file.text] \
		-height [winfo width .cardreadme.file.vsb]
        #pack propagate .cardreadme.horz 0
        #pack .cardreadme.horz -side top -anchor w
        #pack .cardreadme.horz.hsb -fill both
        pack .cardreadme.ok -side bottom
	set cardReadmeWasSeen($cardDevNum) 1
}

proc Card_cardselect { win } {
	global cardDevNum

	set w [winpathprefix $win]
	if { ![string length [$w.card.cardselect curselection]] } return
	Card_set_configvars $win
	set cardDevNum [$w.card.cardselect curselection]
	Card_get_configvars $win
}

proc Card_set_configvars { win } {
	global DeviceIDs cardServer ServerList cardDevNum
	global AccelServerList CardChipSets CardRamDacs CardClockChips
	global cardDacSpeed cardDacProbe cardRamSize UseConfigFile

	set w [winpathprefix $win]
	set devid [lindex $DeviceIDs $cardDevNum]
	global Device_$devid messages

	set Device_${devid}(Server)	$cardServer
	set Device_${devid}(Chipset)	[$w.card.chipset.cbox eget]
	set Device_${devid}(Ramdac)	[$w.card.ramdac.cbox eget]
	set Device_${devid}(ClockChip)	[$w.card.clockchip.cbox eget]
	set Device_${devid}(ExtraLines)	[$w.card.options.text.text get 0.0 end]
	set Device_${devid}(Options)	[split [$w.card.options.list.cbox eget] ,]
	if {[llength $DeviceIDs] == 1} {
		set Device_${devid}(BoardName)	[string range \
			[$w.card.title cget -text] \
			[string length $messages(card.28)] end]
	}
	if { $cardRamSize } {
	    set Device_${devid}(VideoRam)	$cardRamSize
	} else {
	    set Device_${devid}(VideoRam)	""
	}
	if { $UseConfigFile } {
	    if { $cardDacProbe } {
	        set Device_${devid}(DacSpeed)	""
	    } else {
	        set Device_${devid}(DacSpeed)	[expr $cardDacSpeed*1000]
	    }
	}
}

proc Card_get_configvars { win } {
	global DeviceIDs cardServer ServerList cardDevNum
	global AccelServerList CardChipSets CardRamDacs CardClockChips
	global cardDacSpeed cardDacProbe cardRamSize UseConfigFile

	set w [winpathprefix $win]
	set devid [lindex $DeviceIDs $cardDevNum]
	global Device_$devid

	set cardServer		[set Device_${devid}(Server)]
	Card_cbox_setentry $w.card.chipset.cbox [set Device_${devid}(Chipset)]
	Card_cbox_setentry $w.card.ramdac.cbox [set Device_${devid}(Ramdac)]
	Card_cbox_setentry $w.card.clockchip.cbox [set Device_${devid}(ClockChip)]
	$w.card.options.text.text delete 0.0 end
	$w.card.options.text.text insert 0.0 [set Device_${devid}(ExtraLines)]
	Card_cbox_setentry $w.card.options.list.cbox \
		[join [set Device_${devid}(Options)] ,]
	Card_set_cboxlists $win
	if { $UseConfigFile } {
	    set ram [set Device_${devid}(VideoRam)]
	    if { [string length $ram] > 0 } {
	        set cardRamSize	$ram
	    } else {
	        set cardRamSize	0
	    }
	    set speed [set Device_${devid}(DacSpeed)]
	    if { [string length $speed] > 0 } {
	        set cardDacSpeed	[expr int($speed/1000)]
	        set cardDacProbe	0
	    } else {
	        set cardDacSpeed	60
	        set cardDacProbe	1
	    }
	    Card_dacspeed $win
	} else {
	    set cardRamSize 0
	}
}

