# ipentry.tcl --
#
#       An entry widget for IP addresses.
#
# Copyright (c) 2003-2008 Aaron Faupell <afaupell@users.sourceforge.net>
# Copyright (c) 2008 Pat Thoyts <patthoyts@users.sourceforge.net>
#  
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: ipentry.tcl,v 1.19 2009/01/21 07:10:03 afaupell Exp $

package require Tk
package provide ipentry 0.3

namespace eval ::ipentry {
    namespace export ipentry ipentry6
    # copy all the bindings from Entry class to our own IPEntrybindtag class
    foreach x [bind Entry] {
        bind IPEntrybindtag $x [bind Entry $x]
    }
    # then replace certain keys we are interested in with our own
    bind IPEntrybindtag <KeyPress>         {::ipentry::keypress %W %K}
    bind IPEntrybindtag <BackSpace>        {::ipentry::backspace %W}
    bind IPEntrybindtag <period>           {::ipentry::dot %W}
    bind IPEntrybindtag <Key-Right>        {::ipentry::arrow %W %K}
    bind IPEntrybindtag <Key-Left>         {::ipentry::arrow %W %K}
    bind IPEntrybindtag <FocusIn>          {::ipentry::FocusIn %W}
    bind IPEntrybindtag <FocusOut>         {::ipentry::FocusOut %W}
    bind IPEntrybindtag <<Paste>>          {::ipentry::Paste %W CLIPBOARD}
    bind IPEntrybindtag <<PasteSelection>> {::ipentry::Paste %W PRIMARY}
    
    # copy all the bindings from IPEntrybindtag
    foreach x [bind IPEntrybindtag] {
        bind IPEntrybindtag6 $x [bind IPEntrybindtag $x]
    }
    # and replace certain keys with ip6 bindings
    bind IPEntrybindtag6 <KeyPress>         {::ipentry::keypress %W %K 6}
    bind IPEntrybindtag6 <colon>            {::ipentry::dot %W}
    bind IPEntrybindtag6 <period>           {}

    #if {[package vsatisfies [package provide Tk] 8.5]} {
    #     ttk::style layout IPEntryFrame {
    #         Entry.field -sticky news -border 1 -children {
    #             IPEntryFrame.padding -sticky news
    #         }
    #     }
    #     bind [winfo class .] <<ThemeChanged>> \
    #         [list +ttk::style layout IPEntryFrame \
    #              [ttk::style layout IPEntryFrame]]
    # }
}

# ipentry --
#
# main entry point - construct a new ipentry widget
#
# ARGS:
#       w       path name of widget to create
#
#               see ::ipentry::configure for args
#
# RETURNS:
#       the widget path name
#
proc ::ipentry::ipentry {w args} {
    upvar #0 [namespace current]::widget_$w state
    #set state(themed) [package vsatisfies [package provide Tk] 8.5]
    set state(themed) 0
    foreach {name val} $args {
        if {$name eq "-themed"} {
            set state(themed) $val
        }
    }
    if {$state(themed)} {
        ttk::frame $w -style IPEntryFrame -class IPEntry -takefocus 0
    } else {
        frame $w -relief sunken -class IPEntry;#-padx 5
    }
    foreach x {0 1 2 3} y {d1 d2 d3 d4} {
        #if {$state(themed)} {
        #    ttk::entry $w.$x -width 3 -justify center
        #    ttk::label $w.$y -text .
        #}
        entry $w.$x -borderwidth 0 -width 3 -highlightthickness 0 \
            -justify center -takefocus 0
        label $w.$y -borderwidth 0 -font [$w.$x cget -font] -width 1 -text . \
            -justify center -cursor [$w.$x cget -cursor] \
             -background [$w.$x cget -background] \
             -disabledforeground [$w.$x cget -disabledforeground]
        pack $w.$x $w.$y -side left
        bindtags $w.$x [list $w.$x IPEntrybindtag . all]
        bind $w.$y <Button-1> {::ipentry::dotclick %W %x}
    }
    destroy $w.d4
    $w.0 configure -takefocus 1
    if {$state(themed)} {
        pack configure $w.0 -padx {1 0} -pady 1
        pack configure $w.3 -padx {0 1} -pady 1 -fill x -expand 1
        $w.3 configure -justify left
    } else {
        $w configure -borderwidth [lindex [$w.0 configure -bd] 3]
            #-background [$w.0 cget -bg]
    }
    rename ::$w ::ipentry::_$w
    # redirect the widget name command to the widgetCommand dispatcher
    interp alias {} ::$w {} ::ipentry::widgetCommand $w
    bind $w <Destroy> [list ::ipentry::destroyWidget $w]
    if {[llength $args] > 0} {
        eval [list $w configure] $args
    }
    return $w
}

