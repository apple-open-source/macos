# tk_getString.tcl --
#
#       A dialog which prompts for a string input
#
# Copyright (c) 2005    Aaron Faupell <afaupell@users.sourceforge.net>
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# RCS: @(#) $Id: tk_getString.tcl,v 1.11 2005/04/13 01:29:22 andreas_kupries Exp $

package require Tk
package provide getstring 0.1

namespace eval ::getstring {
    namespace export tk_getString
}

if {[tk windowingsystem] == "win32"} {
    option add *TkSDialog*Button.width -8 widgetDefault
    option add *TkSDialog*Button.padX 1m widgetDefault
} else {
    option add *TkSDialog.borderWidth 1 widgetDefault
    option add *TkSDialog*Button.width 5 widgetDefault
}
option add *TkSDialog*Entry.width 20 widgetDefault

proc ::getstring::tk_getString {w var text args} {
    array set options {
        -allowempty 0
        -entryoptions {}
        -title "Enter Information"
    }
    parseOpts options {{-allowempty boolean} {-entryoptions {}} {-geometry {}} \
                       {-title {}}} $args

    variable ::getstring::result
    upvar $var result
    catch {destroy $w}
    set focus [focus]
    set grab [grab current .]

    toplevel $w -relief raised -class TkSDialog
    wm title $w $options(-title)
    wm iconname $w $options(-title)
    wm protocol $w WM_DELETE_WINDOW {set ::getstring::result 0}
    wm transient $w [winfo toplevel [winfo parent $w]]
    wm resizable $w 1 0

    eval [list entry $w.entry] $options(-entryoptions)
    button $w.ok -text OK -default active -command {set ::getstring::result 1}
    button $w.cancel -text Cancel -command {set ::getstring::result 0}
    label $w.label -text $text

    grid $w.label -columnspan 2 -sticky ew -padx 5 -pady 3
    grid $w.entry -columnspan 2 -sticky ew -padx 5 -pady 3
    grid $w.ok $w.cancel -padx 4 -pady 7
    grid rowconfigure $w 2 -weight 1
    grid columnconfigure $w {0 1} -uniform 1 -weight 1

    bind $w <Return> [list $w.ok invoke]
    bind $w <Escape> [list $w.cancel invoke]
    bind $w <Destroy> {set ::getstring::result 0}
    if {!$options(-allowempty)} {
        bind $w.entry <KeyPress> [list after idle [list ::getstring::getStringEnable $w]]
        $w.ok configure -state disabled 
    }

    wm withdraw $w
    update idletasks
    focus -force $w.entry
    if {[info exists options(-geometry)]} {
        wm geometry $w $options(-geometry)
    } elseif {[winfo parent $w] == "."} {
        set x [expr {[winfo screenwidth $w]/2 - [winfo reqwidth $w]/2 - [winfo vrootx $w]}]
        set y [expr {[winfo screenheight $w]/2 - [winfo reqheight $w]/2 - [winfo vrooty $w]}]
        wm geom $w +$x+$y
    } else {
        set t [winfo toplevel [winfo parent $w]]
        set x [expr {[winfo width $t]/2 - [winfo reqwidth $w]/2 - [winfo vrootx $w]}]
        set y [expr {[winfo height $t]/2 - [winfo reqheight $w]/2 - [winfo vrooty $w]}]
        wm geom $w +$x+$y
    }
    wm deiconify $w
    grab $w

    tkwait variable ::getstring::result
    set result [$w.entry get]
    bind $w <Destroy> {}
    grab release $w
    destroy $w
    focus -force $focus
    if {$grab != ""} {grab $grab}
    update idletasks
    return $::getstring::result
}

proc ::getstring::parseOpts {var opts input} {
    upvar $var output
    for {set i 0} {$i < [llength $input]} {incr i} {
        for {set a 0} {$a < [llength $opts]} {incr a} {
           if {[lindex $opts $a 0] == [lindex $input $i]} { break }
        }
        if {$a == [llength $opts]} { error "unknown option [lindex $input $i]" }
        set opt [lindex $opts $a]
        if {[llength $opt] > 1} {
            foreach {opt type} $opt {break}
            if {[incr i] >= [llength $input]} { error "$opt requires an argument" }
            if {$type != "" && ![string is $type -strict [lindex $input $i]]} { error "$opt requires argument of type $type" }
            set output($opt) [lindex $input $i]
        } else {
            set output($opt) {}
        }
    }
}

proc ::getstring::getStringEnable {w} {
    if {![winfo exists $w.entry]} { return }
    if {[$w.entry get] != ""} {
        $w.ok configure -state normal
    } else {
        $w.ok configure -state disabled
    }
}
