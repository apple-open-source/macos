# ipentry.tcl --
#
#       An entry widget for IP addresses.
#
# Copyright (c) 2003    Aaron Faupell <afaupell@users.sourceforge.net>
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: ipentry.tcl,v 1.3 2003/08/02 21:01:39 afaupell Exp $

package provide ipentry 0.1

namespace eval ::ipentry {
    namespace export ipentry
    foreach x [bind Entry] {
        bind IPEntrybindtag $x [bind Entry $x]
    }
    bind IPEntrybindtag <KeyPress>         {::ipentry::keypress %W %K}
    bind IPEntrybindtag <BackSpace>        {::ipentry::backspace %W}
    bind IPEntrybindtag <period>           {::ipentry::dot %W}
    bind IPEntrybindtag <Key-Right>        {::ipentry::arrow %W %K}
    bind IPEntrybindtag <Key-Left>         {::ipentry::arrow %W %K}
    bind IPEntrybindtag <FocusIn>          {::ipentry::FocusIn %W}
    bind IPEntrybindtag <FocusOut>         {::ipentry::FocusOut %W}
    bind IPEntrybindtag <<Paste>>          {::ipentry::Paste %W CLIPBOARD}
    bind IPEntrybindtag <<PasteSelection>> {::ipentry::Paste %W PRIMARY}
}

proc ::ipentry::ipentry {w args} {
    frame $w -bd 2 -relief sunken -class IPEntry
    foreach x {0 1 2 3} y {d1 d2 d3 d4} {
        entry $w.$x -bd 0 -width 3 -highlightthickness 0 -justify center
        label $w.$y -bd 0 -font [$w.$x cget -font] -width 1 -text . -justify center \
                          -cursor [$w.$x cget -cursor] -bg [$w.$x cget -background] \
                          -disabledforeground [$w.$x cget -disabledforeground]
        pack $w.$x $w.$y -side left
        bindtags $w.$x [list $w.$x IPEntrybindtag . all]
        bind $w.$y <Button-1> {::ipentry::dotclick %W %x}
    }
    destroy $w.d4
    rename ::$w ::ipentry::_$w
    interp alias {} ::$w {} ::ipentry::widgetCommand $w
    bind $w <Destroy> [list rename ::$w {}]
    bind $w <FocusIn> [list focus $w.0]
    if {[llength $args] > 0} {
        eval [list $w configure] $args
    }
}

proc ::ipentry::keypress {w key} {
    if {![validate $w $key]} { return }
    catch {
        set insert [$w index insert]
        if {([$w index sel.first] <= $insert) && ([$w index sel.last] >= $insert)} {
            $w delete sel.first sel.last
        }
    }
    $w insert insert $key
}

proc ::ipentry::backspace {w} {
    if {[$w selection present]} {
        $w delete sel.first sel.last
    } else {
        if {[$w index insert] == 0} {
            skip $w prev
        } else {
            $w delete [expr {[$w index insert] - 1}]
        }
    }
}

proc ::ipentry::dot {w} {
    if {[string length [$w get]] > 0} {
        skip $w next
    }
}

proc ::ipentry::FocusIn {w} {
    set p [winfo parent $w]
    foreach x {0 1 2 3} {
        if {"$p.$x" != $w} {
            $p.$x selection clear
        }
    }
}

proc ::ipentry::FocusOut {w} {
    set s [$w get]
    if {[string match {*.0} $w] && $s != "" && $s < 1} {
        $w delete 0 end
        $w insert end 1
    }
}

proc ::ipentry::Paste {w sel} {
    if {[catch {::tk::GetSelection $w $sel} paste]} { return }
    $w delete 0 end
    foreach char [split $paste {}] {
        if {![string match {[0123456789.]} $char]} { continue }
        if {$char != "."} {
            $w insert end $char
        }
        if {[$w get] > 255} {
            $w delete 0 end
            $w insert 0 255
        }
        if {$char == "."} {
            set n [string index $w end]
            if { $n >= 3 } { return }
            set w [string trimright $w "0123"][expr {$n + 1}]
            $w delete 0 end
            continue
        }
    }
}

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

proc ::ipentry::arrow {w key} {
    set i [$w index insert]
    set l [string length [$w get]]
    $w icursor [expr $i [string map {Right + Left -} $key] 1]
    $w selection clear
    if {$key == "Right" && ($i == $l || $l == 0)} {
        skip $w next
    } elseif {$key == "Left" && $i == 0} {
        skip $w prev
    }
}