# ipentry --
#
# main entry point - construct a new ipentry6 widget
#
# ARGS:
#       w       path name of widget to create
#
#               see ::ipentry::configure for args
#
# RETURNS:
#       the widget path name
#
proc ::ipentry::ipentry6 {w args} {
    upvar #0 [namespace current]::widget_$w state
    #set state(themed) [package vsatisfies [package provide Tk] 8.5]
    set state(themed) 0
    foreach {name val} $args {
        if {$name eq "-themed"} {
            set state(themed) $val
        }
    }
    if {$state(themed)} {
        ttk::frame $w -style IPEntryFrame -class IPEntry -takefocus 0
    } else {
        frame $w -relief sunken -class IPEntry;#-padx 5
    }
    foreach x {0 1 2 3 4 5 6 7} y {d1 d2 d3 d4 d5 d6 d7 d8} {
        entry $w.$x -borderwidth 0 -width 4 -highlightthickness 0 \
            -justify center -takefocus 0
        label $w.$y -borderwidth 0 -font [$w.$x cget -font] -width 1 -text : \
            -justify center -cursor [$w.$x cget -cursor] \
            -background [$w.$x cget -background] \
            -disabledforeground [$w.$x cget -disabledforeground]
        pack $w.$x $w.$y -side left
        bindtags $w.$x [list $w.$x IPEntrybindtag6 . all]
        bind $w.$y <Button-1> {::ipentry::dotclick %W %x}
    }
    destroy $w.d8
    $w.0 configure -takefocus 1
    if {$state(themed)} {
        pack configure $w.0 -padx {1 0} -pady 1
        pack configure $w.7 -padx {0 1} -pady 1 -fill x -expand 1
        $w.7 configure -justify left
    } else {
        $w configure -borderwidth [lindex [$w.0 configure -bd] 3]
            #-background [$w.0 cget -bg]
    }
    rename ::$w ::ipentry::_$w
    # redirect the widget name command to the widgetCommand dispatcher
    interp alias {} ::$w {} ::ipentry::widgetCommand6 $w
    bind $w <Destroy> [list ::ipentry::destroyWidget $w]
    if {[llength $args] > 0} {
        eval [list $w configure] $args
    }
    return $w
}

# keypress --
#
# called every time a key is pressed in an ipentry widget
# used by both ipentry and ipentry6
#
# ARGS:
#       w       window argument (%W) from the event binding
#       key     the keysym (%K) from the event
#       type    empty string or "6" depending on the type of ipentry
#
# RETURNS:
#       nothing
#
proc ::ipentry::keypress {w key {type {}}} {
    if {![validate$type $w $key]} { return }
    # sel.first and sel.last throw an error if the selection isnt in $w
    catch {
        set insert [$w index insert]
        # if a key is pressed while there is a selection then delete the
        # selected chars
        if {([$w index sel.first] <= $insert) && ([$w index sel.last] >= $insert)} {
            $w delete sel.first sel.last
        }
    }
    $w insert insert $key
    ::ipentry::updateTextvar $w
}

# backspace --
#
# called when the Backspace key is pressed in an ipentry widget
# used by both ipentry and ipentry6
#
# try to act like a normal backspace except if the cursor is at index 0
# of one entry we need to move to the end of the preceding entry
#
# ARGS:
#       w       window argument (%W) from the event binding
#
# RETURNS:
#       nothing
#
proc ::ipentry::backspace {w} {
    if {[$w selection present]} {
        $w delete sel.first sel.last
    } else {
        if {[$w index insert] == 0} {
            set w [skip $w prev]
        }
        $w delete [expr {[$w index insert] - 1}]
    }
    ::ipentry::updateTextvar $w
}

