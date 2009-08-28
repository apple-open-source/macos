#!/bin/sh
# \
exec wish "$0" "$@"

# tkxmllint --
#
#	Simple GUI for xmllint-style processing of XML documents
#
# Copyright (c) 2003 Zveno
# http://www.zveno.com/
#
# Insert std disclaimer here
#
# $Id: tkxmllint.tcl,v 1.1 2003/03/09 11:12:49 balls Exp $

# Global initialisation

package require dom
package require dom::libxml2

package require msgcat
namespace import ::msgcat::mc

package require uri

# Init --
#
#	Create the GUI
#
# Arguments:
#	win	toplevel window
#
# Results:
#	Tk widgets created

proc Init win {
    upvar \#0 State$win state

    set w [expr {$win == "." ? {} : $win}]

    set state(url) {}

    wm title $win "Tk XML Lint"

    menu $w.menu -tearoff 0
    $win configure -menu $w.menu
    $w.menu add cascade -label [mc File] -menu $w.menu.file
    menu $w.menu.file -tearoff 1
    $w.menu.file add command -label [mc {New Window}] -command NewWindow
    $w.menu.file add separator
    $w.menu.file add command -label [mc Quit] -command {destroy .}
    # TODO: Help menu

    frame $w.controls
    grid $w.controls -row 0 -column 0 -sticky ew
    button $w.controls.check -text [mc Check] -command [list Check $win]
    # TODO: add a nice icon
    grid $w.controls.check -row 0 -column 0 -sticky w
    grid columnconfigure $w.controls 0 -weight 1

    labelframe $w.doc -text [mc Document]
    grid $w.doc -row 1 -column 0 -sticky ew
    label $w.doc.url -text [mc URL:]
    entry $w.doc.urlentry -width 60 -textvariable State${win}(url)
    button $w.doc.browse -text [mc Browse] -command [list Browse $win]
    grid $w.doc.url -row 0 -column 0 -sticky w
    grid $w.doc.urlentry -row 0 -column 1 -sticky ew
    grid $w.doc.browse -row 0 -column 2 -sticky e
    grid columnconfigure $w.doc 1 -weight 1

    labelframe $w.options -text [mc Options]
    grid $w.options -row 2 -column 0 -sticky ew
    checkbutton $w.options.noout -text [mc {Display document}] -variable State${win}(display)
    label $w.options.validate -text [mc Validate]
    radiobutton $w.options.novalidate -text [mc no] -variable State${win}(validate) -value no
    radiobutton $w.options.validatedoc -text [mc yes] -variable State${win}(validate) -value yes
    set state(validate) no
    checkbutton $w.options.timing -text [mc {Display timing}] -variable State${win}(timing)
    checkbutton $w.options.xinclude -text [mc XInclude] -variable State${win}(xinclude)
    grid $w.options.validate -row 0 -column 0 -sticky w
    grid $w.options.novalidate -row 0 -column 1 -sticky w
    grid $w.options.validatedoc -row 1 -column 1 -sticky w
    grid $w.options.noout -row 0 -column 2 -sticky w
    grid $w.options.timing -row 1 -column 2 -sticky w
    grid $w.options.xinclude -row 2 -column 2 -sticky w
    grid columnconfigure $w.options 2 -weight 1

    labelframe $w.messages -text [mc Messages]
    grid $w.messages -row 3 -column 0 -sticky news
    text $w.messages.log -wrap none \
	-state disabled \
	-xscrollcommand [list $w.messages.xscroll set] \
	-yscrollcommand [list $w.messages.yscroll set]
    scrollbar $w.messages.xscroll -orient horizontal \
	-command [list $w.messages.log xview]
    scrollbar $w.messages.yscroll -orient vertical \
	-command [list $w.messages.log yview]
    grid $w.messages.log -row 0 -column 0 -sticky news
    grid $w.messages.yscroll -row 0 -column 1 -sticky ns
    grid $w.messages.xscroll -row 1 -column 0 -sticky ew
    grid rowconfigure $w.messages 0 -weight 1
    grid columnconfigure $w.messages 0 -weight 1

    frame $w.feedback
    grid $w.feedback -row 4 -column 0 -sticky ew
    label $w.feedback.msg -textvariable State${win}(feedback)
    canvas $w.feedback.progress -width 100 -height 25
    grid $w.feedback.progress -row 0 -column 0
    grid $w.feedback.msg -row 0 -column 1 -sticky ew

    grid rowconfigure $win 3 -weight 1
    grid columnconfigure $win 0 -weight 1

    return {}
}

# NewWindow --
#
#	Create another toplevel window
#
# Arguments:
#	None
#
# Results:
#	Tk toplevel created and initialised

proc NewWindow {} {
    global counter

    Init [toplevel .top[Incr counter]]

    return {}
}

# Browse --
#
#	Choose a file
#
# Arguments:
#	win	toplevel window
#
# Results:
#	Current file is set

