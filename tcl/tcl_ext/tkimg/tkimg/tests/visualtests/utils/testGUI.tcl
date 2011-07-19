# Set this variable to 0, if Tk should not be used for testing.
if { [info exists env(UI_TK)] && $env(UI_TK) == 0 } {
    set ui_enable_tk 0
} else {
    set ui_enable_tk 1
}

proc bmpFirst {} {
    return {
    #define first_width 16
    #define first_height 16
    static unsigned char first_bits[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x04, 0x1c, 0x06,
	0x1c, 0x07, 0x9c, 0x3f, 0xdc, 0x3f, 0x9c, 0x3f, 0x1c, 0x07, 0x1c, 0x06,
	0x1c, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    }
}

proc bmpLast {} {
    return {
    #define last_width 16
    #define last_height 16
    static unsigned char last_bits[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x38, 0x60, 0x38,
	0xe0, 0x38, 0xfc, 0x39, 0xfc, 0x3b, 0xfc, 0x39, 0xe0, 0x38, 0x60, 0x38,
	0x20, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    }
}

proc bmpLeft {} {
    return {
    #define left_width 16
    #define left_height 16
    static unsigned char left_bits[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x01,
	0xc0, 0x01, 0xe0, 0x0f, 0xf0, 0x0f, 0xe0, 0x0f, 0xc0, 0x01, 0x80, 0x01,
	0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    }
}

proc bmpRight {} {
    return {
    #define right_width 16
    #define right_height 16
    static unsigned char right_bits[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x80, 0x01,
	0x80, 0x03, 0xf0, 0x07, 0xf0, 0x0f, 0xf0, 0x07, 0x80, 0x03, 0x80, 0x01,
	0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    }
}

proc bmpPlay {} {
    return {
    #define play_width 16
    #define play_height 16
    static unsigned char play_bits[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0xe0, 0x00,
	0xe0, 0x01, 0xe0, 0x03, 0xe0, 0x07, 0xe0, 0x03, 0xe0, 0x01, 0xe0, 0x00,
	0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    }
}

proc bmpHalt {} {
    return {
    #define halt_width 16
    #define halt_height 16
    static unsigned char halt_bits[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30, 0x0c,
	0x60, 0x06, 0xc0, 0x03, 0x80, 0x01, 0xc0, 0x03, 0x60, 0x06, 0x30, 0x0c,
	0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    }
} 

proc ui_initToolhelp { w { bgColor yellow } { fgColor black } } {
    global ui_helpWidget

    # Create Toolbar help window with a simple label in it.
    if { [winfo exists $w] } {
        destroy $w
    }
    toplevel $w
    set ui_helpWidget $w
    label $w.l -text "??" -bg $bgColor -fg $fgColor -relief ridge
    pack $w.l
    wm overrideredirect $w true
    if {[string equal [tk windowingsystem] aqua]}  {
        ::tk::unsupported::MacWindowStyle style $w help none
    }
    wm geometry $w [format "+%d+%d" -100 -100]
}

proc ui_showToolhelp { x y str } {
    global ui_helpWidget

    $ui_helpWidget.l configure -text $str
    raise $ui_helpWidget
    wm geometry $ui_helpWidget [format "+%d+%d" $x [expr $y +10]]
}

proc ui_hideToolhelp {} {
    global ui_helpWidget

    wm geometry $ui_helpWidget [format "+%d+%d" -100 -100]
}

proc ui_button { btnName bmpData cmd helpStr } {
    set imgData [image create bitmap -data $bmpData]
    eval button $btnName -image $imgData -command [list $cmd] -relief flat
    bind $btnName <Enter>  "ui_showToolhelp %X %Y [list $helpStr]"
    bind $btnName <Leave>  { ui_hideToolhelp }
    bind $btnName <Button> { ui_hideToolhelp }
}

proc ui_init {title {winPos "+0+0"} } {
    global ui_enable_tk ui_curImgNo ui_noImgs ui_top

    catch {wm withdraw .}
    if { $ui_enable_tk } {
	set ui_top .testWindow
	ui_initToolhelp .testToolhelp
        toplevel $ui_top
	wm title $ui_top $title
	wm geometry $ui_top $winPos
        frame $ui_top.imgfr -bg lightgrey
        frame $ui_top.menufr -relief raised -bg lightgrey

	label $ui_top.imgfr.img -bg white
	text $ui_top.imgfr.txt -height 2 -width 60 -state disabled
	pack $ui_top.imgfr.txt -side top
	pack $ui_top.imgfr.img -side top

    	ui_button $ui_top.menufr.quit [bmpHalt] ui_exit "Quit test (Esc)"
	pack $ui_top.menufr.quit -in $ui_top.menufr -side left
    	pack $ui_top.menufr $ui_top.imgfr -side top -pady 2 -anchor w
	bind $ui_top <Key-Escape> ui_exit
	wm protocol $ui_top WM_DELETE_WINDOW ui_exit

	P "Visual: [winfo screenvisual $ui_top]"
	P "Depth:  [winfo depth $ui_top]"
    }
    set ui_curImgNo 0
    set ui_noImgs   0
}

proc showimg { imgNo } {
    global ui_enable_tk ui_strings ui_top ui_photos

    if { $ui_enable_tk } {
        $ui_top.imgfr.img config -image $ui_photos($imgNo)

	$ui_top.imgfr.txt configure -state normal
	$ui_top.imgfr.txt delete 1.0 end
	$ui_top.imgfr.txt insert end $ui_strings($imgNo)
	$ui_top.imgfr.txt configure -state disabled
	update
    }
}

proc ui_addimg { poImg str { chanMap {} } } {
    global ui_enable_tk ui_curImgNo ui_noImgs ui_strings ui_images ui_photos
 
    set ui_strings($ui_curImgNo) $str
    set ui_images($ui_curImgNo) $poImg
    if { $ui_enable_tk } {
        set ui_photos($ui_curImgNo) [image create photo]
	$poImg img_photo $ui_photos($ui_curImgNo) $chanMap
   	showimg $ui_curImgNo
    }
    incr ui_curImgNo
    set ui_noImgs $ui_curImgNo
}

proc ui_addphoto { phImg str } {
    global ui_enable_tk ui_curImgNo ui_noImgs ui_strings ui_images ui_photos
 
    set ui_strings($ui_curImgNo) $str
    set ui_images($ui_curImgNo)  "none"
    if { $ui_enable_tk } {
	set ui_photos($ui_curImgNo) $phImg
   	showimg $ui_curImgNo
    }
    incr ui_curImgNo
    set ui_noImgs $ui_curImgNo
}

proc show_first {} {
    global ui_curImgNo ui_noImgs

    set ui_curImgNo 0
    showimg $ui_curImgNo
}

proc show_last {} {
    global ui_curImgNo ui_noImgs

    set ui_curImgNo [expr ($ui_noImgs -1)]
    showimg $ui_curImgNo
}

proc show_play {} {
    global ui_curImgNo ui_noImgs

    while { $ui_curImgNo < [expr ($ui_noImgs -1)] } {
    	incr ui_curImgNo
    	showimg $ui_curImgNo
    }
}

proc show_prev {} {
    global ui_curImgNo

    if { $ui_curImgNo > 0 } {
	incr ui_curImgNo -1
    	showimg $ui_curImgNo
    }
}

proc show_next {} {
    global ui_curImgNo ui_noImgs

    if { $ui_curImgNo < [expr ($ui_noImgs -1)] } {
    	incr ui_curImgNo 1
    	showimg $ui_curImgNo
    }
}

proc ui_show {} {
    global ui_enable_tk ui_curImgNo ui_noImgs ui_strings ui_top

    PrintMachineInfo

    set ui_noImgs $ui_curImgNo
    incr ui_curImgNo -1
    if { $ui_enable_tk } {
	if { $ui_noImgs > 0 } {
	    set fr $ui_top.menufr
    	    ui_button $fr.first [bmpFirst] show_first "Show first image"
    	    ui_button $fr.prev  [bmpLeft]  show_prev  "Show previous image (<-)"
    	    ui_button $fr.next  [bmpRight] show_next  "Show next image (->)"
    	    ui_button $fr.last  [bmpLast]  show_last  "Show last image"
    	    ui_button $fr.play  [bmpPlay]  show_play  "Play image sequence (p)"
	    pack $fr.first $fr.prev $fr.next $fr.last \
	         -in $fr -side left -padx 0
	    pack $fr.play -in $fr -side left -padx 0
    
	    bind $ui_top <Key-Right>  show_next
	    bind $ui_top <Key-Left>   show_prev
	    bind $ui_top <Key-p>      show_play
	}
    } else {
	ui_exit
    }
}

proc ui_delete {} {
    global ui_enable_tk ui_noImgs ui_strings ui_images ui_photos ui_top

	for { set i 0 } { $i < $ui_noImgs } { incr i } {
	if { $ui_enable_tk } {
	    image delete $ui_photos($i)
	}
	    if { [info commands $ui_images($i)] != {} } {
	        deleteimg $ui_images($i)
	    }
	    set ui_strings($i) {}
	}
    if { $ui_enable_tk } {
	destroy $ui_top.imgfr
	destroy $ui_top.menufr
    }
}

proc ui_exit {} {
    ui_delete
    if { [info commands memcheck] != {} } {
	memcheck
    }
    exit
}
