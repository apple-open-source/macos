# chatwidget.tcl --
#
#	This package provides a composite widget suitable for use in chat
#	applications. A number of panes managed by panedwidgets are available
#	for displaying user names, chat text and for entering new comments.
#	The main display area makes use of text widget peers to enable a split
#	view for history or searching.
#
# Copyright (C) 2007 Pat Thoyts <patthoyts@users.sourceforge.net>
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# RCS: @(#) $Id: chatwidget.tcl,v 1.4 2008/06/20 22:53:54 patthoyts Exp $

package require Tk 8.5

namespace eval chatwidget {
    variable version 1.1.0

    namespace export chatwidget

    ttk::style layout ChatwidgetFrame {
        Entry.field -sticky news -border 1 -children {
            ChatwidgetFrame.padding -sticky news
        }
    }
    if {[lsearch -exact [font names] ChatwidgetFont] == -1} {
        eval [list font create ChatwidgetFont] [font configure TkTextFont]
        eval [list font create ChatwidgetBoldFont] \
            [font configure ChatwidgetFont] -weight bold
        eval [list font create ChatwidgetItalicFont] \
            [font configure ChatwidgetFont] -slant italic
    }
}

proc chatwidget::chatwidget {w args} {
    Create $w
    interp hide {} $w
    interp alias {} $w {} [namespace origin WidgetProc] $w
    return $w
}

proc chatwidget::WidgetProc {self cmd args} {
    upvar #0 [namespace current]::$self state
    switch -- $cmd {
        hook {
            if {[llength $args] < 2} {
                return -code error "wrong \# args: should be\
                    \"\$widget hook add|remove|list hook_type ?script? ?priority?\""
            }
            return [uplevel 1 [list [namespace origin Hook] $self] $args]
        }
        cget {
            return [uplevel 1 [list [namespace origin Cget] $self] $args]
        }
        configure {
            return [uplevel 1 [list [namespace origin Configure] $self] $args]
        }
        insert {
            return [uplevel 1 [list [namespace origin Insert] $self] $args]
        }
        message {
            return [uplevel 1 [list [namespace origin Message] $self] $args]
        }
        name {
            return [uplevel 1 [list [namespace origin Name] $self] $args]
        }
        topic {
            return [uplevel 1 [list [namespace origin Topic] $self] $args]
        }
        names {
            return [uplevel 1 [list [namespace origin Names] $self] $args]
        }
        entry {
            return [uplevel 1 [list [namespace origin Entry] $self] $args]
        }
        peer {
            return [uplevel 1 [list [namespace origin Peer] $self] $args]
        }
        chat - 
        default {
            return [uplevel 1 [list [namespace origin Chat] $self] $args]
        }
    }
    return
}

proc chatwidget::Chat {self args} {
    upvar #0 [namespace current]::$self state
    if {[llength $args] == 0} {
        return $state(chat_widget)
    }
    return [uplevel 1 [list $state(chat_widget)] $args]
}

proc chatwidget::Cget {self args} {
    upvar #0 [namespace current]::$self state
    switch -exact -- [set what [lindex $args 0]] {
        -chatstate { return $state(chatstate) }
        -history { return $state(history) }
        default {
            return [uplevel 1 [list $state(chat_widget) cget] $args]
        }
    }
}

proc chatwidget::Configure {self args} {
    upvar #0 [namespace current]::$self state
    switch -exact -- [set option [lindex $args 0]] {
        -chatstate {
            if {[llength $args] > 1} { set state(chatstate) [Pop args 1] }
            else { return $state(chatstate) }
        }
        -history {
            if {[llength $args] > 1} { set state(history) [Pop args 1] }
            else { return $state(history) }
        }
        -font {
            if {[llength $args] > 1} {
                set font [Pop args 1]
                set family [font actual $font -family]
                set size [font actual $font -size]
                font configure ChatwidgetFont -family $family -size $size
                font configure ChatwidgetBoldFont -family $family -size $size
                font configure ChatwidgetItalicFont -family $family -size $size
            } else { return [$state(chat_widget) cget -font] }
        }
        default {
            return [uplevel 1 [list $state(chat_widget) configure] $args]
        }
    }
}

proc chatwidget::Peer {self args} {
    upvar #0 [namespace current]::$self state
    if {[llength $args] == 0} {
        return $state(chat_peer_widget)
    }
    return [uplevel 1 [list $state(chat_peer_widget)] $args]
}

