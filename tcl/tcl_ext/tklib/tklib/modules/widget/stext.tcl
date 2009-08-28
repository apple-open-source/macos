# -*- tcl -*-
#
# stext.tcl -
#
#	Scrolled text widget.  A blend of the text widget with the
#	scrolledwindow.
#
#	While I do not recommend making scrolledXXX versions of widgets
#	(instead, use the 3 line wrapper), this is an example of how one
#	would do that.
#
# RCS: @(#) $Id: stext.tcl,v 1.1 2007/06/20 23:53:51 hobbs Exp $
#

if 0 {
    # Samples
    package require widget::scrolledwindow
    #set sw [widget::scrolledwindow .sw -scrollbar vertical]
    #set text [text .sw.text -wrap word]
    #$sw setwidget $text
    #pack $sw -fill both -expand 1

    proc test {{root .f}} {
	destroy $root
	set f   [ttk::frame $root]
	set lbl [ttk::label $f.lbl -text "Scrolled Text snidget:" -anchor w]
	set st  [widget::scrolledtext $f.sw -borderwidth 1 -relief sunken]
	pack $lbl -fill x
	pack $st -fill both -expand 1
	pack $f -fill both -expand 1 -padx 4 -pady 4
    }
}

###

package require widget
package require widget::scrolledwindow

snit::widgetadaptor widget::scrolledtext {
    # based on widget::scrolledwindow
    component text

    delegate option * to text
    delegate method * to text

    delegate option -scrollbar to hull
    delegate option -auto to hull
    delegate option -sides to hull
    delegate option -borderwidth to hull
    delegate option -relief to hull

    constructor args {
	# You want the outer scrolledwindow to display bd/relief
	installhull using widget::scrolledwindow
	install text using text $win.text \
	    -borderwidth 0 -relief flat -highlightthickness 1
	$hull setwidget $text

	# Enable with the bits below to have a fancy override for text
	# widget commands (like insert/delete)
	#rename $text ${selfns}::$text.
	#interp alias {} $text {} {*}[mymethod _text]

	# Use Ttk TraverseIn event to handle megawidget focus properly
	bind $win <<TraverseIn>> [list focus -force $text]

	$self configurelist $args
    }

    #destructor { rename $text {} }
    #method _text {cmd args} {
    #	# Here you could override insert or delete ...
    #	uplevel 1 [linsert $args 0 ${selfns}::$text. $cmd]
    #}
}

package provide widget::scrolledtext 1.0