proc Browse win {
    upvar \#0 State$win state

    set w [expr {$win == "." ? {} : $win}]

    set fname [tk_getOpenFile -parent $win -title "Select XML Document"]
    if {![string length $fname]} {
	return {}
    }

    set state(url) file://$fname

    return {}
}

# Check --
#
#	Parse the given document and display report
#
# Arguments:
#	win	toplevel window
#
# Results:
#	Document read into memory, parsed and report displayed

proc Check win {
    upvar \#0 State$win state

    set w [expr {$win == "." ? {} : $win}]

    set state(url) [$w.doc.urlentry get]

    if {[catch {uri::split $state(url)} spliturl]} {
	# Try the URL as a pathname
	set fname $state(url)
	set state(url) file://$state(url)
    } else {
	array set urlarray $spliturl
	switch -- $urlarray(scheme) {
	    http {
		tk_messageBox -message "http URLs are not supported yet" -parent $win -type ok -icon warning
		return {}
	    }
	    file {
		set fname $urlarray(path)
	    }
	    default {
		tk_messageBox -message "\"$urlarray(scheme)\" type URLs are not supported" -parent $win -type ok -icon warning
		return {}
	    }
	}
    }

    Log clear $win
    set time(start) [clock clicks -milliseconds]

    Feedback $win [mc "Opening $fname"]
    if {[catch {open $fname} ch]} {
	tk_messageBox -message "unable to open document \"$fname\" due to \"$ch\"" -parent $win -type ok -icon error
	return {}
    }
    set time(open) [clock clicks -milliseconds]
    Log timing $win "Opening document took [expr $time(open) - $time(start)]ms\n"

    Feedback $win [mc "Reading document"]
    set xml [read $ch]
    close $ch
    set time(read) [clock clicks -milliseconds]
    Log timing $win "Reading document took [expr $time(read) - $time(open)]ms\n"

    Feedback $win [mc "Parsing XML"]
    if {[catch {dom::parse $xml -baseuri $state(url)} doc]} {
	Log add $win $doc
    }
    set time(parse) [clock clicks -milliseconds]
    Log timing $win "Parsing document took [expr $time(parse) - $time(read)]ms\n"
    set time(last) $time(parse)

    if {$state(xinclude)} {
	Feedback $win [mc "XInclude processing"]
	if {[catch {dom::xinclude $doc} msg]} {
	    Log add $win $msg
	    Feedback $win [mc "XInclude processing failed"]
	    after 2000 [list Feedback $win {}]
	}
	set time(xinclude) [clock clicks -milliseconds]
	Log timing $win "XInclude took [expr $time(xinclude) - $time(last)]ms\n"
	set time(last) $time(xinclude)
    }

    if {$state(validate)} {
	Feedback $win [mc "Validating document"]
	if {[catch {dom::validate $doc} msg]} {
	    Feedback $win [mc "Document is not valid"]
	}
	Log add $win $msg
	set time(validate) [clock clicks -milliseconds]
	Log timing $win "Validation took [expr $time(validate) - $time(last)]ms\n"
	set time(last) $time(validate)
    }

    if {$state(display)} {
	Log add $win [dom::serialize $doc]
	set time(serialize) [clock clicks -milliseconds]
	Log timing $win "Displaying document took [expr $time(serialize) - $time(last)]ms\n"
	set time(last) $time(serialize)
    }

    Feedback $win [mc "Processing completed"]
    after 2000 [list Feedback $win {}]

    dom::destroy $doc
    set time(destroy) [clock clicks -milliseconds]
    Log timing $win "Freeing took [expr $time(destroy) - $time(last)]ms\n"

    Log timing $win "Total time: [expr $time(destroy) - $time(start)]ms\n"

    return {}
}

# Log -- Manage the log window

proc Log {method win args} {
    upvar \#0 State$win state

    set w [expr {$win == "." ? {} : $win}]

    switch -- $method {
	clear {
	    $w.messages.log configure -state normal
	    $w.messages.log delete 1.0 end
	    $w.messages.log configure -state disabled
	}
	add {
	    $w.messages.log configure -state normal
	    $w.messages.log insert end [lindex $args 0]
	    $w.messages.log configure -state disabled
	}
	timing {
	    if {$state(timing)} {
		$w.messages.log configure -state normal
		$w.messages.log insert end [lindex $args 0]
		$w.messages.log configure -state disabled
	    }
	}
	default {
	    return -code error "unknown method \"$method\""
	}
    }

    return {}
}

# Feedback -- Manage the feedback widget

proc Feedback {win msg} {
    upvar \#0 State$win state

    set state(feedback) $msg
    update

    return {}
}

# Incr -- utility to increment a variable, handling non-existance

proc Incr var {
    upvar $var v
    if {[info exists v]} {
	incr v
    } else {
	set v 1
    }

    return $v
}

Init .
