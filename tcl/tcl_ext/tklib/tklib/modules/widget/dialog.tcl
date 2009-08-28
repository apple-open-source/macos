# -*- tcl -*-
#
# dialog.tcl -
#
#	Generic dialog widget (themed)
#
# RCS: @(#) $Id: dialog.tcl,v 1.21 2007/03/02 00:38:03 hobbs Exp $
#

# Creation and Options - widget::dialog $path ...
#    -command	-default {} ; # gets appended: $win $reason
#    -focus     -default {} ; # subwindow to set focus on display
#    -modal	-default none
#    -padding	-default 0
#    -parent	-default ""
#    -place	-default center
#    -separator	-default 1
#    -synchronous -default 1
#    -title	-default ""
#    -transient -default 1
#    -type	-default custom ; # {ok okcancel okcancelapply custom}
#    -timeout	-default 0 ; # only active with -synchronous
#
# Methods
#  $path add $what $args... => $id
#  $path getframe           => $frame
#  $path setwidget $widget  => ""
#  $path display
#  $path cancel
#  $path withdraw
#
# Bindings
#  Escape            => invokes [$dlg close cancel]
#  WM_DELETE_WINDOW  => invokes [$dlg close cancel]
#

if 0 {
    # Samples
    package require widget::dialog
    set dlg [widget::dialog .pkgerr -modal local -separator 1 \
		 -place right -parent . -type okcancel \
		 -title "Dialog Title"]
    set frame [frame $dlg.f]
    label $frame.lbl -text "Type Something In:"
    entry $frame.ent
    grid $frame.lbl $frame.ent -sticky ew
    grid columnconfigure $frame 1 -weight 1
    $dlg setwidget $frame
    puts [$dlg display]
    destroy $dlg

    # Using -synchronous with a -type custom dialog requires that the
    # custom buttons call [$dlg close $reason] to trigger the close
    set dlg [widget::dialog .pkgerr -title "Yes/No Dialog" -separator 1 \
		 -parent . -type custom]
    set frame [frame $dlg.f]
    label $frame.lbl -text "Type Something In:"
    entry $frame.ent
    grid $frame.lbl $frame.ent -sticky ew
    grid columnconfigure $frame 1 -weight 1
    $dlg setwidget $frame
    $dlg add button -text "Yes" -command [list $dlg close yes]
    $dlg add button -text "No" -command [list $dlg close no]
    puts [$dlg display]
}

# ### ######### ###########################
## Prerequisites

#package require image   ; # bitmaps
package require snit    ; # object system
package require tile
package require msgcat

# ### ######### ###########################
## Implementation