proc chatwidget::Topic {self cmd args} {
    upvar #0 [namespace current]::$self state
    switch -exact -- $cmd {
        show { grid $self.topic -row 0 -column 0 -sticky new }
        hide { grid forget $self.topic }
        set  { set state(topic) [lindex $args 0] }
        default {
            return -code error "bad option \"$cmd\":\
                must be show, hide or set"
        }
    }
}

proc chatwidget::Names {self args} {
    upvar #0 [namespace current]::$self state
    set frame [winfo parent $state(names_widget)]
    set pane [winfo parent $frame]
    if {[llength $args] == 0} {
        return $state(names_widget)
    }
    if {[llength $args] == 1 && [lindex $args 0] eq "hide"} {
        return [$pane forget $frame]
    }
    if {[llength $args] == 1 && [lindex $args 0] eq "show"} {
        return [$pane add $frame]
    }
    return [uplevel 1 [list $state(names_widget)] $args] 
}

proc chatwidget::Entry {self args} {
    upvar #0 [namespace current]::$self state
    if {[llength $args] == 0} {
        return $state(entry_widget)
    }
    if {[llength $args] == 1 && [lindex $args 0] eq "text"} {
        return [$state(entry_widget) get 1.0 end-1c]
    }
    return [uplevel 1 [list $state(entry_widget)] $args]
}

proc chatwidget::Message {self text args} {
    upvar #0 [namespace current]::$self state
    set chat $state(chat_widget)

    set mark end
    set type normal
    set nick Unknown
    set time [clock seconds]
    set tags {}

    while {[string match -* [set option [lindex $args 0]]]} {
        switch -exact -- $option {
            -nick { set nick [Pop args 1] }
            -time { set time [Pop args 1] }
            -type { set type [Pop args 1] }
            -mark { set mark [Pop args 1] }
            -tags { set tags [Pop args 1] }
            default {
                return -code error "unknown option \"$option\""
            }
        }
        Pop args
    }

    if {[catch {Hook $self run message $text \
                    -mark $mark -type $type -nick $nick \
                    -time $time -tags $tags}] == 3} then {
        return
    }

    if {$type ne "system"} { lappend tags NICK-$nick }
    lappend tags TYPE-$type
    $chat configure -state normal
    set ts [clock format $time -format "\[%H:%M\]\t"]
    $chat insert $mark $ts [concat BOOKMARK STAMP $tags]
    if {$type eq "action"} {
        $chat insert $mark "   * $nick " [concat BOOKMARK NICK $tags]
        lappend tags ACTION
    } elseif {$type eq "system"} {
    } else {
        $chat insert $mark "$nick\t" [concat BOOKMARK NICK $tags]
    }
    if {$type ne "system"} { lappend tags MSG NICK-$nick }
    #$chat insert $mark $text $tags
    Insert $self $mark $text $tags
    $chat insert $mark "\n" $tags
    $chat configure -state disabled
    if {$state(autoscroll)} {
        $chat see $mark
    }
    return
}

