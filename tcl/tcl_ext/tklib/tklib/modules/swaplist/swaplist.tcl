# swaplist.tcl --
#
#       A dialog which allows a user to move options between two lists
#
# Copyright (c) 2005    Aaron Faupell <afaupell@users.sourceforge.net>
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# RCS: @(#) $Id: swaplist.tcl,v 1.6 2008/02/06 07:15:16 afaupell Exp $

package require Tk
package provide swaplist 0.2

namespace eval swaplist {
    namespace export swaplist
}

if {[tk windowingsystem] == "win32"} {
    option add *Swaplist*Button.width -10 widgetDefault
    option add *Swaplist*Button.padX 1m widgetDefault
    option add *Swaplist*Border.borderWidth 2 widgetDefault
    option add *Swaplist*Border*Listbox.borderWidth 0 widgetDefault
} else {
    option add *Swaplist.borderWidth 1 widgetDefault
    option add *Swaplist*Button.width 5 widgetDefault
}

proc ::swaplist::swaplist {w var list1 list2 args} {
    array set options {
        -title "Configuration"
    }
    parseOpts options {{-llabel {}} {-rlabel {}} {-title {}} -embed \
                       {-reorder boolean} {-geometry {}} {-lbuttontext {}} \
                       {-rbuttontext {}} {-ubuttontext {}} {-dbuttontext {}}} \
                      $args

    if {[info exists options(-embed)]} {
        frame $w
        unset options(-embed)
        return [eval [list ::swaplist::createSwaplist $w $var $list1 $list2] [array get options]]
    }

    catch {destroy $w}
    set focus [focus]
    set grab [grab current .]
    
    toplevel $w -class Swaplist -relief raised
    wm title $w $options(-title)
    wm protocol $w WM_DELETE_WINDOW {set ::swaplist::whichButton 0}
    wm transient $w [winfo toplevel [winfo parent $w]]

    eval [list ::swaplist::createSwaplist $w ::swaplist::selectedList $list1 $list2] [array get options]

    frame $w.oc -pady 7
    button $w.oc.ok -default active -text "OK" -command {set ::swaplist::whichButton 1}
    button $w.oc.cancel -text "Cancel" -command {set ::swaplist::whichButton 0}
    pack $w.oc.cancel -side right -padx 7
    pack $w.oc.ok -side right
    grid $w.oc -columnspan 4 -row 2 -column 0 -sticky ew -columnspan 4

    bind $w <Return> [list $w.oc.ok invoke]
    bind $w <Escape> [list $w.oc.cancel invoke]
    bind $w <Destroy> {set ::swaplist::whichButton 0}

    #SetButtonState $w
    
    wm withdraw $w
    update idletasks
    if {[info exists options(-geometry)]} {
        wm geometry $w $options(-geometry)
    } elseif {[winfo parent $w] == "."} {
        set x [expr {[winfo screenwidth $w]/2 - [winfo reqwidth $w]/2 - [winfo vrootx $w]}]
        set y [expr {[winfo screenheight $w]/2 - [winfo reqheight $w]/2 - [winfo vrooty $w]}]
        wm geometry $w +$x+$y
    } else {
        set t [winfo toplevel [winfo parent $w]]
        set x [expr {[winfo width $t]/2 - [winfo reqwidth $w]/2 - [winfo vrootx $w]}]
        set y [expr {[winfo height $t]/2 - [winfo reqheight $w]/2 - [winfo vrooty $w]}]
        wm geometry $w +$x+$y
    }
    wm deiconify $w
    grab $w

    tkwait variable ::swaplist::whichButton
    upvar $var results
    set results $::swaplist::selectedList
    bind $w <Destroy> {}
    grab release $w
    destroy $w
    focus -force $focus
    if {$grab != ""} {grab $grab}
    update idletasks
    return $::swaplist::whichButton
}