# dot --
#
# called when the dot (Period) key is pressed in an ipentry widget
# used by both ipentry and ipentry6
#
# treat the current entry as done and move to the next entry field
#
# ARGS:
#       w       window argument (%W) from the event binding
#
# RETURNS:
#       nothing
#
proc ::ipentry::dot {w} {
    if {[string length [$w get]] > 0} {
        skip $w next 1
    }
    ::ipentry::updateTextvar $w
}

# FocusIn --
#
# called when the focus enters any of the child widgets of an ipentry
# used by both ipentry and ipentry6
#
# clear the selection of all child widgets other than the one with focus
#
# ARGS:
#       w       window argument (%W) from the event binding
#
# RETURNS:
#       nothing
#
proc ::ipentry::FocusIn {w} {
    set p [winfo parent $w]
    foreach x {0 1 2 3 4 5 6 7} {
        if {![winfo exists $p.$x]} { break }
        if {"$p.$x" != $w} {
            $p.$x selection clear
        }
    }
}

# FocusOut --
#
# called when the focus leaves any of the child widgets of an ipentry
# used by both ipentry and ipentry6
#
# dont allow a 0 in the first quad
#
# ARGS:
#       w       window argument (%W) from the event binding
#
# RETURNS:
#       nothing
#
proc ::ipentry::FocusOut {w} {
    set s [$w get]
    if {[string match {*.0} $w] && $s != "" && $s < 1} {
        $w delete 0 end
        $w insert end 1
        ::ipentry::updateTextvar $w
    }
    # trim off leading zeros
    if {[string length $s] > 1} {
        set n [string trimleft $s 0]
        if {$n eq ""} { set n 0 }
        if {![string equal $n $s]} {
            $w delete 0 end
            $w insert end $n
        }
    }
}

# Paste --
#
# called from the <<Paste>> virtual event
# used by ipentry only
#
# clear the selection of all child widgets other than the one with focus
#
# ARGS:
#       w       window argument (%W) from the event binding
#       sel     one of CLIPBOARD or PRIMARY
#
# RETURNS:
#       nothing
#
proc ::ipentry::Paste {w sel} {
    if {[catch {::tk::GetSelection $w $sel} paste]} { return }
    $w delete 0 end
    foreach char [split $paste {}] {
        # ignore everything except dots and digits
        if {![string match {[0123456789.]} $char]} { continue }
        if {$char != "."} {
            $w insert end $char
        }
        # if value is over 255 truncate it
        if {[$w get] > 255} {
            $w delete 0 end
            $w insert 0 255
        }
        # if char is a . then get the index of the current entry
        # and update $w to point to the next entry
        if {$char == "."} {
            set n [string index $w end]
            if { $n >= 3 } { return }
            set w [string trimright $w "0123"][expr {$n + 1}]
            $w delete 0 end
            continue
        }
    }
    ::ipentry::updateTextvar $w
}

# Paste6 --
#
# called from the <<Paste>> virtual event
# used by both ipentry6 only
#
# clear the selection of all child widgets other than the one with focus
#
# ARGS:
#       w       window argument (%W) from the event binding
#       sel     one of CLIPBOARD or PRIMARY
#
# RETURNS:
#       nothing
#
proc ::ipentry::Paste6 {w sel} {
    if {[catch {::tk::GetSelection $w $sel} paste]} { return }
    $w delete 0 end
    foreach char [split $paste {}] {
        # ignore everything except colons and hex digits
        if {![string match {[0123456789abcdefABCDEF:]} $char]} { continue }
        if {$char != ":"} {
            $w insert end $char
        }
        # if char is a : then get the index of the current entry
        # and update $w to point to the next entry
        if {$char == ":"} {
            set n [string index $w end]
            if { $n >= 7 } { return }
            set w [string trimright $w "01234567"][expr {$n + 1}]
            $w delete 0 end
            continue
        }
    }
    ::ipentry::updateTextvar $w
}