snit::widget widget::dialog {
    # ### ######### ###########################
    hulltype toplevel

    component frame
    component separator
    component buttonbox

    delegate option -padding to frame;
    delegate option * to hull
    delegate method * to hull

    option -command	-default {};
    # {none local global}
    option -modal	-default none -configuremethod C-modal;
    #option -padding	-default 0 -configuremethod C-padding;
    option -parent	-default "" -configuremethod C-parent;
    # {none center left right above below over}
    option -place	-default center -configuremethod C-place;
    option -separator	-default 1 -configuremethod C-separator;
    option -synchronous -default 1;
    option -title	-default "" -configuremethod C-title;
    option -transient	-default 1 -configuremethod C-transient;
    option -type	-default custom -configuremethod C-type;
    option -timeout	-default 0;
    option -focus	-default "";

    # We may make this an easier customizable messagebox, but not yet
    #option -anchor      c; # {n e w s c}
    #option -text	"";
    #option -bitmap	"";
    #option -image	"";

    # ### ######### ###########################
    ## Public API. Construction

    constructor {args} {
	wm withdraw $win

	install frame using ttk::frame $win._frame
	install separator using ttk::separator $win._separator \
	    -orient horizontal
	if {[tk windowingsystem] eq "aqua"} {
	    # left top right bottom - Aqua corner resize control padding
	    set btnpad [list 0 6 14 4]
	} else {
	    # left top right bottom
	    set btnpad [list 0 6 0 4]
	}
	install buttonbox using ttk::frame $win._buttonbox -padding $btnpad

	grid $frame     -row 0 -column 0 -sticky news
	grid $separator -row 1 -column 0 -sticky ew
	# Should padding effect the buttonbox?
	grid $buttonbox -row 2 -column 0 -sticky ew

	grid columnconfigure $win 0 -weight 1
	grid rowconfigure    $win 0 -weight 1

	# Default to invoking no/cancel/withdraw
	wm protocol $win WM_DELETE_WINDOW [mymethod close cancel]
	bind $win <Key-Escape> [mymethod close cancel]
	# Ensure grab release on unmap?
	#bind $win <Unmap> [list grab release $win]

	# Handle defaults
	if {!$options(-separator)} {
	    grid remove $separator
	}

	$self configurelist $args
    }

    # ### ######### ###########################
    ## Public API. Extend container by application specific content.

    # getframe and setwidget are somewhat mutually exlusive.
    # Use one or the other.
    method getframe {} {
	return $frame
    }

    method setwidget {w} {
	if {[winfo exists $setwidget]} {
	    grid remove $setwidget
	    set setwidget {}
	}
	if {[winfo exists $w]} {
	    grid $w -in $frame -row 0 -column 0 -sticky news
	    grid columnconfigure $frame 0 -weight 1
	    grid rowconfigure    $frame 0 -weight 1
	    set setwidget $w
	}
    }

    variable uid 0
    method add {what args} {
	if {$what eq "button"} {
	    set w [eval [linsert $args 0 ttk::button $buttonbox._b[incr uid]]]
	} elseif {[winfo exists $what]} {
	    set w $what
	} else {
	    return -code error "unknown add type \"$what\", must be:\
		button or a pathname"
	}
	set col [lindex [grid size $buttonbox] 0]; # get last column
	if {$col == 0} {
	    # ensure weighted 0 column
	    grid columnconfigure $buttonbox 0 -weight 1
	    incr col
	}
	grid $w -row 0 -column $col -sticky ew -padx 4
	return $w
    }

    method display {} {
	set lastFocusGrab [focus]
	set last [grab current $win]
	lappend lastFocusGrab $last
	if {[winfo exists $last]} {
	    lappend lastFocusGrab [grab status $last]
	}

	$self PlaceWindow $win $options(-place) $options(-parent)
	if {$options(-modal) ne "none"} {
	    if {$options(-modal) eq "global"} {
		catch {grab -global $win}
	    } else {
		catch {grab $win}
	    }
	}
	if {[winfo exists $options(-focus)]} {
	    catch { focus $options(-focus) }
	}
	# In order to allow !custom synchronous, we need to allow
	# custom dialogs to set [myvar result].  They do that through
	# [$dlg close $reason]
	if {$options(-synchronous)} {
	    if {$options(-timeout) > 0} {
		# set var after specified timeout
		set timeout_id [after $options(-timeout) \
				    [list set [myvar result] timeout]]
	    }
	    vwait [myvar result]
	    catch {after cancel $timeout_id}
	    return [$self withdraw $result]
	}
    }

    method close {{reason {}}} {
	set code 0
	if {$options(-command) ne ""} {
	    set cmd $options(-command)
	    lappend cmd $win $reason
	    set code [catch {uplevel \#0 $cmd} result]
	} else {
	    # set result to trigger any possible vwait
	    set result $reason
	}
	if {$code == 3} {
	    # 'break' return code - don't withdraw
	    return $result
	} else {
	    # Withdraw on anything but 'break' return code
	    $self withdraw $result
	}
	return -code $code $result
    }

    method withdraw {{reason "withdraw"}} {
	set result $reason
	catch {grab release $win}
	# Let's avoid focus/grab restore if we don't think we were showing
	if {![winfo ismapped $win]} { return $reason }
	wm withdraw $win
	foreach {oldFocus oldGrab oldStatus} $lastFocusGrab { break }
	# Ensure last focus/grab wasn't a child of this window
	if {[winfo exists $oldFocus] && ![string match $win* $oldFocus]} {
	    catch {focus $oldFocus}
	}
	if {[winfo exists $oldGrab] && ![string match $win* $oldGrab]} {
	    if {$oldStatus eq "global"} {
		catch {grab -global $oldGrab}
	    } elseif {$oldStatus eq "local"} {
		catch {grab $oldGrab}
	    }
	}
	return $result
    }

    # ### ######### ###########################
    ## Internal. State variable for close-button (X)

    variable lastFocusGrab {};
    variable isPlaced 0;
    variable result {};
    variable setwidget {};

    # ### ######### ###########################
    ## Internal. Handle changes to the options.

    method C-title {option value} {
	wm title $win $value
	wm iconname $win $value
        set options($option) $value
    }
    method C-modal {option value} {
	set values [list none local global]
	if {[lsearch -exact $values $value] == -1} {
	    return -code error "unknown $option option \"$value\":\
		must be one of [join $values {, }]"
	}
        set options($option) $value
    }
    method C-separator {option value} {
	if {$value} {
	    grid $separator
	} else {
	    grid remove $separator
	}
        set options($option) $value
    }
    method C-parent {option value} {
	if {$options(-transient) && [winfo exists $value]} {
	    wm transient $win [winfo toplevel $value]
	    wm group $win [winfo toplevel $value]
	} else {
	    wm transient $win ""
	    wm group $win ""
	}
        set options($option) $value
    }
    method C-transient {option value} {
	if {$value && [winfo exists $options(-parent)]} {
	    wm transient $win [winfo toplevel $options(-parent)]
	    wm group $win [winfo toplevel $options(-parent)]
	} else {
	    wm transient $win ""
	    wm group $win ""
	}
        set options($option) $value
    }
    method C-place {option value} {
	set values [list none center left right over above below pointer]
	if {[lsearch -exact $values $value] == -1} {
	    return -code error "unknown $option option \"$value\":\
		must be one of [join $values {, }]"
	}
	set isPlaced 0
        set options($option) $value
    }
    method C-type {option value} {
	set types [list ok okcancel okcancelapply custom]
	# ok
	# okcancel
	# okcancelapply
	# custom
	# msgcat

	if {$options(-type) eq $value} { return }
	if {[lsearch -exact $types $value] == -1} {
	    return -code error "invalid type \"$value\", must be one of:\
		[join $types {, }]"
	}
	if {$options(-type) ne "custom"} {
	    # Just trash whatever we had
	    eval [list destroy] [winfo children $buttonbox]
	}

	set ok     [msgcat::mc "OK"]
	set cancel [msgcat::mc "Cancel"]
	set apply  [msgcat::mc "Apply"]
	set okBtn  [ttk::button $buttonbox.ok -text $ok -default active \
			-command [mymethod close ok]]
	set canBtn [ttk::button $buttonbox.cancel -text $cancel \
			-command [mymethod close cancel]]
	set appBtn [ttk::button $buttonbox.apply -text $apply \
			-command [mymethod close apply]]

	# [OK] [Cancel] [Apply]
	grid x $okBtn $canBtn $appBtn -padx 4
	grid columnconfigure $buttonbox 0 -weight 1
	#bind $win <Return> [list $okBtn invoke]
	#bind $win <Escape> [list $canBtn invoke]
	if {$value eq "ok"} {
	    grid remove $canBtn $appBtn
	} elseif {$value eq "okcancel"} {
	    grid remove $appBtn
	}
        set options($option) $value
    }

    # ### ######### ###########################
    ## Internal.

    method PlaceWindow {w place anchor} {
	# Variation of tk::PlaceWindow
	if {$isPlaced || $place eq "none"} {
	    # For most options, we place once and then just deiconify
	    wm deiconify $w
	    raise $w
	    return
	}
	set isPlaced 1
	if {$place eq "pointer"} {
	    # pointer placement occurs each time, centered
	    set anchor center
	    set isPlaced 0
	} elseif {![winfo exists $anchor]} {
	    set anchor [winfo toplevel [winfo parent $w]]
	    if {![winfo ismapped $anchor]} {
		set place center
	    }
	}
	wm withdraw $w
	update idletasks
	set checkBounds 1
	if {$place eq "center"} {
	    set x [expr {([winfo screenwidth $w]-[winfo reqwidth $w])/2}]
	    set y [expr {([winfo screenheight $w]-[winfo reqheight $w])/2}]
	    set checkBounds 0
	} elseif {$place eq "pointer"} {
	    ## place at POINTER (centered)
	    if {$anchor eq "center"} {
		set x [expr {[winfo pointerx $w]-[winfo reqwidth $w]/2}]
		set y [expr {[winfo pointery $w]-[winfo reqheight $w]/2}]
	    } else {
		set x [winfo pointerx $w]
		set y [winfo pointery $w]
	    }
	} elseif {![winfo ismapped $anchor]} {
	    ## All the rest require the anchor to be mapped
	    ## If the anchor isn't mapped, use center
	    set x [expr {([winfo screenwidth $w]-[winfo reqwidth $w])/2}]
	    set y [expr {([winfo screenheight $w]-[winfo reqheight $w])/2}]
	    set checkBounds 0
	} elseif {$place eq "over"} {
	    ## center about WIDGET $anchor
	    set x [expr {[winfo rootx $anchor] + \
			     ([winfo width $anchor]-[winfo reqwidth $w])/2}]
	    set y [expr {[winfo rooty $anchor] + \
			     ([winfo height $anchor]-[winfo reqheight $w])/2}]
	} elseif {$place eq "above"} {
	    ## above (north of) WIDGET $anchor, centered
	    set x [expr {[winfo rootx $anchor] + \
			     ([winfo width $anchor]-[winfo reqwidth $w])/2}]
	    set y [expr {[winfo rooty $anchor] - [winfo reqheight $w]}]
	} elseif {$place eq "below"} {
	    ## below WIDGET $anchor, centered
	    set x [expr {[winfo rootx $anchor] + \
			     ([winfo width $anchor]-[winfo reqwidth $w])/2}]
	    set y [expr {[winfo rooty $anchor] + [winfo height $anchor]}]
	} elseif {$place eq "left"} {
	    ## left of WIDGET $anchor, top-aligned
	    set x [expr {[winfo rootx $anchor] - [winfo reqwidth $w]}]
	    set y [winfo rooty $anchor]
	} elseif {$place eq "right"} {
	    ## right of WIDGET $anchor, top-aligned
	    set x [expr {[winfo rootx $anchor] + [winfo width $anchor]}]
	    set y [winfo rooty $anchor]
	} else {
	    return -code error "unknown place type \"$place\""
	}
	if {[tk windowingsystem] eq "win32"} {
	    # win32 multiple desktops may produce negative geometry - avoid.
	    set checkBounds -1
	}
	if {$checkBounds} {
	    if {$x < 0 && $checkBounds > 0} {
		set x 0
	    } elseif {$x > ([winfo screenwidth $w]-[winfo reqwidth $w])} {
		set x [expr {[winfo screenwidth $w]-[winfo reqwidth $w]}]
	    }
	    if {$y < 0 && $checkBounds > 0} {
		set y 0
	    } elseif {$y > ([winfo screenheight $w]-[winfo reqheight $w])} {
		set y [expr {[winfo screenheight $w]-[winfo reqheight $w]}]
	    }
	    if {[tk windowingsystem] eq "aqua"} {
		# Avoid the native menu bar which sits on top of everything.
		if {$y < 20} { set y 20 }
	    }
	}
	wm geometry $w +$x+$y
	wm deiconify $w
	raise $w
    }

    # ### ######### ###########################
}

# ### ######### ###########################
## Ready for use

package provide widget::dialog 1.2
