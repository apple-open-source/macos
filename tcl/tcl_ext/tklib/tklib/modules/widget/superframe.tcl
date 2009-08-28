# -*- tcl -*-
#
# superframe.tcl -
#
#	Superframe widget - enhanced labelframe widget
#
# RCS: @(#) $Id: superframe.tcl,v 1.3 2006/09/29 16:25:07 hobbs Exp $
#

# Allows 3 styles of labelframes:
#  border        standard labelframe
#  whitespace    labelframe with inset contents, no border
#  separator     labelframe with inset contents, topright separator
#
# Based on OS X grouping types:
#   http://developer.apple.com/documentation/UserExperience/Conceptual/OSXHIGuidelines/XHIGLayout/chapter_19_section_4.html
#

# ### ######### ###########################
## Prerequisites

package require widget
# We could do this without tile ... but let's not
package require tile

# ### ######### ###########################
## Implementation

snit::widgetadaptor widget::superframe {
    # ### ######### ###########################
    delegate option * to hull except {-style -labelwidget -text -font}
    delegate method * to hull

    option -style -default border -readonly 1;
    option -labelwidget -default "" -configuremethod C-labelwidget;
    option -text        -default "" -configuremethod C-text;
    option -font        -default "" -configuremethod C-font;

    # ### ######### ###########################
    ## Public API. Construction

    constructor {args} {
	set wtype ttk::labelframe
	# Grab -style option for processing - do not pass through
	set idx [lsearch -exact $args "-style"]
	if {$idx != -1} {
	    set options(-style) [lindex $args [expr {$idx + 1}]]
	    set args [lreplace $args $idx [expr {$idx + 1}]]
	}
	set styles [list border whitespace separator]
	if {[lsearch -exact $styles $options(-style)] == -1} {
	    return -code error \
		"style must be one of: border, whitespace or separator"
	}
	parray options
	if {$options(-style) ne "border"} {
	    set wtype labelframe
	}
	installhull using $wtype
	if {$options(-style) ne "border"} {
	    set args [linsert $args 0 -relief flat -borderwidth 0]
	}
	if {$options(-style) eq "separator"} {
	    set sf [ttk::frame $win._labelwidget]
	    ttk::label $sf.lbl -text $options(-text)
	    ttk::separator $sf.sep -orient horizontal

	    grid $sf.lbl -row 0 -column 0 -stick sew
	    grid $sf.sep -row 0 -column 1 -stick sew -pady 2 -padx 2
	    grid columnconfigure $sf 1 -weight 1
	    grid rowconfigure    $sf 0 -weight 1

	    $hull configure -labelwidget $sf
	    bind $win <Configure> \
		[subst { if {"%W" eq "$win"} { $self SepSize } }]
	}
	$self configurelist $args
	return
    }

    # ### ######### ###########################
    ## Public API. Retrieve components

    method labelwidget {} {
	if {$options(-style) ne "separator"} {
	    return [$hull cget -labelwidget]
	} else {
	    return $win._labelwidget
	}
    }

    method SepSize {} {
	if {$options(-style) ne "separator"} { return 0 }

	set lw $win._labelwidget
	set rw  [winfo width $win]
	set lrw [winfo width $lw.lbl]
	set width [expr {$rw - $lrw - 10}]

	grid columnconfigure $lw 1 -minsize $width
    }

    # ### ######### ###########################
    ## Internal. Handling option changes.

    method C-labelwidget {option value} {
	if {$options(-style) ne "separator"} {
	    $hull configure -labelwidget $value
	} else {
	    set oldw [$hull cget -labelwidget]
	    if {$oldw ne ""} { grid forget $oldw }
	    if {$oldw eq $value || $value eq ""} { return }
	    grid $value -in $win._labelwidget -row 0 -column 0 -sticky ew
	}
	set options($option) $value
    }

    method C-text {option value} {
	if {$options(-style) ne "separator"} {
	    $hull configure -text $value
	} else {
	    $win._labelwidget.lbl configure -text $value
	}
	set options($option) $value
    }

    method C-font {option value} {
	if {$options(-style) ne "separator"} {
	    $hull configure -font $value
	} else {
	    $win._labelwidget.lbl configure -font $value
	}
	set options($option) $value
    }

    # ### ######### ###########################
}

# ### ######### ###########################
## Ready for use

package provide widget::superframe 1.0