# dotclick --
#
# called when mouse button 1 is clicked on any of the label widgets
# used by both ipentry and ipentry6
#
# decide which side of the dot was clicked and put the focus and cursor
# in the correct entry
#
# ARGS:
#       w       window argument (%W) from the event binding
#
# RETURNS:
#       nothing
#
proc ::ipentry::dotclick {w x} {
    if {$x > ([winfo width $w] / 2)} {
        set w [winfo parent $w].[string index $w end]
        focus $w
        $w icursor 0
    } else {
        set w [winfo parent $w].[expr {[string index $w end] - 1}]
        focus $w
        $w icursor end
    }
}

# arrow --
#
# called when the left or right arrow keys are pressed in an ipentry
# used by both ipentry and ipentry6
#
# ARGS:
#       w       window argument (%W) from the event binding
#       key     one of Left or Right
#
# RETURNS:
#       nothing
#
proc ::ipentry::arrow {w key} {
    set i [$w index insert]
    set l [string length [$w get]]
    # move the icursor +1 or -1 position
    $w icursor [expr $i [string map {Right + Left -} $key] 1]
    $w selection clear
    # if we are moving right and the cursor is at the end, or the entry is empty
    if {$key == "Right" && ($i == $l || $l == 0)} {
        skip $w next
    } elseif {$key == "Left" && $i == 0} {
        skip $w prev
    }
}

# validate --
#
# called by keypress to validate the input
# used by ipentry only
#
# ARGS:
#       w       window argument (%W) from the event binding
#       key     the key pressed
#
# RETURNS:
#       a boolean indicating if the key is valid or not
#
proc ::ipentry::validate {w key} {
    if {![string match {[0123456789]} $key]} { return 0 }
    set curval [$w get]
    set insert [$w index insert]
    # dont allow more than a single 0 to be entered
    if {$curval == "0" && $key == "0"} { return 0 }
    if {[string length $curval] == 2} {
        set curval [join [linsert [split $curval {}] $insert $key] {}]
        if {$curval > 255} {
            $w delete 0 end
            $w insert 0 255
            $w selection range 0 end
            ::ipentry::updateTextvar $w
            return 0
        } elseif {$insert == 2} {
            skip $w next 1
        }
        return 1
    }
    if {[string length $curval] >= 3 && ![$w selection present]} {
        if {$insert == 3} { skip $w next 1 }
        return 0
    }
    return 1
}

# validate6 --
#
# called by keypress to validate the input
# used by ipentry6 only
#
# ARGS:
#       w       window argument (%W) from the event binding
#       key     the key pressed
#
# RETURNS:
#       a boolean indicating if the key is valid or not
#
proc ::ipentry::validate6 {w key} {
    if {![string is xdigit $key]} { return 0 }
    set curval 0x[$w get]
    set insert [$w index insert]
    # dont allow more than a single 0 to be entered
    if {$curval == "0" && $key == "0"} { return 0 }
    if {[string length $curval] == 5} {
        set curval [join [linsert [split $curval {}] $insert $key] {}]
        if {$insert == 3} {
            skip $w next 1
        }
        return 1
    }
    if {[string length $curval] >= 6 && ![$w selection present]} {
        if {$insert == 4} { skip $w next 1 }
        return 0
    }
    return 1
}

# skip --
#
# move the cursor to the previous or next entry widget
# used by both ipentry and ipentry6
#
# ARGS:
#       w       name of the current entry widget 
#       dir     direction to move, one of next or prev
#       sel     boolean indicating whether to select the digits in the next entry
#
# RETURNS:
#       the name of the widget with focus
#
proc ::ipentry::skip {w dir {sel 0}} {
    set n [string index $w end]
    if {$dir == "next"} {
        set next [string trimright $w "012345678"][expr {$n + 1}]
        if { ![winfo exists $next] } { return $w }
        focus $next
        if {$sel} {
            $next icursor 0
            $next selection range 0 end
        }
        return $next
    } else {
        if { $n <= 0 } { return $w }
        set prev [string trimright $w "012345678"][expr {$n - 1}]
        focus $prev
        $prev icursor end
        return $prev
    }
}

