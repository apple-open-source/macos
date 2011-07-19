# -*- tcl -*-
#
# scrollw.tcl -
#
#	Scrolled widget
#
# RCS: @(#) $Id: scrollw.tcl,v 1.15 2010/06/01 18:06:52 hobbs Exp $
#

# Creation and Options - widget::scrolledwindow $path ...
#  -scrollbar -default "both" ; vertical horizontal none
#  -auto      -default "both" ; vertical horizontal none
#  -sides     -default "se"   ;
#  -size      -default 0      ; scrollbar -width (not recommended to change)
#  -ipad      -default {0 0}  ; represents internal {x y} padding between
#			      ; scrollbar and given widget
#  All other options to frame
#
# Methods
#  $path getframe           => $frame
#  $path setwidget $widget  => $widget
#  All other methods to frame
#
# Bindings
#  NONE
#

if 0 {
    # Samples
    package require widget::scrolledwindow
    #set sw [widget::scrolledwindow .sw -scrollbar vertical]
    #set text [text .sw.text -wrap word]
    #$sw setwidget $text
    #pack $sw -fill both -expand 1

    set sw [widget::scrolledwindow .sw -borderwidth 1 -relief sunken]
    set text [text $sw.text -borderwidth 0 -height 4 -width 20]
    $sw setwidget $text
    pack $sw -fill both -expand 1 -padx 4 -pady 4

    set sw [widget::scrolledwindow .ssw -borderwidth 2 -relief solid]
    set text [text $sw.text -borderwidth 0 -height 4 -width 20]
    $sw setwidget $text
    pack $sw -fill both -expand 1 -padx 4 -pady 4
}

###

package require widget