proc chatwidget::Insert {self mark args} {
    upvar #0 [namespace current]::$self state
    if {![info exists state(urluid)]} {set state(urluid) 0}
    set w $state(chat_widget)
    set parts {}
    foreach {s t} $args {
        while {[regexp -indices {\m(https?://[^\s]+)} $s -> ndx]} {
            foreach {fr bk} $ndx break
            lappend parts [string range $s 0 [expr {$fr - 1}]] $t
            lappend parts [string range $s $fr $bk] \
                [linsert $t end URL URL-[incr state(urluid)]]
            set s [string range $s [incr bk] end]
        }
        lappend parts $s $t
    }
    set ws [$w cget -state]
    $w configure -state normal
    eval [list $w insert $mark] $parts
    $w configure -state $ws
}

# $w name add ericthered -group admin -color red
# state(names) {{pat -color red -group admin -thing wilf} {eric ....}}
proc chatwidget::Name {self cmd args} {
    upvar #0 [namespace current]::$self state
    switch -exact -- $cmd {
        list {
            switch -exact -- [lindex $args 0] {
                -full {
                    return $state(names)
                }
                default {
                    foreach item $state(names) { lappend r [lindex $item 0] }
                    return $r
                }
            }
        }
        add {
            if {[llength $args] < 1 || ([llength $args] % 2) != 1} {
                return -code error "wrong # args: should be\
                    \"add nick ?-group group ...?\""
            }
            set nick [lindex $args 0]
            if {[set ndx [lsearch -exact -index 0 $state(names) $nick]] == -1} {
                array set opts {-group {} -colour black}
                array set opts [lrange $args 1 end]
                lappend state(names) [linsert [array get opts] 0 $nick]
            } else {
                array set opts [lrange [lindex $state(names) $ndx] 1 end]
                array set opts [lrange $args 1 end]
                lset state(names) $ndx [linsert [array get opts] 0 $nick]
            }
            UpdateNames $self
        }
        delete {
            if {[llength $args] != 1} {
                return -code error "wrong # args: should be \"delete nick\""
            }
            set nick [lindex $args 0]
            if {[set ndx [lsearch -exact -index 0 $state(names) $nick]] != -1} {
                set state(names) [lreplace $state(names) $ndx $ndx]
                UpdateNames $self
            }
        }
        get {
            if {[llength $args] < 1} {
                return -code error "wrong # args:\
                    should be \"get nick\" ?option?"
            }
            set result {}
            set nick [lindex $args 0]
            if {[set ndx [lsearch -exact -index 0 $state(names) $nick]] != -1} {
                set result [lindex $state(names) $ndx]
                if {[llength $args] > 1} {
                    if {[set ndx [lsearch $result [lindex $args 1]]] != -1} {
                        set result [lindex $result [incr ndx]]
                    } else {
                        set result {}
                    }
                }
            }
            return $result
        }
        default {
            return -code error "bad name option \"$cmd\":\
                must be list, names, add or delete"
        }
    }
}

proc chatwidget::UpdateNames {self} {
    upvar #0 [namespace current]::$self state
    if {[info exists state(updatenames)]} {
        after cancel $state(updatenames)
    }
    set state(updatenames) [after idle [list [namespace origin UpdateNamesExec] $self]]
}

proc chatwidget::UpdateNamesExec {self} {
    upvar #0 [namespace current]::$self state
    unset state(updatenames)
    set names $state(names_widget)
    set chat  $state(chat_widget)
    
    foreach tagname [lsearch -all -inline [$names tag names] NICK-*] {
        $names tag delete $tagname
    }
    foreach tagname [lsearch -all -inline [$names tag names] GROUP-*] {
        $names tag delete $tagname
    }

    $names configure -state normal
    $names delete 1.0 end
    array set groups {}
    foreach item $state(names) {
        set group {}
        if {[set ndx [lsearch $item -group]] != -1} {
            set group [lindex $item [incr ndx]]
        }
        lappend groups($group) [lindex $item 0]
    }

    foreach group [lsort [array names groups]] {
        Hook $self run names_group $group
        $names insert end "$group\n" [list SUBTITLE GROUP-$group]
        foreach nick [lsort -dictionary $groups($group)] {
            $names tag configure NICK-$nick
            unset -nocomplain opts ; array set opts {}
            if {[set ndx [lsearch -exact -index 0 $state(names) $nick]] != -1} {
                array set opts [lrange [lindex $state(names) $ndx] 1 end]
                if {[info exists opts(-color)]} {
                    $names tag configure NICK-$nick -foreground $opts(-color)
                    $chat  tag configure NICK-$nick -foreground $opts(-color)
                }
                eval [linsert [lindex $state(names) $ndx] 0 \
                          Hook $self run names_nick]
            }
            $names insert end $nick\n [list NICK NICK-$nick GROUP-$group]
        }
    }
    $names insert end "[llength $state(names)] nicks\n" [list SUBTITLE]

    $names configure -state disabled
}

proc chatwidget::Pop {varname {nth 0}} {
    upvar $varname args
    set r [lindex $args $nth]
    set args [lreplace $args $nth $nth]
    return $r
}

proc chatwidget::Hook {self do type args} {
    upvar #0 [namespace current]::$self state
    set valid {message post names_group names_nick chatstate url}
    if {[lsearch -exact $valid $type] == -1} {
        return -code error "unknown hook type \"$type\":\
                must be one of [join $valid ,]"
    }
    switch -exact -- $do {
	add {
            if {[llength $args] < 1 || [llength $args] > 2} {
                return -code error "wrong # args: should be \"add hook cmd ?priority?\""
            }
            foreach {cmd pri} $args break
            if {$pri eq {}} { set pri 50 }
            lappend state(hook,$type) [list $cmd $pri]
            set state(hook,$type) [lsort -real -index 1 [lsort -unique $state(hook,$type)]]
	}
        remove {
            if {[llength $args] != 1} {
                return -code error "wrong # args: should be \"remove hook cmd\""
            }
            if {![info exists state(hook,$type)]} { return }
            for {set ndx 0} {$ndx < [llength $state(hook,$type)]} {incr ndx} {
                set item [lindex $state(hook,$type) $ndx]
                if {[lindex $item 0] eq [lindex $args 0]} {
                    set state(hook,$type) [lreplace $state(hook,$type) $ndx $ndx]
                    break
                }
            }
            set state(hook,$type)
        }
        run {
            if {![info exists state(hook,$type)]} { return }
            set res ""
            foreach item $state(hook,$type) {
                foreach {cmd pri} $item break
                set code [catch {eval $cmd $args} err]
                if {$code} {
                    ::bgerror "error running \"$type\" hook: $err"
                    break
                } else {
                    lappend res $err
                }
            }
            return $res
        }
        list {
            if {[info exists state(hook,$type)]} {
                return $state(hook,$type)
            }
        }
	default {
	    return -code error "unknown hook action \"$do\":\
                must be add, remove, list or run"
	}
    }
}

proc chatwidget::Grid {w {row 0} {column 0}} {
    grid rowconfigure $w $row -weight 1
    grid columnconfigure $w $column -weight 1
}

proc chatwidget::Create {self} {
    upvar #0 [set State [namespace current]::$self] state
    set state(history) {}
    set state(current) 0
    set state(autoscroll) 1
    set state(names) {}
    set state(chatstatetimer) {}
    set state(chatstate) active

    # NOTE: By using a non-ttk frame as the outermost part we are able
    # to be [wm manage]d. The outermost frame should be invisible at all times.
    set self [frame $self -class Chatwidget \
                  -borderwidth 0 -highlightthickness 0 -relief flat]
    set outer [ttk::panedwindow $self.outer -orient vertical]
    set inner [ttk::panedwindow $outer.inner -orient horizontal]

    # Create a topic/subject header
    set topic [ttk::frame $self.topic]
    ttk::label $topic.label -anchor w -text Topic
    ttk::entry $topic.text -state disabled -textvariable [set State](topic)
    grid $topic.label $topic.text -sticky new -pady {2 0} -padx 1
    Grid $topic 0 1

    # Create the usernames scrolled text
    set names [ttk::frame $inner.names -style ChatwidgetFrame]
    text $names.text -borderwidth 0 -relief flat -font ChatwidgetFont
    ttk::scrollbar $names.vs -command [list $names.text yview]
    $names.text configure -width 10 -height 10 -state disabled \
        -yscrollcommand [list [namespace origin scroll_set] $names.vs $inner 0]
    bindtags $names.text [linsert [bindtags $names.text] 1 ChatwidgetNames]
    grid $names.text $names.vs -sticky news -padx 1 -pady 1
    Grid $names 0 0
    set state(names_widget) $names.text

    # Create the chat display
    set chatf [ttk::frame $inner.chat -style ChatwidgetFrame]
    set peers [ttk::panedwindow $chatf.peers -orient vertical]
    set upper [ttk::frame $peers.upper]
    set lower [ttk::frame $peers.lower]

    set chat [text $lower.text -borderwidth 0 -relief flat -wrap word \
                  -state disabled -font ChatwidgetFont]
    set chatvs [ttk::scrollbar $lower.vs -command [list $chat yview]]
    $chat configure -height 10 -state disabled \
        -yscrollcommand [list [namespace origin scroll_set] $chatvs $peers 1]
    grid $chat $chatvs -sticky news
    Grid $lower 0 0
    set peer [$chat peer create $upper.text -borderwidth 0 -relief flat \
                  -wrap word -state disabled -font ChatwidgetFont]
    set peervs [ttk::scrollbar $upper.vs -command [list $peer yview]]
    $peer configure -height 0 \
        -yscrollcommand [list [namespace origin scroll_set] $peervs $peers 0]
    grid $peer $peervs -sticky news
    Grid $upper 0 0
    $peers add $upper
    $peers add $lower -weight 1
    grid $peers -sticky news -padx 1 -pady 1
    Grid $chatf 0 0
    bindtags $chat [linsert [bindtags $chat] 1 ChatwidgetText]
    set state(chat_widget) $chat
    set state(chat_peer_widget) $peer
    
    # Create the entry widget
    set entry [ttk::frame $outer.entry -style ChatwidgetFrame]
    text $entry.text -borderwidth 0 -relief flat -font ChatwidgetFont
    ttk::scrollbar $entry.vs -command [list $entry.text yview]
    $entry.text configure -height 1 \
        -yscrollcommand [list [namespace origin scroll_set] $entry.vs $outer 0]
    bindtags $entry.text [linsert [bindtags $entry.text] 1 ChatwidgetEntry]
    grid $entry.text $entry.vs -sticky news -padx 1 -pady 1
    Grid $entry 0 0
    set state(entry_widget) $entry.text

    bind ChatwidgetEntry <Return> "[namespace origin Post] \[[namespace origin Self] %W\]"
    bind ChatwidgetEntry <KP_Enter> "[namespace origin Post] \[[namespace origin Self] %W\]"
    bind ChatwidgetEntry <Shift-Return> "#"
    bind ChatwidgetEntry <Control-Return> "#"
    bind ChatwidgetEntry <Key-Up>   "[namespace origin History] \[[namespace origin Self] %W\] prev"
    bind ChatwidgetEntry <Key-Down> "[namespace origin History] \[[namespace origin Self] %W\] next"
    bind ChatwidgetEntry <Key-Tab> "[namespace origin Nickcomplete] \[[namespace origin Self] %W\]"
    bind ChatwidgetEntry <Key-Prior> "\[[namespace origin Self] %W\] chat yview scroll -1 pages"
    bind ChatwidgetEntry <Key-Next> "\[[namespace origin Self] %W\] chat yview scroll 1 pages"
    bind ChatwidgetEntry <Key> "+[namespace origin Chatstate] \[[namespace origin Self] %W\] composing"
    bind ChatwidgetEntry <FocusIn> "+[namespace origin Chatstate] \[[namespace origin Self] %W\] active"
    bind $self <Destroy> "+unset -nocomplain [namespace current]::%W"
    bind $peer       <Map> [list [namespace origin PaneMap] %W $peers 0]
    bind $names.text <Map> [list [namespace origin PaneMap] %W $inner -90]
    bind $entry.text <Map> [list [namespace origin PaneMap] %W $outer -28]

    bind ChatwidgetText <<ThemeChanged>> {
        ttk::style layout ChatwidgetFrame {
            Entry.field -sticky news -border 1 -children {
                ChatwidgetFrame.padding -sticky news
            }
        }
    }

    $names.text tag configure SUBTITLE \
        -background grey80 -font ChatwidgetBoldFont
    $chat tag configure NICK        -font ChatwidgetBoldFont
    $chat tag configure TYPE-system -font ChatwidgetItalicFont
    $chat tag configure URL         -underline 1

    $inner add $chatf -weight 1
    $inner add $names
    $outer add $inner -weight 1
    $outer add $entry
    
    grid $outer -row 1 -column 0 -sticky news -padx 1 -pady 1
    Grid $self 1 0
    return $self
}

proc chatwidget::Self {widget} {
    set class [winfo class [set w $widget]]
    while {[winfo exists $w] && [winfo class $w] ne "Chatwidget"} {
        set w [winfo parent $w]
    }
    if {![winfo exists $w]} {
        return -code error "invalid window $widget" 
    }
    return $w
}

# Set initial position of sash
proc chatwidget::PaneMap {w pane offset} {
    bind $pane <Map> {}
    if {[llength [$pane panes]] > 1} {
        if {$offset < 0} {
            if {[$pane cget -orient] eq "horizontal"} {
                set axis width
            } else {
                set axis height
            }
            #after idle [list $pane sashpos 0 [expr {[winfo $axis $pane] + $offset}]]
            after idle [namespace code [list PaneMapImpl $pane $axis $offset]]
        } else {
            #after idle [list $pane sashpos 0 $offset]
            after idle [namespace code [list PaneMapImpl $pane {} $offset]]
        }
    }
}

proc chatwidget::PaneMapImpl {pane axis offset} {
    if {$axis eq {}} {
        set size 0
    } else {
        set size [winfo $axis $pane]
    }
    set sashpos [expr {$size + $offset}]
    #puts stderr "PaneMapImpl $pane $axis $offset : size:$size sashpos:$sashpos"
    after 0 [list $pane sashpos 0 $sashpos]
}

# Handle auto-scroll smarts. This will cause the scrollbar to be removed if
# not required and to disable autoscroll for the text widget if we are not
# tracking the bottom line.
proc chatwidget::scroll_set {scrollbar pw set f1 f2} {
    $scrollbar set $f1 $f2
    if {($f1 == 0) && ($f2 == 1)} {
	grid remove $scrollbar
    } else {
        if {[winfo manager $scrollbar] eq {}} {}
            if {[llength [$pw panes]] > 1} {
                set pos [$pw sashpos 0]
                grid $scrollbar
                after idle [list $pw sashpos 0 $pos]
            } else {
                grid $scrollbar
            }
        
    }
    if {$set} {
        upvar #0 [namespace current]::[Self $scrollbar] state
        set state(autoscroll) [expr {(1.0 - $f2) < 1.0e-6 }]
    }
}

proc chatwidget::Post {self} {
    set msg [$self entry get 1.0 end-1c]
    if {$msg eq ""} { return -code break "" }
    if {[catch {Hook $self run post $msg}] != 3} {
        $self entry delete 1.0 end
        upvar #0 [namespace current]::$self state
        set state(history) [lrange [lappend state(history) $msg] end-50 end]
        set state(current) [llength $state(history)]
    }
    return -code break ""
}

proc chatwidget::History {self dir} {
    upvar #0 [namespace current]::$self state
    switch -exact -- $dir {
        prev {
            if {$state(current) == 0} { return }
            if {$state(current) == [llength $state(history)]} {
                set state(temp) [$self entry get 1.0 end-1c]
            }
            if {$state(current)} { incr state(current) -1 }
            $self entry delete 1.0 end
            $self entry insert 1.0 [lindex $state(history) $state(current)]
            return
        }
        next {
            if {$state(current) == [llength $state(history)]} { return }
            if {[incr state(current)] == [llength $state(history)] && [info exists state(temp)]} {
                set msg $state(temp)
            } else {
                set msg [lindex $state(history) $state(current)]
            }
            $self entry delete 1.0 end
            $self entry insert 1.0 $msg
        }
        default {
            return -code error "invalid direction \"$dir\":
                must be either prev or next"
        }
    }
}

proc chatwidget::Nickcomplete {self} {
    upvar #0 [namespace current]::$self state
    if {[info exists state(nickcompletion)]} {
        foreach {index matches after} $state(nickcompletion) break
        after cancel $after
        incr index
        if {$index > [llength $matches]} { set index 0 }
        set delta 2c
    } else {
        set delta 1c
        set partial [$self entry get "insert - $delta wordstart" "insert - $delta wordend"]
        set matches [lsearch -all -inline -glob -index 0 $state(names) $partial*]
        set index 0
    }
    switch -exact -- [llength $matches] {
        0 { bell ; return -code break ""}
        1 { set match [lindex [lindex $matches 0] 0]}
        default {
            set match [lindex [lindex $matches $index] 0]
            set state(nickcompletion) [list $index $matches \
                [after 2000 [list [namespace origin NickcompleteCleanup] $self]]]
        }
    }
    $self entry delete "insert - $delta wordstart" "insert - $delta wordend"
    $self entry insert insert "$match "
    return -code break ""
}

proc chatwidget::NickcompleteCleanup {self} {
    upvar #0 [namespace current]::$self state
    if {[info exists state(nickcompletion)]} {
        unset state(nickcompletion)
    }
}

# Update the widget chatstate (one of active, composing, paused, inactive, gone)
# These are from XEP-0085 but seem likey useful in many chat-type environments.
# Note: this state is _per-widget_. This is not the same as [tk inactive]
# active = got focus and recently active
#   composing = typing
#   paused = 5 secs non typing
# inactive = no activity for 30 seconds
# gone = no activity for 2 minutes or closed the window
proc chatwidget::Chatstate {self what} {
    upvar #0 [namespace current]::$self state
    after cancel $state(chatstatetimer)
    switch -exact -- $what {
        composing - active {
            set state(chatstatetimer) [after 5000 [namespace code [list Chatstate $self paused]]]
        }
        paused {
            set state(chatstatetimer) [after 25000 [namespace code [list Chatstate $self inactive]]]
        }
        inactive {
            set state(chatstatetimer) [after 120000 [namespace code [list Chatstate $self gone]]]
        }
        gone {}
    }
    set fire [expr {$state(chatstate) eq $what ? 0 : 1}]
    set state(chatstate) $what
    if {$fire} {
        catch {Hook $self run chatstate $what}
        event generate $self <<ChatwidgetChatstate>>
    }
}
    
package provide chatwidget $chatwidget::version