proc ::swaplist::createSwaplist {w var list1 list2 args} {
    array set options {
        -reorder 1
        -llabel "Available:"
        -rlabel "Selected:"
        -lbuttontext "<<"
        -rbuttontext ">>"
        -ubuttontext "Move Up"
        -dbuttontext "Move Down"
    }
    parseOpts options {{-llabel {}} {-rlabel {}} {-title {}} \
                       {-reorder boolean} {-lbuttontext {}} {-geometry {}}\
                       {-rbuttontext {}} {-ubuttontext {}} {-dbuttontext {}}} \
                      $args

    set olist $list1
    
    # remove items in list2 from list1
    foreach x $list2 {
        if {[set i [lsearch $list1 $x]] >= 0} {
            set list1 [lreplace $list1 $i $i]
        }
    }

    label $w.heading1 -text $options(-llabel) -anchor w
    label $w.heading2 -text $options(-rlabel) -anchor w

    foreach x {list1 list2} {
        frame $w.$x -class Border -relief sunken
        scrollbar $w.$x.scrolly -orient v -command [list $w.$x.list yview]
        scrollbar $w.$x.scrollx -orient h -command [list $w.$x.list xview]
        listbox $w.$x.list -selectmode extended -yscrollcommand [list $w.$x.scrolly set] -xscrollcommand [list $w.$x.scrollx set]
        grid $w.$x.list -row 0 -column 0 -sticky nesw
        grid $w.$x.scrolly -row 0 -column 1 -sticky ns
        grid $w.$x.scrollx -row 1 -column 0 -sticky ew
        grid columnconfigure $w.$x 0 -weight 1
        grid rowconfigure $w.$x 0 -weight 1
    }
    $w.list2.list configure -listvariable $var
    $w.list2.list delete 0 end
    eval [list $w.list1.list insert end] $list1
    eval [list $w.list2.list insert end] $list2

    set width [min 5 $options(-lbuttontext) $options(-rbuttontext)]
    frame $w.lr
    button $w.lr.left -width $width -text $options(-lbuttontext) -command [list ::swaplist::ShiftL $w $olist]
    if {$options(-reorder)} {
        button $w.lr.right -width $width -text $options(-rbuttontext) -command [list ::swaplist::ShiftRNormal $w $olist]
    } else {
        button $w.lr.right -width $width -text $options(-rbuttontext) -command [list ::swaplist::ShiftRNoReorder $w $olist]
    }
    grid $w.lr.right -pady 4
    grid $w.lr.left -pady 4
    grid columnconfigure $w.lr 0 -uniform 1

    set width [min 3 $options(-ubuttontext) $options(-dbuttontext)]
    frame $w.ud
    button $w.ud.up   -width $width -text $options(-ubuttontext) -command [list ::swaplist::ShiftUD $w.list2.list u]
    button $w.ud.down -width $width -text $options(-dbuttontext) -command [list ::swaplist::ShiftUD $w.list2.list d]
    pack $w.ud.up   -side top    -pady 4
    pack $w.ud.down -side bottom -pady 4

    grid $w.heading1 -row 0 -column 0 -sticky ew   -padx {3 0} -pady 3
    grid $w.heading2 -row 0 -column 2 -sticky ew   -padx {0 3} -pady 3
    grid $w.list1    -row 1 -column 0 -sticky nesw -padx {3 0}
    grid $w.lr       -row 1 -column 1              -padx 7
    grid $w.list2    -row 1 -column 2 -sticky nesw -padx {0 3}
    if {$options(-reorder)} {
        grid $w.ud -row 1 -column 3 -padx {2 5}
    }
    grid columnconfigure $w {0 2} -weight 1
    grid rowconfigure $w 1 -weight 1

    bind $w <Key-Up> [list ::swaplist::UpDown %W %K]
    bind $w <Key-Down> [list ::swaplist::UpDown %W %K]
    bind $w.list1.list <Double-Button-1> [list ::swaplist::Double %W]
    bind $w.list2.list <Double-Button-1> [list ::swaplist::Double %W]
    #bind $w.list1.list <<ListboxSelect>> [list ::swaplist::SetButtonState %W]
    #bind $w.list2.list <<ListboxSelect>> [list ::swaplist::SetButtonState %W]
    
    if {![catch {package present autoscroll}]} {
        ::autoscroll::autoscroll $w.list1.scrollx
        ::autoscroll::autoscroll $w.list1.scrolly
        ::autoscroll::autoscroll $w.list2.scrollx
        ::autoscroll::autoscroll $w.list2.scrolly
    }

    #SetButtonState $w
    return $w
}