snit::widget widget::scrolledwindow {
    hulltype ttk::frame

    component hscroll
    component vscroll

    delegate option * to hull
    delegate method * to hull
    #delegate option -size to {hscroll vscroll} as -width

    option -scrollbar -default "both" -configuremethod C-scrollbar \
	-type [list snit::enum -values [list none horizontal vertical both]]
    option -auto      -default "both" -configuremethod C-scrollbar \
	-type [list snit::enum -values [list none horizontal vertical both]]
    option -sides     -default "se" -configuremethod C-scrollbar \
	-type [list snit::enum -values [list ne en nw wn se es sw ws]]
    option -size      -default 0 -configuremethod C-size \
	-type [list  snit::integer -min 0 -max 30]
    option -ipad      -default 0 -configuremethod C-ipad \
	-type [list snit::listtype -type {snit::integer} -minlen 1 -maxlen 2]

    typevariable scrollopts {none horizontal vertical both}

    variable realized 0    ; # set when first Configure'd
    variable hsb -array {
	packed 0 present 0 auto 0 row 2 col 1 lastmin -1 lastmax -1 lock 0
	sticky "ew" padx 0 pady 0
    }
    variable vsb -array {
	packed 0 present 0 auto 0 row 1 col 2 lastmin -1 lastmax -1 lock 0
	sticky "ns" padx 0 pady 0
    }
    variable pending {}    ; # pending after id for scrollbar mgmt

    constructor args {
	if {[tk windowingsystem] ne "aqua"} {
	    # ttk scrollbars on aqua are a bit wonky still
	    install hscroll using ttk::scrollbar $win.hscroll \
		-orient horizontal -takefocus 0
	    install vscroll using ttk::scrollbar $win.vscroll \
		-orient vertical -takefocus 0
	} else {
	    install hscroll using scrollbar $win.hscroll \
		-orient horizontal -takefocus 0
	    install vscroll using scrollbar $win.vscroll \
		-orient vertical -takefocus 0
	    # in case the scrollbar has been overridden ...
	    catch {$hscroll configure -highlightthickness 0}
	    catch {$vscroll configure -highlightthickness 0}
	}

	set hsb(bar) $hscroll
	set vsb(bar) $vscroll
	bind $win <Configure> [mymethod _realize $win]

	grid columnconfigure $win 1 -weight 1
	grid rowconfigure    $win 1 -weight 1

	set pending [after idle [mymethod _setdata]]
	$self configurelist $args
    }

    destructor {
	after cancel $pending
	set pending {}
    }

    # Do we need this ??
    method getframe {} { return $win }

    variable setwidget {}
    method setwidget {widget} {
	if {$setwidget eq $widget} { return }
	if {[winfo exists $setwidget]} {
	    grid remove $setwidget
	    # in case we only scroll in one direction
	    catch {$setwidget configure -xscrollcommand ""}
	    catch {$setwidget configure -yscrollcommand ""}
	    $hscroll configure -command {}
	    $vscroll configure -command {}
	    set setwidget {}
	}
	if {$pending ne {}} {
	    # ensure we have called most recent _setdata
	    after cancel $pending
	    $self _setdata
	}
	if {[winfo exists $widget]} {
	    set setwidget $widget
	    grid $widget -in $win -row 1 -column 1 -sticky news

	    # in case we only scroll in one direction
	    if {$hsb(present)} {
		$widget configure -xscrollcommand [mymethod _set_scroll hsb]
		$hscroll configure -command [list $widget xview]
	    }
	    if {$vsb(present)} {
		$widget configure -yscrollcommand [mymethod _set_scroll vsb]
		$vscroll configure -command [list $widget yview]
	    }
	}
	return $widget
    }

    method C-size {option value} {
	set options($option) $value
	$vscroll configure -width $value
	$hscroll configure -width $value
    }

    method C-scrollbar {option value} {
	set options($option) $value
	after cancel $pending
	set pending [after idle [mymethod _setdata]]
    }

    method C-ipad {option value} {
	set options($option) $value
	# double value to ensure a single int value covers pad x and y
	foreach {padx pady} [concat $value $value] { break }
	set vsb(padx) [list $padx 0] ; set vsb(pady) 0
	set hsb(padx) 0              ; set vsb(pady) [list $pady 0]
	if {$vsb(present) && $vsb(packed)} {
	    grid configure $vsb(bar) -padx $vsb(padx) -pady $vsb(pady)
	}
	if {$hsb(present) && $hsb(packed)} {
	    grid configure $hsb(bar) -padx $hsb(padx) -pady $hsb(pady)
	}
    }

    method _set_scroll {varname vmin vmax} {
	if {!$realized} { return }
	# This is only called if the scrollbar is attached properly
	upvar 0 $varname sb
	if {$sb(auto)} {
	    if {!$sb(lock)} {
		# One last check to avoid loops when not locked
		if {$vmin == $sb(lastmin) && $vmax == $sb(lastmax)} {
		    return
		}
		set sb(lastmin) $vmin
		set sb(lastmax) $vmax
	    }
	    if {$sb(packed) && $vmin == 0 && $vmax == 1} {
		if {!$sb(lock)} {
		    set sb(packed) 0
		    grid remove $sb(bar)
		}
	    } elseif {!$sb(packed) && ($vmin != 0 || $vmax != 1)} {
		set sb(packed) 1
		grid $sb(bar) -column $sb(col) -row $sb(row) \
		    -sticky $sb(sticky) -padx $sb(padx) -pady $sb(pady)
	    }
	    set sb(lock) 1
	    update idletasks
	    set sb(lock) 0
	}
	$sb(bar) set $vmin $vmax
    }

    method _setdata {} {
	set pending {}
	set bar   [lsearch -exact $scrollopts $options(-scrollbar)]
	set auto  [lsearch -exact $scrollopts $options(-auto)]

	set hsb(present) [expr {$bar & 1}]  ; # idx 1 or 3
	set hsb(auto)    [expr {$auto & 1}] ; # idx 1 or 3
	set hsb(row)     [expr {[string match *n* $options(-sides)] ? 0 : 2}]
	set hsb(col)     1
	set hsb(sticky)  "ew"

	set vsb(present) [expr {$bar & 2}]  ; # idx 2
	set vsb(auto)    [expr {$auto & 2}] ; # idx 2
	set vsb(row)     1
	set vsb(col)     [expr {[string match *w* $options(-sides)] ? 0 : 2}]
	set vsb(sticky)	 "ns"

	if {$setwidget eq ""} {
	    grid remove $hsb(bar)
	    grid remove $vsb(bar)
	    set hsb(packed) 0
	    set vsb(packed) 0
	    return
	}

	foreach varname {hsb vsb} {
	    upvar 0 $varname sb
	    foreach {vmin vmax} [$sb(bar) get] { break }
	    set sb(packed) [expr {$sb(present) &&
				   (!$sb(auto) || ($vmin != 0 || $vmax != 1))}]
	    if {$sb(packed)} {
		grid $sb(bar) -column $sb(col) -row $sb(row) \
		    -sticky $sb(sticky) -padx $sb(padx) -pady $sb(pady)
	    } else {
		grid remove $sb(bar)
	    }
	}
    }

    method _realize {w} {
	if {$w eq $win} {
	    bind $win <Configure> {}
	    set realized 1
	}
    }
}

package provide widget::scrolledwindow 1.2.1
