#!/bin/sh
# -*- tcl -*- \
exec tclsh8.3 "$0" "$@"

# flatten.tcl --
#
#	Parse a DTD, resolve all external entities, parameter
#	entities and conditional sections and save the result.
#
# Copyright (c) 2000 Zveno Pty Ltd
# http://www.zveno.com/
# 
# Zveno makes this software and all associated data and documentation
# ('Software') available free of charge for any purpose.
# Copies may be made of this Software but all of this notice must be included
# on any copy.
# 
# The Software was developed for research purposes and Zveno does not warrant
# that it is error free or fit for any purpose.  Zveno disclaims any
# liability for all claims, expenses, losses, damages and costs any user may
# incur as a result of using, copying or modifying the Software.
#
# CVS: $Id: flatten.tcl,v 1.2 2000/05/19 23:56:20 steve Exp $

# Allow the script to work from the source directory
set auto_path [linsert $auto_path 0 [file dirname [file dirname [file join [pwd] [info script]]]]]

# We need TclXML
package require xml 2.0

# Process --
#
#	Parse a XML document or DTD and emit result
#
# Arguments:
#	data	XML text
#	type	"xml" or "dtd"
#	out	output channel
#	args	configration options
#
# Results:
#	Data is parsed and flattened DTD written to output channel

proc Process {data type out args} {
    global elementDeclCount PEDeclCount AttListDeclCount CommentCount
    global config
    set elementDeclCount [set PEDeclCount [set AttListDeclCount [set CommentCount 0]]]

    # Create the parser object.
    # We want to use the Tcl-only parser for this application,
    # because it can resolve entities without doing full
    # validation.

    set parser [eval ::xml::parser \
	    -elementstartcommand ElementStart \
	    -validate 1 \
	    $args \
	    ]

    if {$config(wantElementDecls)} {
	$parser configure -elementdeclcommand [list ElementDeclaration $out]
    }
    if {$config(wantPEDecls)} {
	$parser configure -parameterentitydeclcommand [list PEDecl $out]
    }
    if {$config(wantAttListDecls)} {
	$parser configure -attlistdeclcommand [list AttListDecl $out]
    }
    if {$config(wantComments)} {
	$parser configure -commentcommand [list Comment $out]
    }

    switch $type {
	xml {
	    # Proceed with normal parsing method
	    $parser parse $data
	}

	dtd {
	    # Use the DTD parsing method instead
	    $parser parse $data -dtdsubset external
	}
    }

    # Clean up parser object
    #$parser free
    #rename $parser {}

    return {}
}

# ElementStart --
#
#	Callback for the start of an element.
#
# Arguments:
#	name	tag name
#	attlist	attribute list
#	args	other information
#
# Results:
#	Returns break error code, since we don't
#	care about the document instance, only the DTD

proc ElementStart {name attlist args} {
    return -code break
}

# ElementDeclaration --
#
#	Callback for an element declaration.
#
# Arguments:
#	out	output channel
#	name	tag name
#	cmodel	content model specification
#
# Results:
#	Writes element declaration to output channel

proc ElementDeclaration {out name cmodel} {
    global elementDeclCount
    incr elementDeclCount

    regsub -all "\[ \t\n\r\]+" $cmodel { } cmodel
    puts $out "<!ELEMENT $name $cmodel>"

    return {}
}

# PEDecl --
#
#	Callback for a parameter entity declaration.
#
# Arguments:
#	out	output channel
#	name	PE name
#	repl	replacement text
#
# Results:
#	Writes info to stderr

proc PEDecl {out name repl args} {
    global PEDeclCount
    incr PEDeclCount

    if {[llength $args]} {
	puts $out "<!ENTITY % $name PUBLIC \"[lindex $args 0]\" \"$repl\">"
    } else {
	puts $out "<!ENTITY % $name \"[string trim $repl]\">"
    }

    return {}
}

# AttListDecl --
#
#	Callback for an attribute list declaration.
#
# Arguments:
#	out	output channel
#	name	element name
#	attname	attribute name
#	type	attribute definition type
#	dflt	default type
#	dfltval	default value
#
# Results:
#	Writes info to stderr

proc AttListDecl {out name attname type dflt dfltval} {
    global AttListDeclCount
    incr AttListDeclCount

    puts $out "<!ATTLIST $name $attname $type $dflt $dfltval>"

    return {}
}

