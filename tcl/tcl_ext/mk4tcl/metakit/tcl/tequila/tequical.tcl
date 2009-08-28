#!/bin/sh
# Copyright (c) 1999-2000 Jean-Claude Wippler <jcw@equi4.com>
#
# TequiCal  -  shared yearly event calendar, a demo based on Tequila
# \
exec tclkit "$0" ${1+"$@"}  

proc bgerror message {
    puts "bgerror: message, errorInfo, and errorCode are\
            '$message', '$::errorInfo', and '$::errorCode'."
}

    # present a dialog box asking for a host to connect to
proc AskSite {{host localhost}} {
    global as_host as_status
    set as_host $host
    toplevel .as
    wm title .as "Site setup"
    pack [label .as.l -text "Where is the Tequila server?"] \
         [entry .as.e -width 30 -textvariable as_host] \
         [button .as.b -text "OK" -command {set as_status 1}] -padx 4 -pady 4
    bind .as <Return> {.as.b invoke}
    bind .as <Escape> {exit}
    wm protocol .as WM_DELETE_WINDOW {exit}
    update
    raise .as
    .as.e selection adjust end
    focus .as.e
    vwait as_status
    destroy .as
    return $as_host 
}

    # set up the main window and the trace on the global shared array
proc MainWindow {} {
    global calendar detail

    wm title . "Tequila Calendar Demo"
    
    frame .f1
    scrollbar .f1.sb -command [list .f1.lb yview]
    listbox .f1.lb -width 50 -height 10 -exportselection 0 \
        -yscrollcommand [list .f1.sb set]
    pack .f1.sb -fill y -side right
    pack .f1.lb -fill both -expand 1
    
    frame .f2
    pack [label .f2.l1 -text "Event:" -anchor e -width 7] -side left
    pack [entry .f2.e -textvariable detail(e)] -side left -fill x -expand 1
    
    frame .f3
    pack [label .f3.l1 -text "Month:" -anchor e -width 7] \
         [entry .f3.e1 -textvariable detail(m) -width 3] \
         [label .f3.l2 -text "  Day:"] \
         [entry .f3.e2 -textvariable detail(d) -width 3] \
         [label .f3.l3 -text "    (clear event to delete)" -anchor w] -side left
    
    pack .f1 -fill both -expand 1 -padx 4 -pady 4
    pack .f2 -fill x -padx 4 
    pack .f3 -anchor w -padx 4 -pady 4

    bind .f1.lb <ButtonPress> {ChooseEntry [.f1.lb nearest %y]}
    .f1.lb insert end "Loading..."

    ChooseEntry end
    trace variable detail wu {after cancel FixEntry; after 100 FixEntry; #}
    trace variable calendar wu {after cancel FixList; after 100 FixList; #}
}

    # this gets called shortly after each change to the calendar array
proc FixList {} {
    global calendar
    .f1.lb delete 0 end
    foreach k [lsort [array names calendar]] {
        .f1.lb insert end $k
    }
    .f1.lb insert end "(new entry)"
}

    # this gets called shortly after each editing change
proc FixEntry {} {
    global calendar detail
    
    regexp {..$} "00$detail(m)" m
    regexp {..$} "00$detail(d)" d
    set new "$m/$d $detail(e)"

    if {$new == $detail(full)} return
    
    catch {unset calendar($detail(full))}
    if {$detail(e) != ""} {
        set calendar($new) ""
    }
    set detail(full) $new
}

    # called when a list entry has been clicked
proc ChooseEntry {n} {
    global detail
    
    set detail(full) [.f1.lb get $n]
    if {![regexp {^(..)/(..) (.*)} $detail(full) x \
                                    detail(m) detail(d) detail(e)]} {
        set now [clock seconds]
        set detail(m) [clock format $now -format %m]
        set detail(d) [clock format $now -format %d]
        set detail(e) ""
    }
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# Actual work starts here: connect, set up window/traces, and attach.

source tequila.tcl

    # set up a connection to the central Tequila server
eval tequila::open [AskSite] 20458

    # layout the main window and show it
MainWindow
update

    # setup for a global "calendar" array to be shared
tequila::attach calendar

    # set some initial data when there is nothing yet
if {[array size calendar] == 0} {
    foreach x {
        "01/01 Happy New Year!"
        "12/25 Christmas Day"
        "04/07 Someone's birthday (1989)"
    } {
        set calendar($x) ""
    }
}
