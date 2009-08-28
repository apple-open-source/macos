# as_style.tcl --
#
#	This file implements package style::as.
#
# Copyright (c) 2003 ActiveState Corporation, a division of Sophos
#
# Basic use:
#
# style::as::init ?which?
# style::as::reset ?which?
# style::as::enable ?what ?args??
#	ie: enable control-mousewheel local|global
#

package require Tk

namespace eval style::as {
    variable version 1.4
    variable highlightbg "#316AC5" ; # SystemHighlight
    variable highlightfg "white"   ; # SystemHighlightText
    variable bg          "white"   ; # SystemWindow
    variable fg          "black"   ; # SystemWindowText
    if {[string equal $::tcl_platform(platform) "windows"]} {
	# Use the system colors on Windows, as they can adapt
	# to the user's personal color scheme
	set highlightbg "SystemHighlight"
	set highlightfg "SystemHighlightText"
	set bg          "SystemWindow"
	set fg          "SystemWindowText"
    }

    # This may need to be adjusted for some window managers that are
    # more aggressive with their own Xdefaults (like KDE and CDE)
    variable prio "widgetDefault"

    # assume MouseWheel binding is the same across widget classes
    variable mw
    set mw(classes) [list Text Listbox Table TreeCtrl]
    if {![info exists mw(binding)]} {
	# do this only once, in case of re-source-ing
	set mw(binding) [bind Text <MouseWheel>]
	set mw(s-binding) [bind Text <Shift-MouseWheel>]
	if {[tk windowingsystem] eq "x11"} {
	    set mw(binding4) [bind Text <4>]
	    set mw(binding5) [bind Text <5>]
	}
    }
    if {[tk windowingsystem] eq "aqua"} {
	set mw(ctrl) "Command"
    } else {
	set mw(ctrl) "Control"
    }
}; # end of namespace style::as

proc style::as::init {args} {
    package require Tk
    variable prio

    if {[llength $args]} {
	set arg [lindex $args 0]
	set len [string length $arg]
	if {$len > 2 && [string equal -len $len $arg "-priority"]} {
	    set prio [lindex $args 1]
	    set args [lrange $args 2 end]
	}
    }
    if {[llength $args]} {
	foreach what $args {
	    style::as::init_$what
	}
    } else {
	foreach cmd [info procs init_*] {
	    $cmd
	}
    }

    if {$::tcl_platform(os) eq "Windows CE"} {
	# WinCE is for small screens, with 240x320 (QVGA) the most common.
	# Adapt the defaults to that size.
	option add *font			{Tahoma 7} $prio
	option add *Button.borderWidth		1 $prio
	option add *Entry.borderWidth		1 $prio
	option add *Listbox.borderWidth		1 $prio
	option add *Spinbox.borderWidth		1 $prio
	option add *Text.borderWidth		1 $prio
	option add *Scrollbar.width		11 $prio
	option add *padY			0 $prio
    }
}
proc style::as::reset {args} {
    if {[llength $args]} {
	foreach what $args {
	    style::as::reset_$what
	}
    } else {
	foreach cmd [info commands style::as::reset_*] {
	    $cmd
	}
    }
}
proc style::as::enable {what args} {
    variable mw
    switch -exact $what {
	mousewheel { init_mousewheel }
	control-mousewheel {
	    set type [lindex $args 0]; # should be local or global
	    bind all <Control-MouseWheel> \
		[list ::style::as::CtrlMouseWheel %W %X %Y %D $type]
	    bind all <$mw(ctrl)-plus> \
		[list ::style::as::CtrlMouseWheel %W %X %Y 120 $type]
	    bind all <$mw(ctrl)-minus> \
		[list ::style::as::CtrlMouseWheel %W %X %Y -120 $type]
	    if {[tk windowingsystem] eq "x11"} {
		bind all <Control-ButtonPress-4> \
		    [list ::style::as::CtrlMouseWheel %W %X %Y 120 $type]
		bind all <Control-ButtonPress-5> \
		    [list ::style::as::CtrlMouseWheel %W %X %Y -120 $type]
	    }
	}
	default {
	    return -code error "unknown option \"$what\""
	}
    }
}
proc style::as::disable {what args} {
    variable mw
    switch -exact $what {
	mousewheel { reset_mousewheel }
	control-mousewheel {
	    bind all <Control-MouseWheel> {}
	    bind all <$mw(ctrl)-plus> {}
	    bind all <$mw(ctrl)-minus> {}
	    if {[tk windowingsystem] eq "x11"} {
		bind all <Control-ButtonPress-4> {}
		bind all <Control-ButtonPress-5> {}
	    }
	}
	default {
	    return -code error "unknown option \"$what\""
	}
    }
}