proc ::ipentry::validate {w key} {
    if {![string match {[0123456789]} $key]} { return 0 }
    set s [$w get]
    set i [$w index insert]
    if {$s == "0" && $key == "0"} { return 0 }
    if {[string length $s] == 2} {
        set s [join [linsert [split $s {}] $i $key] {}]
        if {$s > 255} {
            $w delete 0 end
            $w insert 0 255
            $w selection range 0 end
            return 0
        } elseif {$i == 2} {
            skip $w next
        }
        return 1
    }
    if {[string length $s] >= 3 && ![$w selection present]} {
        if {$i == 3} { skip $w next }
        return 0
    }
    return 1
}

proc ::ipentry::skip {w dir} {
    set n [string index $w end]
    if {$dir == "next"} {
        if { $n >= 3 } { return }
        set next [string trimright $w "0123"][expr {$n + 1}]
        focus $next
        $next icursor 0
        $next selection range 0 end
    } else {
        if { $n <= 0 } { return }
        set prev [string trimright $w "0123"][expr {$n - 1}]
        focus $prev
        $prev icursor end
    }
}

proc ::ipentry::_foreach {w cmd} {
    foreach x {0 d1 1 d2 2 d3 3} {
        eval [list $w.$x] $cmd
    }
}

proc ::ipentry::cget {w cmd} {
    switch -exact -- [lindex $args 0] {
        -bd     -
        -relief {
            return [::ipentry::_$w cget [lindex $args 0]]
        }
        default {
            return [$w.0 cget $cmd]
        }
    }
    return
}

proc ::ipentry::configure {w args} {
    while {[set cmd [lindex $args 0]] != ""} {
        switch -exact -- $cmd {
            -state {
                set state [lindex $args 1]
                if {$state == "disabled"} {
                    _foreach $w [list configure -state disabled]
                    if {[set dbg [$w.0 cget -disabledbackground]] == ""} {
                        set dbg [$w.0 cget -bg]
                    }
                    foreach x {d1 d2 d3} { $w.$x configure -bg $dbg }
                    ::ipentry::_$w configure -bg $dbg
                } elseif {$state == "normal"} {
                    _foreach $w [list configure -state normal]
                    foreach x {d1 d2 d3} { $w.$x configure -bg [$w.0 cget -bg] }
                    ::ipentry::_$w configure -background [$w.0 cget -bg]
                } elseif {$state == "readonly"} {
                    foreach x {0 1 2 3} { $w.$x configure -state readonly }
                    if {[set robg [$w.0 cget -readonlybackground]] == ""} {
                        set robg [$w.0 cget -bg]
                    }
                    foreach x {d1 d2 d3} { $w.$x configure -bg $robg }
                    ::ipentry::_$w configure -bg $robg
                }
                set args [lrange $args 2 end]
            }
            -bg {
                _foreach $w [list configure -bg [lindex $args 1]]
                ::ipentry::_$w configure -bg [lindex $args 1]
                set args [lrange $args 2 end]
            }
            -disabledforeground {
                _foreach $w [list configure -disabledforeground [lindex $args 1]]
                set args [lrange $args 2 end]
            }
            -font -
            -fg   {
                _foreach $w [list configure $cmd [lindex $args 1]]
                set args [lrange $args 2 end]
            }
            -bd                  -
            -relief              -
            -highlightcolor      -
            -highlightbackground -
            -highlightthickness  {
                _$w configure $cmd [lindex $args 1]
                set args [lrange $args 2 end]
            }
            -readonlybackground -
            -disabledbackground -
            -selectforeground   -
            -selectbackground   -
            -selectborderwidth  -
            -insertbackground   {
                foreach x {0 1 2 3} { $w.$x configure $cmd [lindex $args 1] }
                set args [lrange $args 2 end]
            }
            default {
                error "unknown option \"[lindex $args 0]\""
            }
        }
    }
}

proc ::ipentry::widgetCommand {w cmd args} {
    switch -exact -- $cmd {
        get {
            foreach x {0 1 2 3} {
                set s [$w.$x get]
                if {[string length $s] > 1} { set s [string trimleft $s 0] }
                lappend r $s
            }
            return $r
        }
        insert {
            foreach x {0 1 2 3} {
                set n [lindex $args 0 $x]
                if {$n != ""} {
                    if {![string is integer -strict $n]} {
                        error "cannot insert non-numeric arguments"
                    }
                    if {$n > 255} { set n 255 }
                    if {$n < 0}   { set n 0 }
                    if {$x == 0 && $n < 1} { set n 1 }
                }
                $w.$x delete 0 end
                $w.$x insert 0 $n
            }
        }
        icursor {
            if {![string match $w.* [focus]]} {return}
            set i [lindex $args 0]
            if {![string is integer -strict $i]} {error "argument must be an integer"}
            set s [expr {$i / 4}]
            focus $w.$s
            $w.$s icursor [expr {$i % 4}]
        }
        complete {
            foreach x {0 1 2 3} {
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