proc ::swaplist::parseOpts {var opts input} {
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

# return the min unless string1 or string2 is longer, if so return length of the longer one
proc ::swaplist::min {min s1 s2} {
    if {[string length $s1] > $min || [string length $s2] > $min} {
        return [expr {
                ([string length $s1] > [string length $s2]) \
                ? [string length $s1] \
                : [string length $s2]
               }]
    } else {
        return $min
    }
}

# return a list in reversed order
proc ::swaplist::lreverse {list} {
    set new {}
    foreach x $list {set new [linsert $new 0 $x]}
    return $new
}

# binding for "move left" button
proc ::swaplist::ShiftL {w olist} {
    set from $w.list2.list
    set to $w.list1.list
        
    if {[set cur [$from curselection]] == ""} { return }
    foreach x [lreverse $cur] {
        set name [$from get $x]
        $from delete $x
        set i [FindPos $olist [$to get 0 end] $name]
        $to insert $i $name
        $to selection set $i
    }
    if {[llength $cur] == 1} {$to see $i}
    if {[lindex $cur 0] == 0} {
        $from selection set 0
    } elseif {[lindex $cur 0] == [$from index end]} {
        $from selection set end
    } else {
        $from selection set [lindex $cur 0]
    }
}

# binding for "move right" button if -reorder is true
proc ::swaplist::ShiftRNormal {w olist} {
    set from $w.list1.list
    set to $w.list2.list

    if {[set cur [$from curselection]] == ""} { return }
    $to selection clear 0 end
    foreach x $cur {
        $to insert end [$from get $x]
        $to selection set end
    }
    foreach x [lreverse $cur] {
        $from delete $x
    }
    $to see end
}

# binding for "move right" button if -reorder is false
proc ::swaplist::ShiftRNoReorder {w olist} {
    set from $w.list1.list
    set to $w.list2.list
        
    if {[set cur [$from curselection]] == ""} { return }
    foreach x $cur {
        set name [$from get $x]
        set pos [FindPos $olist [$to get 0 end] $name]
        $to insert $pos $name
        lappend new $pos
    }
    foreach x [lreverse $cur] { $from delete $x }
    if {[$from index end] == 0} {
        foreach x $new {$to selection set $x}
    } elseif {[lindex $cur 0] == 0} {
        $from selection set 0
    } elseif {[lindex $cur 0] == [$from index end]} {
        $from selection set end
    } else {
        $from selection set [lindex $cur 0]
    }
}

# binding for "move up" and "move down" buttons
proc ::swaplist::ShiftUD {w dir} {
    if {[set sel [$w curselection]] == ""} { return }
    set list {}
    # delete in reverse order so shifting indexes dont bite us
    foreach x [lreverse $sel] {
        # make a list in correct order with the items index and contents
        set list [linsert $list 0 [list $x [$w get $x]]]
        $w delete $x
    }
    if {$dir == "u"} {
        set n 0
        foreach x $list {
            set i [lindex $x 0]
            if {[incr i -1] < $n} {set i $n}
            $w insert $i [lindex $x 1]
            $w selection set $i
            incr n
        }
        $w see [expr {[lindex $list 0 0] - 1}]
    }
    if {$dir == "d"} {
        set n [$w index end]
        foreach x $list {
            set i [lindex $x 0]
            if {[incr i] > $n} {set i $n}
            $w insert $i [lindex $x 1]
            $w selection set $i
            incr n
        }
        $w see $i
    }
}

# find the position $el should have in $curlist, by looking at $olist
# $curlist should be a subset of $olist
proc ::swaplist::FindPos {olist curlist el} {
    set orig [lsearch $olist $el]
    set end [llength $curlist]
    for {set i 0} {$i < $end} {incr i} {
        if {[lsearch $olist [lindex $curlist $i]] > $orig} { break }
    }
    return $i
}

# binding for the up and down arrow keys, just dispatch and have tk
# do the right thing
proc ::swaplist::UpDown {w key} {
    if {[winfo toplevel $w] != $w} {return}
    if {[set cur [$w.list2.list curselection]] != ""} {
        tk::ListboxUpDown $w.list2.list [string map {Up -1 Down 1} $key]
    } elseif {[set cur [$w.list1.list curselection]] != ""} {
        tk::ListboxUpDown $w.list1.list [string map {Up -1 Down 1} $key]
    } else {
        return
    }
}

# binding for double click, just invoke the left or right button
proc ::swaplist::Double {w} {
    set top [winfo toplevel $w]
    if {[string match *.list1.* $w]} {
        $top.lr.right invoke
    } elseif {[string match *.list2.* $w]} {
        $top.lr.left invoke
    }
}

proc ::swaplist::SetButtonState {w} {
    set top [winfo toplevel $w]
    if {[$top.list2.list curselection] != ""} {
        $top.lr.left  configure -state normal
        $top.lr.right configure -state disabled
    } elseif {[$top.list1.list curselection] != ""} {
        $top.lr.left  configure -state disabled
        $top.lr.right configure -state normal
    } else {
        $top.lr.left  configure -state disabled
        $top.lr.right configure -state disabled
    }

    if {[set cur [$top.list2.list curselection]] == ""} {
        $top.ud.up configure -state disabled
        $top.ud.down configure -state disabled
    } elseif {$cur == 0} {
        $top.ud.up configure -state disabled
        $top.ud.down configure -state normal
    } elseif {$cur == ([$top.list2.list index end] - 1)} {
        $top.ud.up configure -state normal
        $top.ud.down configure -state disabled
    } else {
        $top.ud.up configure -state normal
        $top.ud.down configure -state normal
    }
}