## Fonts
##
proc style::as::init_fonts {args} {
    if {[lsearch -exact [font names] ASfont] == -1} {
	switch -exact [tk windowingsystem] {
	    "x11" {
		set size	-12
		set family	Helvetica
		set fsize	-12
		set ffamily	Courier
	    }
	    "win32" {
		set size	8
		set family	Tahoma
		set fsize	9
		set ffamily	Courier
	    }
	    "aqua" - "macintosh" {
		set size	11
		set family	"Lucida Grande"
		set fsize	11
		set ffamily	Courier
	    }
	}
	font create ASfont      -size $size -family $family
	font create ASfontBold  -size $size -family $family -weight bold
	font create ASfontFixed -size $fsize -family $ffamily
	font create ASfontFixedBold -size $fsize -family $ffamily -weight bold
	for {set i -2} {$i <= 4} {incr i} {
	    set isize  [expr {$size + ($i * (($size > 0) ? 1 : -1))}]
	    set ifsize [expr {$fsize + ($i * (($fsize > 0) ? 1 : -1))}]
	    font create ASfont$i      -size $isize -family $family
	    font create ASfontBold$i  -size $isize -family $family -weight bold
	    font create ASfontFixed$i -size $ifsize -family $ffamily
	    font create ASfontFixedBold$i \
		-size $fsize -family $ffamily -weight bold
	}
    }

    if {1 || [tk windowingsystem] eq "x11"} {
	variable prio

	option add *Text.font		ASfontFixed $prio
	option add *Button.font		ASfont $prio
	option add *Canvas.font		ASfont $prio
	option add *Checkbutton.font	ASfont $prio
	option add *Entry.font		ASfont $prio
	option add *Label.font		ASfont $prio
	option add *Labelframe.font	ASfont $prio
	option add *Listbox.font	ASfont $prio
	if {[tk windowingsystem] ne "aqua"} {
	    option add *Menu.font	ASfont $prio
	}
	option add *Menubutton.font	ASfont $prio
	option add *Message.font	ASfont $prio
	option add *Radiobutton.font	ASfont $prio
	option add *Spinbox.font	ASfont $prio

	option add *Table.font		ASfont $prio
	option add *TreeCtrl*font	ASfont $prio
    }
}

proc style::as::reset_fonts {args} {
}

proc style::as::CtrlMouseWheel {W X Y D {what local}} {
    set w [winfo containing $X $Y]
    if {[winfo exists $w]} {
	set top [winfo toplevel $w]
	while {[catch {$w cget -font} font]
	       || ![string match "ASfont*" $font]} {
	    if {$w eq $top} { return }
	    set w [winfo parent $w]
	}
	if {$what eq "local"} {
	    # get current font size (0 by default) and adjust the current
	    # widget's font to the next sized preconfigured font
	    set cnt [regexp -nocase -- {([a-z]+)(\-?\d)?} $font -> name size]
	    if {$size eq ""} {
		set size [expr {($D > 0) ? 1 : -1}]
	    } else {
		set size [expr {$size + (($D > 0) ? 1 : -1)}]
	    }
	    set font $name$size
	    if {[lsearch -exact [font names] $font] != -1} {
		catch {$w configure -font $font}
	    }
	} else {
	    # readjust all the font sizes based on the current one
	    set size [font configure ASfont -size]
	    # handle negative font sizes (by pixel instead of point)
	    set neg [expr {($size < 0) ? -1 : 1}]
	    incr size [expr {$neg * (($D > 0) ? 1 : -1)}]
	    # but we do have limits on how small/large things can get
	    if {abs($size) < 6 || abs($size) > 18} { return }
	    font configure ASfont      -size $size
	    font configure ASfontBold  -size $size
	    font configure ASfontFixed -size [expr {$size+(1*$neg)}]
	    # force reconfigure of this widget with the same font in
	    # case it doesn't have a WorldChanged function
	    catch {$w configure -font $font}
	    if {0} {
		# we shouldn't need this if the user isn't improperly
		# switching between global/local ctrl-mswhl modes
		for {set i -2} {$i <= 4} {incr i} {
		    font configure ASfont$i      \
			-size [expr {$size+($i*$neg)}] -family $family
		    font configure ASfontBold$i  \
			-size [expr {$size+($i*$neg)}] -family $family \
			-weight bold
		    font configure ASfontFixed$i \
			-size [expr {$size+((1+$i)*$neg)}] -family Courier
		}
	    }
	}
    }
}