# _foreach --
#
# utility for the widget configure command
#
# perform a command on every subwidget of an ipentry frame
#
# ARGS:
#       w       name of the ipentry frame 
#       cmd     command to perform
#       type    one of empty, "entry", or "dot"
#
# RETURNS:
#       nothing
#
proc ::ipentry::_foreach {w cmd {type {}}} {
    if {$type == "" || $type == "entry"} {
        foreach x {0 1 2 3 4 5 6 7} {
            if {![winfo exists $w.$x]} { break }
            eval [list $w.$x] $cmd
        }
    }
    if {$type == "" || $type == "dot"} {
        foreach x {d1 d2 d3 d4 d5 d6 d7} {
            if {![winfo exists $w.$x]} { break }
            eval [list $w.$x] $cmd
        }
     }
}

# cget --
#
# handle the widgetName cget subcommand
# used by both ipentry and ipentry6
#
# ARGS:
#       w       name of the ipentry widget 
#       cmd     name of a configuration option
#
# RETURNS:
#       the value of the requested option
#
proc ::ipentry::cget {w cmd} {
    upvar #0 [namespace current]::widget_$w state
    switch -exact -- $cmd {
        -bd -
        -borderwidth -
        -relief {
            # for bd and relief return the value from the container frame
            if {!$state(themed)} {
                return [::ipentry::_$w cget $cmd]
            }
        }
        -textvariable {
            if {[info exists ::ipentry::textvars($w)]} {
                return $::ipentry::textvars($w)
            }
            return {}
        }
        -themed { return $state(themed) }
        -takefocus { return 0 }
        default {
            # for all other commands return the value from the first entry
            return [$w.0 cget $cmd]
        }
    }
}

# configure --
#
# handle the widgetName configure subcommand
# used by both ipentry and ipentry6
#
# ARGS:
#       w       name of the ipentry widget 
#       args    name/value pairs of configuration options
#
# RETURNS:
#       nothing
#
proc ::ipentry::configure {w args} {
    upvar #0 [namespace current]::widget_$w Priv
    while {[set cmd [lindex $args 0]] != ""} {
        switch -exact -- $cmd {
            -state {
                set state [lindex $args 1]
                if {$state == "disabled"} {
                    _foreach $w [list configure -state disabled]
                    if {[set dbg [$w.0 cget -disabledbackground]] == ""} {
                          set dbg [$w.0 cget -bg]
                    }
                    _foreach $w [list configure -bg $dbg] dot
                    if {$Priv(themed)} {
                        ::ipentry::_$w state disabled
                    } else {
                        ::ipentry::_$w configure -background $dbg
                    }
                } elseif {$state == "normal"} {
                    _foreach $w [list configure -state normal]
                    _foreach $w [list configure -bg [$w.0 cget -bg]] dot
                    if {$Priv(themed)} {
                        ::ipentry::_$w state {!readonly !disabled}
                    } else {
                         ::ipentry::_$w configure -background [$w.0 cget -bg]
                    }
                } elseif {$state == "readonly"} {
                    _foreach $w [list configure -state readonly] entry
                    if {[set robg [$w.0 cget -readonlybackground]] == ""} {
                        set robg [$w.0 cget -bg]
                    }
                    _foreach $w [list configure -bg $robg] dot
                    if {$Priv(themed)} {
                        ::ipentry::_$w state !readonly
                    } else {
                        ::ipentry::_$w configure -background $robg
                    }
                }
                set args [lrange $args 2 end]
            }
            -bg - -background {
                set bg [lindex $args 1]
                _foreach $w [list configure -background $bg]
                if {!$Priv(themed)} {
                    ::ipentry::_$w configure -background $bg
                }
                set args [lrange $args 2 end]
            }
            -disabledforeground {
                _foreach $w [list configure -disabledforeground [lindex $args 1]]
                set args [lrange $args 2 end]
            }
            -font -
            -fg - -foreground {
                _foreach $w [list configure $cmd [lindex $args 1]]
                set args [lrange $args 2 end]
            }
            -bd - -borderwidth -
            -relief -
            -highlightcolor -
            -highlightbackground -
            -highlightthickness {
                _$w configure $cmd [lindex $args 1]
                set args [lrange $args 2 end]
            }
            -readonlybackground -
            -disabledbackground -
            -selectforeground   -
            -selectbackground   -
            -selectborderwidth  -
            -insertbackground {
                _foreach $w [list configure $cmd [lindex $args 1]] entry
                set args [lrange $args 2 end]
            }
            -themed {
                # ignored - only used in widget creation
            }
            -textvariable {
                set name [lindex $args 1]
                upvar #0 $name var
                #if {![string match ::* $name]} { set name ::$name }
                if {[info exists ::ipentry::textvars($w)]} {
                    set trace [trace info variable var]
                    trace remove variable var [lindex $trace 0 0] [lindex $trace 0 1]
                }
                set ::ipentry::textvars($w) $name
                if {![info exists var]} { set var "" }
                ::ipentry::traceFired $w $name {} write
                if {[winfo exists $w.4]} {
                    trace add variable var {write unset} [list ::ipentry::traceFired6 $w]
                } else {
                    trace add variable var {write unset} [list ::ipentry::traceFired $w]
                }
                set args [lrange $args 2 end]
            }
            default {
                error "unknown option \"[lindex $args 0]\""
            }
        }
    }
}

