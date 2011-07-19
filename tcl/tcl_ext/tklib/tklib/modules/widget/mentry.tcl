# -*- tcl -*-
#
# mentry.tcl -
#
#	MenuEntry widget
#
# RCS: @(#) $Id: mentry.tcl,v 1.7 2010/06/01 18:06:52 hobbs Exp $
#

# Creation and Options - widget::menuentry $path ...
#  -menu -default "" ; menu to associate with entry
#  -image -default "default"
#  All other options to entry
#
# Methods
#  All other methods to entry
#
# Bindings
#  NONE
#

if 0 {
    # Samples
    package require widget::menuentry
    set me [widget::menuentry .me]
    set menu [menu .me.menu -tearoff 0]
    $menu add radiobutton -label "Name" -variable foo -value name
    $menu add radiobutton -label "Abstract" -variable foo -value abstract
    $menu add separator
    $menu add radiobutton -label "Name and Abstract" \
	-variable foo -value [list name abstract]
    $me configure -menu $menu
    pack $me -fill x -expand 1 -padx 4 -pady 4
}

###

package require widget

namespace eval ::widget {
    # PNG version has partial alpha transparency for better look
    variable menuentry_pngdata {
	iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAACXBIWXMAAAs6
	AAALOgFkf1cNAAACkklEQVR4nHWSXUhTYRjHdxnRVQTeCElXp7vS6BCZFGlO
	nc2vbdrccrbhR9IKI7KOXzQniikzUvyIlNoHrZgXmYrbas6cg3keKFKoqHiC
	VowgeC6C4PB24RmlRy/+Nw/v7/c+/5dXxRhTMcZUoqeWF73mgOi1pMBnlURP
	vZSYNqVWJw2BlZFKPn1uezZhr8kGPktS9JjFxPQFIf7AwK1O6LnVcZ0QGzeI
	sVFDcslVZttRIHpqefBZkmuPjU5AOgxIVYBkB6QWQCoFpENRV5kz6qpMhvs0
	ik1Uax5zYM1tFgGJA6QmQGoDpBuAdB2QrgGSEZCyIoNaMdSnCeywQV0qMVUj
	AFIFIN2U4VYZbgGkZkDKDzlLhHBfaUohAG+9FJ80cIB0+b9b0xWaAKkBkIyL
	3Wou3K+VlBXcFik2puPkg3ZAuiLLGuWZFZAM8x0FXMipUQriD42p2GiVAEhq
	GWyWYRsgXQKkOkDKm7tdIMx3FiorrIzpAysjOhGQsgBJL4NWQLLIsBaQMhe6
	i36/aDsbVwiiw+X88n1dMjKkdQLSQUA6A0gGQNIBUi4gZUaHdX/e+O0s3Hqa
	zdhzaxQf6dXAedvSUFky3F8qBh1FwkLnOW6uvYCbu5UvRAYqpPXnbexrYox9
	Wr7Lgne07GnjiYwtAsaYKthTzAd7igNBpyYVcmqkoKNEmuso/LXYrWEfXvay
	7+8esR8bbvZ+sYv5rackX/3xjC2C3TJzNc8UGaxmn18PseTbKfYldo/FJyys
	V8199FzM2bu5hkrFtud/ybPmk6ago5xtzLaz9dlOFnXpmb+B/+k2Z+/79xi7
	wOk8sfEmd20OW+hSM7+V/+Y2Zx9QVNgNTsdbd2z/RPURh9t8dE969hckF6c1
	n3C8ywAAAABJRU5ErkJggg==
    }
    variable menuentry_gifdata {
	R0lGODlhEAAQAPcAAAQEBIREJJpaL6RaL6RkL6RkOq9kOq9vOrpvRLp6RLqE
	T7qPT8SPT8SaT8SaWsSaZM+kWs+kZM+vb8/k79qvetq6etq6hNrEj+TPmuTP
	pOTapPr6+gAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	AAAAAAAAAAAAAAAAAP///yH5BAEAAP8ALAAAAAAQABAAQAh4AP8JhBChIAQH
	AhMKdIBQYcIECRRGcOhQAcWLDi5kuPAggMAIECgyYOBw4kWBFh0yWKCQAQUM
	F1ImBECT4oAEBiSGTMiQIoSdImX+M3mSJc+TAiMqdEDSoQMJCC4qmKoggQIL
	GjRYyCmQpleFCipUcMC160kBCQMCADs=
    }
}

proc ::widget::createMenuEntryLayout {} {
    variable MENUENTRY
    if {[info exists MENUENTRY]} { return }
    set MENUENTRY 1
    variable menuentry_pngdata
    variable menuentry_gifdata
    set img ::widget::img_menuentry
    if {[package provide img::png] != ""} {
	image create photo $img -format PNG -data $menuentry_pngdata
    } else {
	image create photo $img -format GIF -data $menuentry_gifdata
    }
    namespace eval ::ttk [list set img $img] ; # namespace resolved
    namespace eval ::ttk {
	# Create -padding for space on left and right of icon
	set pad [expr {[image width $img] + 4}]
	style theme settings "default" {
	    style layout MenuEntry {
		Entry.field -children {
		    MenuEntry.icon -side left
		    Entry.padding -children {
			Entry.textarea
		    }
		}
	    }
	    # center icon in padded cell
	    style element create MenuEntry.icon image $img \
		-sticky "" -padding [list $pad 0 0 0]
	}
	if 0 {
	    # Some mappings would be required per-theme to adapt to theme
	    # changes
	    foreach theme [style theme names] {
		style theme settings $theme {
		    # Could have disabled, pressed, ... state images
		    #style map MenuEntry -image [list disabled $img]
		}
	    }
	}
    }
}