## Misc
##
proc style::as::init_misc {args} {
    variable prio
    variable highlightbg
    variable highlightfg
    variable bg
    variable fg
    option add *ScrolledWindow.ipad		0 $prio

    # Various other common widgets from popular widget sets
    foreach class {HList Tree Tree.c TixHList TixTree} {
	option add *$class.borderWidth		1 $prio
	option add *$class.background		$bg $prio
	option add *$class.foreground		$fg $prio
	option add *$class.selectBorderWidth	0 $prio
	option add *$class.selectForeground	$highlightfg $prio
	option add *$class.selectBackground	$highlightbg $prio
    }
    if {[tk windowingsystem] ne "x11"} {
	option add *TreeCtrl.useTheme 1
    }
}

## Listbox
##
proc style::as::init_listbox {args} {
    variable prio
    if {[tk windowingsystem] eq "x11"} {
	variable highlightbg
	variable highlightfg
	variable bg
	variable fg
	option add *Listbox.background		$bg $prio
	option add *Listbox.foreground		$fg $prio
	option add *Listbox.selectBorderWidth	0 $prio
	option add *Listbox.selectForeground	$highlightfg $prio
	option add *Listbox.selectBackground	$highlightbg $prio
    }
    option add *Listbox.activeStyle		dotbox $prio
}

## Button
##
proc style::as::init_button {args} {
    variable prio
    if {[tk windowingsystem] eq "x11"} {
	option add *Button.padX			1 $prio
	option add *Button.padY			2 $prio
    }
    option add *Button.highlightThickness	1 $prio
}

## Entry
##
proc style::as::init_entry {args} {
    if {[tk windowingsystem] eq "x11"} {
	variable prio
	variable highlightbg
	variable highlightfg
	variable bg
	variable fg
	option add *Entry.background		$bg $prio
	option add *Entry.foreground		$fg $prio
	option add *Entry.selectBorderWidth	0 $prio
	option add *Entry.selectForeground	$highlightfg $prio
	option add *Entry.selectBackground	$highlightbg $prio
    }
}

## Spinbox
##
proc style::as::init_spinbox {args} {
    if {[tk windowingsystem] eq "x11"} {
	variable prio
	variable highlightbg
	variable highlightfg
	variable bg
	variable fg
	option add *Spinbox.background		$bg $prio
	option add *Spinbox.foreground		$fg $prio
	option add *Spinbox.selectBorderWidth	0 $prio
	option add *Spinbox.selectForeground	$highlightfg $prio
	option add *Spinbox.selectBackground	$highlightbg $prio
    }
}

## Text
##
proc style::as::init_text {args} {
    if {[tk windowingsystem] eq "x11"} {
	variable prio
	variable highlightbg
	variable highlightfg
	variable bg
	variable fg
	option add *Text.background		$bg $prio
	option add *Text.foreground		$fg $prio
	option add *Text.selectBorderWidth	0 $prio
	option add *Text.selectForeground	$highlightfg $prio
	option add *Text.selectBackground	$highlightbg $prio
    }
}

## Menu
##
proc style::as::init_menu {args} {
    if {[tk windowingsystem] eq "x11"} {
	variable prio
	variable highlightbg
	variable highlightfg
	option add *Menu.activeBackground	$highlightbg $prio
	option add *Menu.activeForeground	$highlightfg $prio
	option add *Menu.activeBorderWidth	1 $prio
	option add *Menu.borderWidth		1 $prio
    }
}

## Menubutton
##
proc style::as::init_menubutton {args} {
    variable prio
    variable highlightbg
    variable highlightfg
    option add *Menubutton.activeBackground	$highlightbg $prio
    option add *Menubutton.activeForeground	$highlightfg $prio
    option add *Menubutton.activeBorderWidth	1 $prio
    option add *Menubutton.borderWidth		1 $prio
    option add *Menubutton.highlightThickness	0 $prio
    option add *Menubutton*padX			4 $prio
    option add *Menubutton*padY			3 $prio
}

## Scrollbar
##
proc style::as::init_scrollbar {args} {
    variable prio
    if {[tk windowingsystem] eq "x11"} {
	option add *Scrollbar.width		12 $prio
	option add *Scrollbar.troughColor	"#bdb6ad" $prio
    }
    option add *Scrollbar.borderWidth		1 $prio
    option add *Scrollbar.highlightThickness	0 $prio
}