# destroyWidget --
#
# bound to the <Destroy> event
# used by both ipentry and ipentry6
#
# ARGS:
#       w       name of the ipentry widget 
#
# RETURNS:
#       nothing
#
proc ::ipentry::destroyWidget {w} {
    upvar #0 [namespace current]::widget_$w state
    if {[info exists ::ipentry::textvars($w)]} {
        upvar #0 $::ipentry::textvars($w) var
        set trace [trace info variable var]
        trace remove variable var [lindex $trace 0 0] [lindex $trace 0 1]
    }
    rename $w {}
    unset state
}

# traceFired --
#
# called by the variable trace on the ipentry textvariable
# used by ipentry only
#
# ARGS:
#       w       name of the ipentry widget 
#       varname name of the variable being traced
#       key     array index of the variable
#       op      operation performed on the variable, read/write/unset
#
# RETURNS:
#       nothing
#
proc ::ipentry::traceFired {w name key op} {
    upvar #0 $name var
    if {[info level] > 1} {
        set caller [lindex [info level -1] 0]
        if {$caller == "::ipentry::updateTextvar" || $caller == "::ipentry::traceFired"} { return }
    }
    if {$op == "write"} {
        _insert $w [split $var .]
        set val [string trim [join [$w get] .] .]
        # allow a dot at the end, but only if we have less than 3 already
        if {[string index $var end] == "." && [regexp -all {\.+} $var] <= 3} { append val . }
        if {$val eq $var} return
        after 0 [list set $name $val]
        set var $val
    } elseif {$op == "unset"} {
        ::ipentry::updateTextvar $w.0
        trace add variable var {write unset} [list ipentry::traceFired $w]
    }
}

# traceFired6 --
#
# called by the variable trace on the ipentry textvariable
# used by ipentry6 only
#
# ARGS:
#       w       name of the ipentry widget 
#       varname name of the variable being traced
#       key     array index of the variable
#       op      operation performed on the variable, read/write/unset
#
# RETURNS:
#       nothing
#
proc ::ipentry::traceFired6 {w name key op} {
    upvar #0 $name var
    if {[info level] > 1} {
        set caller [lindex [info level -1] 0]
        if {$caller == "::ipentry::updateTextvar" || $caller == "::ipentry::traceFired6"} { return }
    }
    if {$op == "write"} {
        _insert6 $w [split $var :]
        set val [string trim [join [$w get] :] :]
        # allow a dot at the end, but only if we have less than 3 already
        if {[string index $var end] == ":" && [regexp -all {\:+} $var] <= 7} { append val : }
        if {$val eq $var} return
        after 0 [list set $name $val]
        set var $val
    } elseif {$op == "unset"} {
        ::ipentry::updateTextvar $w.0
        trace add variable var {write unset} [list ipentry::traceFired6 $w]
    }
}

# updateTextvar --
#
# called by all procs which change the value of the ipentry
# used by both ipentry and ipentry6
#
# update the textvariable if it exists with the new value
#
# ARGS:
#       w       name of the ipentry widget 
#
# RETURNS:
#       nothing
#
proc ::ipentry::updateTextvar {w} {
    set p [winfo parent $w]
    if {![info exists ::ipentry::textvars($p)]} { return }
    set c [$p.d1 cget -text]
    set val [string trim [join [$p get] $c] $c]
    upvar #0 $::ipentry::textvars($p) var
    if {[info exists var] && $var == $val} { return }
    set var $val
}