# Comment --
#
#	Callback for a comment.
#
# Arguments:
#	out	output channel
#	data	comment data
#
# Results:
#	Writes info to stderr

proc Comment {out data} {
    global CommentCount
    incr CommentCount

    puts $out "<!--${data}-->"

    return {}
}

# Open --
#
#	Manage opening document in GUI environment
#
# Arguments:
#	None
#
# Results:
#	XML or DTD document opened and parsed

proc Open {} {
    global currentDir status

    set filename [tk_getOpenFile -parent . -title "Open Document" -initialdir $currentDir -defaultextension ".xml" -filetypes {
	{{XML Documents}	{.xml}	}
	{{DTD Files}		{.dtd}	}
	{{All File}		*	}
    }]
    if {![string length $filename]} {
	return {}
    }

    set currentDir [file dirname $filename]
    set savename [file join [file rootname $filename].dtd]
    set savename [tk_getSaveFile -parent . -title "Save DTD" -initialdir $currentDir -initialfile $savename -defaultextension ".dtd" -filetypes {
	{{XML Documents}	{.xml}	}
	{{DTD Files}		{.dtd}	}
	{{All File}		*	}
    }]
    if {![string length $savename]} {
	return {}
    }

    set status Processing
    set oldcursor [. cget -cursor]
    . configure -cursor watch
    grab .elementDecls
    update

    set ch [open $filename]
    set out [open $savename w]
    if {[catch {Process [read $ch] [expr {[file extension $filename] == ".dtd" ? "dtd" : "xml"}] $out -baseurl file://[file join [pwd] $filename]} err]} {

	tk_messageBox -message [format [mc {Unable to process document "%s" due to "%s"}] $filename $err] -icon error -default ok -parent . -type ok
    } else {
	tk_messageBox -message [mc "DTD Saved OK"] -icon info -default ok -parent . -type ok
    }

    close $ch
    close $out
    set status {}
    grab release .elementDecls
    . configure -cursor $oldcursor
    return {}
}

### Main script

# Initialize message catalog, in case it is used
package require msgcat
namespace import msgcat::mc
catch {::msgcat::mcload [file join [file dirname [info script]] msgs]}

# Usage: flatten.tcl file1 file2 ...
# "-" reads input from stdin
# No arguments - Tk means read from stdin
# Files read from stdin assumed to be XML documents
# When given files to read, all output goes to stdout
# No arguments + Tk means use GUI

switch [llength $argv] {
    0 {
	if {![catch {package require Tk}]} {
	    # Create a nice little GUI
	    array set config {wantElementDecls 1 wantPEDecls 0 wantAttlistDecls 1 wantComments 0}
	    checkbutton .wantElementDecls -variable config(wantElementDecls)
	    label .elementDeclLabel -text [mc "Element declarations:"]
	    label .elementDecls -textvariable elementDeclCount
	    checkbutton .wantPEDecls -variable config(wantPEDecls)
	    label .peDeclLabel -text [mc "PE declarations:"]
	    label .peDecls -textvariable PEDeclCount
	    checkbutton .wantAttListDecls -variable config(wantAttListDecls)
	    label .attListDeclLabel -text [mc "Atttribute List declarations:"]
	    label .attListDecls -textvariable AttListDeclCount
	    checkbutton .wantComments -variable config(wantComments)
	    label .commentLabel -text [mc "Comments:"]
	    label .comments -textvariable CommentCount
	    label .status -textvariable status -foreground red
	    grid .wantElementDecls .elementDeclLabel .elementDecls
	    grid .wantPEDecls .peDeclLabel .peDecls
	    grid .wantAttListDecls .attListDeclLabel .attListDecls
	    grid .wantComments .commentLabel .comments
	    grid .status - -
	    . configure -menu .menu
	    menu .menu -tearoff 0
	    .menu add cascade -label [mc File] -menu .menu.file
	    menu .menu.file
	    .menu.file add command -label [mc Open] -command Open
	    .menu.file add separator
	    .menu.file add command -label [mc Quit] -command exit
	    set currentDir [pwd]
	} else {
	    Process [read stdin] xml stdout
	}
    }
    default {
	foreach filename $argv {
	    if {$filename == "-"} {
		Process [read stdin] xml stdout
	    } else {
		set ch [open $filename]
		Process [read $ch] [expr {[file extension $filename] == ".dtd" ? "dtd" : "xml"}] stdout -baseurl file://[file join [pwd] $filename]
		close $ch
	    }
	}
    }
}