snit::widgetadaptor widget::menuentry {
    delegate option * to hull
    delegate method * to hull

    option -image -default "default" -configuremethod C-image
    option -menu -default "" -configuremethod C-menu

    constructor args {
	::widget::createMenuEntryLayout

	installhull using ttk::entry -style MenuEntry

	bindtags $win [linsert [bindtags $win] 1 TMenuEntry]

	$self configurelist $args
    }

    method C-menu {option value} {
	if {$value ne "" && ![winfo exists $value]} {
	    return -code error "invalid widget \"$value\""
	}
	set options($option) $value
    }

    method C-image {option value} {
	set options($option) $value
	if {$value eq "default"} {
	}
    }
}

# Bindings for menu portion.
#
# This is a variant of the ttk menubutton.tcl bindings.
# See menubutton.tcl for detailed behavior info.
#

namespace eval ttk {
    bind TMenuEntry <Enter>	{ %W state active }
    bind TMenuEntry <Leave>	{ %W state !active }
    bind TMenuEntry <<Invoke>> 	{ ttk::menuentry::Popdown %W %x %y }
    bind TMenuEntry <Control-space> { ttk::menuentry::Popdown %W 10 10 }

    if {[tk windowingsystem] eq "x11"} {
	bind TMenuEntry <ButtonPress-1>   { ttk::menuentry::Pulldown %W %x %y }
	bind TMenuEntry <ButtonRelease-1> { ttk::menuentry::TransferGrab %W }
	bind TMenuEntry <B1-Leave>  	  { ttk::menuentry::TransferGrab %W }
    } else {
    	bind TMenuEntry <ButtonPress-1>  \
	    { %W state pressed ; ttk::menuentry::Popdown %W %x %y }
	bind TMenuEntry <ButtonRelease-1> { %W state !pressed }
    }

    namespace eval menuentry {
	variable State

	array set State {
	    pulldown	0
	    oldcursor	{}
	}
    }
}

# PostPosition --
#	Returns the x and y coordinates where the menu 
#	should be posted, based on the menuentry and menu size
#	and -direction option.
#
# TODO: adjust menu width to be at least as wide as the button
#	for -direction above, below.
#
proc ttk::menuentry::PostPosition {mb menu} {
    set x [winfo rootx $mb]
    set y [winfo rooty $mb]
    set dir "below" ; #[$mb cget -direction]

    set bw [winfo width $mb]
    set bh [winfo height $mb]
    set mw [winfo reqwidth $menu]
    set mh [winfo reqheight $menu]
    set sw [expr {[winfo screenwidth  $menu] - $bw - $mw}]
    set sh [expr {[winfo screenheight $menu] - $bh - $mh}]

    switch -- $dir {
	above { if {$y >= $mh} { incr y -$mh } { incr y  $bh } }
	below { if {$y <= $sh} { incr y  $bh } { incr y -$mh } }
	left  { if {$x >= $mw} { incr x -$mw } { incr x  $bw } }
	right { if {$x <= $sw} { incr x  $bw } { incr x -$mw } }
	flush {
	    # post menu atop menuentry.
	    # If there's a menu entry whose label matches the
	    # menuentry -text, assume this is an optionmenu
	    # and place that entry over the menuentry.
	    set index [FindMenuEntry $menu [$mb cget -text]]
	    if {$index ne ""} {
		incr y -[$menu yposition $index]
	    }
	}
    }

    return [list $x $y]
}

# Popdown --
#	Post the menu and set a grab on the menu.
#
proc ttk::menuentry::Popdown {me x y} {
    if {[$me instate disabled] || [set menu [$me cget -menu]] eq ""
	|| [$me identify $x $y] ne "MenuEntry.icon"} {
	return
    }
    foreach {x y} [PostPosition $me $menu] { break }
    tk_popup $menu $x $y
}

# Pulldown (X11 only) --
#	Called when Button1 is pressed on a menuentry.
#	Posts the menu; a subsequent ButtonRelease 
#	or Leave event will set a grab on the menu.
#
proc ttk::menuentry::Pulldown {mb x y} {
    variable State
    if {[$mb instate disabled] || [set menu [$mb cget -menu]] eq ""
	|| [$mb identify $x $y] ne "MenuEntry.icon"} {
	return
    }
    foreach {x y} [PostPosition $mb $menu] { break }
    set State(pulldown) 1
    set State(oldcursor) [$mb cget -cursor]

    $mb state pressed
    $mb configure -cursor [$menu cget -cursor]
    $menu post $x $y
    tk_menuSetFocus $menu
}

# TransferGrab (X11 only) --
#	Switch from pulldown mode (menuentry has an implicit grab)
#	to popdown mode (menu has an explicit grab).
#
proc ttk::menuentry::TransferGrab {mb} {
    variable State
    if {$State(pulldown)} {
	$mb configure -cursor $State(oldcursor)
	$mb state {!pressed !active}
	set State(pulldown) 0
	grab -global [$mb cget -menu]
    }
}

# FindMenuEntry --
#	Hack to support tk_optionMenus.
#	Returns the index of the menu entry with a matching -label,
#	-1 if not found.
#
proc ttk::menuentry::FindMenuEntry {menu s} {
    set last [$menu index last]
    if {$last eq "none"} {
	return ""
    }
    for {set i 0} {$i <= $last} {incr i} {
	if {![catch {$menu entrycget $i -label} label]
	    && ($label eq $s)} {
	    return $i
	}
    }
    return ""
}

package provide widget::menuentry 1.0.1
