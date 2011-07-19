# -*- tcl -*-
#
#  panelframe.tcl
#	Create PanelFrame widgets.
#	A PanelFrame is a boxed frame that allows you to place items
#	in the label area (liked combined frame+toolbar).  It uses the
#	highlight colors the default frame color.
#
#	Scrolled widget
#
# Copyright 2005 Jeffrey Hobbs
#
# RCS: @(#) $Id: panelframe.tcl,v 1.6 2010/06/01 18:06:52 hobbs Exp $
#

if 0 {
    # Samples
    lappend auto_path ~/cvs/tcllib/tklib/modules/widget

    package require widget::panelframe
    set f [widget::panelframe .pf -text "My Panel"]
    set sf [frame $f.f -padx 4 -pady 4]
    pack [text $sf.text] -fill both -expand 1
    $f setwidget $sf
    pack $f -fill both -expand 1 -padx 4 -pady 4
}

###

package require widget

namespace eval widget {
    variable entry_selbg
    variable entry_selfg
    if {![info exists entry_selbg]} {
	set entry_selbg [widget::tkresource entry -selectbackground]
	if {$entry_selbg eq ""} { set entry_selbg "black" }
	set entry_selfg [widget::tkresource entry -selectforeground]
	if {$entry_selfg eq ""} { set entry_selfg "black" }
    }
    snit::macro widget::entry-selectbackground {} [list return $entry_selbg]
    snit::macro widget::entry-selectforeground {} [list return $entry_selfg]

    variable imgdata {
	#define close_width 16
	#define close_height 16
	static char close_bits[] = {
	    0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x10, 0x08,
	    0x38, 0x1c, 0x70, 0x0e,
	    0xe0, 0x07, 0xc0, 0x03,
	    0xc0, 0x03, 0xe0, 0x07,
	    0x70, 0x0e, 0x38, 0x1c,
	    0x10, 0x08, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00};
    }
    # We use the same -foreground as the default
    image create bitmap ::widget::X -data $imgdata -foreground $entry_selfg
}

snit::widget widget::panelframe {
    hulltype frame ; # not themed

    component title
    component tframe
    #component frame
    #component close

    delegate option * to hull
    delegate method * to hull

    widget::propagate {-panelbackground panelBackground Background} \
	-default [widget::entry-selectbackground] to {hull title tframe} \
	as -background
    widget::propagate {-panelforeground panelForeground Foreground} \
	-default [widget::entry-selectforeground] to {title} \
	as -foreground

    # type listof 1..4 int
    option -ipad -default 1 -configuremethod C-ipad

    # should we use this instead of setwidget?
    #option -window -default "" -configuremethod C-window ; # -isa window

    # The use of a bold font by default would be better
    delegate option -font to title
    delegate option -text to title
    delegate option -textvariable to title

    # Should we have automatic state handling?
    #option -state -default normal

    if 0 {
	# This would be code to have an automated close button
	option -closebutton -default 0 -configuremethod C-closebutton
    }

    variable items {} ; # items user has added

    constructor args {
	$hull configure -borderwidth 1 -relief flat \
	    -background $options(-panelbackground)
	install tframe using frame $win.title \
	    -background $options(-panelbackground)
	install title using label $win.title.label -anchor w -bd 0 \
	    -background $options(-panelbackground) \
	    -foreground $options(-panelforeground)
	# does it need to be a ttk::frame ?
	#install frame using ttk::frame $win.frame

	foreach {ipadx ipady} [$self _padval $options(-ipad)] { break }

	if 0 {
	    install close using button $tframe.close -image ::widget::X \
		-padx 0 -pady 0 -relief flat -overrelief raised \
		-bd 1 -highlightthickness 0 \
		-background $options(-panelbackground) \
		-foreground $options(-panelforeground)
	    #$close configure -font "Marlett -14" -text \u0072
	    if {$options(-closebutton)} {
		pack $close -side right -padx $ipadx -pady $ipady
	    }
	}

	grid $tframe -row 0 -column 0 -sticky ew
	#grid $frame  -row 1 -column 0 -sticky news
	grid columnconfigure $win 0 -weight 1
	grid rowconfigure    $win 1 -weight 1
	#grid columnconfigure $frame 0 -weight 1
	#grid rowconfigure    $frame 0 -weight 1

	pack $title -side left -fill x -anchor w -padx $ipadx -pady $ipady

	$self configurelist $args
    }

    method C-ipad {option value} {
	set len [llength $value]
	foreach {a b} $value { break }
	if {$len == 0 || $len > 2} {
	    return -code error \
		"invalid pad value \"$value\", must be 1 or 2 pixel values"
	}
	pack configure $title -padx $ipadx -pady $ipady
	set options($option) $value
    }

    if 0 {
	method C-closebutton {option value} {
	    if {$value} {
		foreach {ipadx ipady} [$self _padval $options(-ipad)] { break }
		pack $close -side right -padx $ipadx -pady $ipady
	    } else {
		pack forget $close
	    }
	    set options($option) $value
	}
    }

    # We could create and extra frame and return it, but in order to
    # not decide whether that is a ttk or regular frame, just force
    # the user to use setwidget instead
    #method getframe {} { return $frame }

    variable setwidget {}
    method setwidget {w} {
	if {[winfo exists $setwidget]} {
	    grid remove $setwidget
	    set setwidget {}
	}
	if {[winfo exists $w]} {
	    grid $w -in $win -row 1 -column 0 -sticky news
	    set setwidget $w
	}
    }

    method add {w args} {
	array set opts [list \
			    -side   right \
			    -fill   none \
			    -expand 0 \
			    -pad    $options(-ipad) \
			   ]
	foreach {key val} $args {
	    if {[info exists opts($key)]} {
		set opts($key) $val
	    } else {
		set msg "unknown option \"$key\", must be one of: "
		append msg [join [lsort [array names opts]] {, }]
		return -code error $msg
	    }
	}
	foreach {ipadx ipady} [$self _padval $opts(-pad)] { break }

	lappend items $w
	pack $w -in $tframe -padx $ipadx -pady $ipady -side $opts(-side) \
	    -fill $opts(-fill) -expand $opts(-expand)

	return $w
    }

    method remove {args} {
	set destroy [string equal [lindex $args 0] "-destroy"]
	if {$destroy} {
	    set args [lrange $args 1 end]
	}
	foreach w $args {
	    set idx [lsearch -exact $items $w]
	    if {$idx == -1} {
		# ignore unknown
		continue
	    }
	    if {$destroy} {
		destroy $w
	    } elseif {[winfo exists $w]} {
		pack forget $w
	    }
	    set items [lreplace $items $idx $idx]
	}
    }

    method delete {args} {
	return [$self remove -destroy $args]
    }

    method items {} {
	return $items
    }

    method _padval {padval} {
	set len [llength $padval]
	foreach {a b} $padval { break }
	if {$len == 0 || $len > 2} {
	    return -code error \
		"invalid pad value \"$padval\", must be 1 or 2 pixel values"
	} elseif {$len == 1} {
	    return [list $a $a]
	} elseif {$len == 2} {
	    return $padval
	}
    }
}

package provide widget::panelframe 1.1