## PanedWindow
##
proc style::as::init_panedwindow {args} {
    variable prio
    option add *Panedwindow.borderWidth		0 $prio
    option add *Panedwindow.sashWidth		3 $prio
    option add *Panedwindow.showHandle		0 $prio
    option add *Panedwindow.sashPad		0 $prio
    option add *Panedwindow.sashRelief		flat $prio
    option add *Panedwindow.relief		flat $prio
}

## MouseWheel
##
proc style::as::MouseWheel {wFired X Y D {shifted 0}} {
    # Set event to check based on call
    set evt "<[expr {$shifted?{Shift-}:{}}]MouseWheel>"
    # do not double-fire in case the class already has a binding
    if {[bind [winfo class $wFired] $evt] ne ""} { return }
    # obtain the window the mouse is over
    set w [winfo containing $X $Y]
    # if we are outside the app, try and scroll the focus widget
    if {![winfo exists $w]} { catch {set w [focus]} }
    if {[winfo exists $w]} {
	if {[bind $w $evt] ne ""} {
	    # Awkward ... this widget has a MouseWheel binding, but to
	    # trigger successfully in it, we must give it focus.
	    # XXX For now, let's do nothing - maybe check containing != focus?
	    # Users should restrict MouseWheel bindings to special cases only.
	    if {0} {
		catch {focus} old
		if {$w ne $old} { focus $w }
		event generate $w $evt -rootx $X -rooty $Y -delta $D
		if {$w ne $old} { catch {focus $old} }
	    }
	    return
	}
	# aqua and x11/win32 have different delta handling
	if {[tk windowingsystem] ne "aqua"} {
	    set delta [expr {- ($D / 30)}]
	} else {
	    set delta [expr {- ($D)}]
	}
	# scrollbars have different call conventions
	if {[string match "*Scrollbar" [winfo class $w]]} {
	    catch {tk::ScrollByUnits $w \
		       [string index [$w cget -orient] 0] $delta}
	} else {
	    set view [expr {$shifted ? "xview" : "yview"}]
	    # Walking up to find the proper widget handles cases like
	    # embedded widgets in a canvas
	    while {[catch {$w $view scroll $delta units}]
		   && [winfo toplevel $w] ne $w} {
		set w [winfo parent $w]
	    }
	}
    }
}
proc style::as::init_mousewheel {args} {
    variable mw

    # Create a catch-all MouseWheel proc & binding and
    # alter default bindings to allow toplevel binding to control all
    bind all <MouseWheel> [list ::style::as::MouseWheel %W %X %Y %D 0]
    bind all <Shift-MouseWheel> [list ::style::as::MouseWheel %W %X %Y %D 1]
    foreach class $mw(classes) {
	bind $class <MouseWheel> {}
	bind $class <Shift-MouseWheel> {}
    }
    #if {[bind [winfo toplevel %W] <MouseWheel>] ne ""} { continue }
    #%W yview scroll [expr {- (%D / 120) * 4}] units

    if {[tk windowingsystem] eq "x11"} {
	# Support for mousewheels on Linux/Unix commonly comes through
	# mapping the wheel to the extended buttons.
	bind all <Button-4> [list ::style::as::MouseWheel %W %X %Y 120]
	bind all <Button-5> [list ::style::as::MouseWheel %W %X %Y -120]
	foreach class $mw(classes) {
	    bind $class <Button-4> {}
	    bind $class <Button-5> {}
	}
    }
    # Disable this bwidget proc if it exists.  It creates bindings that
    # are unnecessary and possibly dangerous in combination
    catch { proc ::BWidget::bindMouseWheel args {} }
}
proc style::as::reset_mousewheel {args} {
    # Remove catch-all MouseWheel binding and restore default bindings
    variable mw

    bind all <MouseWheel> {}
    bind all <Shift-MouseWheel> {}
    foreach class $mw(classes) {
	bind $class <MouseWheel> $mw(binding)
	bind $class <Shift-MouseWheel> $mw(s-binding)
    }
    if {[tk windowingsystem] eq "x11"} {
	bind all <Button-4> {}
	bind all <Button-5> {}
	foreach class $mw(classes) {
	    bind $class <Button-4> $mw(binding4)
	    bind $class <Button-5> $mw(binding5)
	}
    }
}

package provide style::as $style::as::version