# _insert --
#
# called by the variable trace on the ipentry textvariable and widget insert cmd
# used by ipentry only
#
# ARGS:
#       w       name of an ipentry widget 
#       val     a list of 4 values to be inserted into the ipentry
#
# RETURNS:
#       nothing
#
proc ::ipentry::_insert {w val} {
    foreach x {0 1 2 3} {
        set n [lindex $val $x]
        if {$n != ""} {
            if {![string is integer -strict $n]} {
                #error "cannot insert non-numeric arguments"
                return
            }
            if {$n > 255} { set n 255 }
            if {$n <= 0}  { set n 0 }
            if {$x == 0 && $n < 1} { set n 1 }
        }
        $w.$x delete 0 end
        $w.$x insert 0 $n
    }
}

# _insert6 --
#
# called by the variable trace on the ipentry textvariable and widget insert cmd
# used by both ipentry6 only
#
# ARGS:
#       w       name of an ipentry widget 
#       val     a list of 8 values to be inserted into the ipentry
#
# RETURNS:
#       nothing
#
proc ::ipentry::_insert6 {w val} {
    foreach x {0 1 2 3 4 5 6 7} {
        set n [lindex $val $x]
        if {![string is xdigit $n]} {
              #error "cannot insert non-hex arguments"
              return
        }
        if {$n != "" } {
            if "$x == 0 && 0x$n < 1" { set n 1 }
            if "0x$n > 0xffff" { set n ffff }
        }
        $w.$x delete 0 end
        $w.$x insert 0 $n
    }
}

# widgetCommand --
#
# handle the widgetName command
# used by ipentry, with some commands passed through from widgetCommand6
#
# ARGS:
#       w       name of the ipentry widget 
#       cmd     the subcommand
#       args    arguments to the subcommand
#
# RETURNS:
#       the results of the invoked subcommand
#
proc ::ipentry::widgetCommand {w cmd args} {
    upvar #0 [namespace current]::widget_$w state
    switch -exact -- $cmd {
        get {
            # return the 4 entry values as a list
            foreach x {0 1 2 3 4 5 6 7} {
                if {![winfo exists $w.$x]} { break }
                set s [$w.$x get]
                if {[string length $s] > 1} {
                    set s [string trimleft $s 0]
                    if {$s == ""} { set s 0 }
                }
                
                lappend r $s
            }
            return $r
        }
        insert {
            _insert $w [join $args]
            ::ipentry::updateTextvar $w.3
        }
        icursor {
            if {![string match $w.* [focus]]} { return }
            set i [lindex $args 0]
            if {![string is integer -strict $i]} { error "argument must be an integer" }
            set s [expr {$i / 4}]
            focus $w.$s
            $w.$s icursor [expr {$i % 4}]
        }
        complete {
            foreach x {0 1 2 3 4 5 6 7} {
                if {![winfo exists $w.$x]} { break }
                if {[$w.$x get] == ""} { return 0 }
            }
            return 1
        }
        configure {
            eval [list ::ipentry::configure $w] $args
        }
        cget {
            return [::ipentry::cget $w [lindex $args 0]]
        }
        default {
            error "bad option \"$cmd\": must be get, insert, complete, cget, or configure"
        }
    }
}

# widgetCommand6 --
#
# handle the widgetName command for ipentry6 widgets
# most subcommands are passed through to widgetCommand by the default case
#
# ARGS:
#       w       name of the ipentry widget 
#       cmd     the subcommand
#       args    arguments to the subcommand
#
# RETURNS:
#       the results of the invoked subcommand
#
proc ::ipentry::widgetCommand6 {w cmd args} {
    upvar #0 [namespace current]::widget_$w state
    switch -exact -- $cmd {
        insert {
            _insert6 $w [join $args]
            ::ipentry::updateTextvar $w.7
        }
        icursor {
            if {![string match $w.* [focus]]} { return }
            set i [lindex $args 0]
            if {![string is integer -strict $i]} { error "argument must be am integer" }
            set s [expr {$i / 8}]
            focus $w.$s
            $w.$s icursor [expr {$i % 8}]
        }
        default {
            return [eval [list ::ipentry::widgetCommand $w $cmd] $args]
        }
    }
}
