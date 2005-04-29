#!/bin/sh
# Copyright (c) 1999-2000 Jean-Claude Wippler <jcw@equi4.com>
#
# Tequilas  -  the "Tequila Server" implements shared persistent arrays
#\
exec tclkit "$0" ${1+"$@"}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# Imlementation notes:
#
# Commands starting with "Tqs" can be called from the remote client
# The rest uses lowercase "tqs" to prevent this (and for uniqueness)
#
# There is one global array which is used for all information which
# this server needs to carry around and track, called "tqs_info":
#
#   tqs_info(pending)   - id of pending "after" request, unset if none
#   tqs_info(timeout)   - milliSecs before timed commit, unset if never
#   tqs_info(verbose)   - log level: 0=off, 1=req's, 2=notify, 3=reply
#
# External views (type "X") are stored as files in directory, one item
# per text file.  This can be used to store large amounts of text in
# regular files, outside Metakit (though commit doesn't apply to them):
#
#   tqs_external(view)  - directory name, set for each external view
#
# Valid while processing an incoming request:
#   tqs_info(port)      - socket name of current client request
#
# The following will be defined for individual views:
#   tqs_notify($view)   - socket name of client to notify on changes
# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    # conditonal logging output
proc tqsPuts {level msg} {
    global tqs_info
    if {$level <= $tqs_info(verbose)} {
        puts $msg
    }
}

    # return a displayable string of limited length
proc tqsDisplay {str len} {
    if {[string length $str] > $len} {
        set str "[string range $str 0 $len]..."
    }
    regsub -all {[^ -~]} $str {?} str
    return $str
}

    # remote execution of any Metakit command, added 20-02-2000
proc TqsRemote {cmd args} {
	eval mk::$cmd $args
}

    # return the names of all views currently on file
proc TqsInfo {} {
    mk::file views tqs
}

    # get or set log level (see above for meaning of values 0..3)
proc TqsVerbose {{level ""}} {
    global tqs_info
    if {$level != ""} {
        set tqs_info(verbose) $level
    }
    return $tqs_info(verbose)
}

    # define a view (Metakit's equivalent concept for a Tcl array)
    # if the second argument is true, all existing data is removed
    # the third arg is used to specify a binary (B) of memo format (M)
    # if the third arg is "X", use a directory with files for storage
proc TqsDefine {view {clear 0} {type S}} {
    if {$type == "X"} {
        global tqs_external
        set tqs_external($view) ""
        if {$clear} {
            catch {file delete -force $view.data}
            tqsTrace $view "" u
        }
        file mkdir $view.data
        #catch {mk::view delete tqs.$view}
    } else {
        mk::view layout tqs.$view "name text:$type date:I"
        if {$clear && [mk::view size tqs.$view] > 0} {
            mk::view size tqs.$view 0
            tqsTrace $view "" u
        }
        #file delete -force $view.data
    }
    return
}

    # get rid of a view
proc TqsUndefine {view} {
    global tqs_external
    if {[info exists tqs_external($view)]} {
        file delete -force $view.data
        unset tqs_external($view)
    } else {
        mk::view delete tqs.$view
    }
    tqsTrace $view "" u
    return
}

    # return the list of all keys, like "array names view"
proc TqsNames {view} {
    set result {}
    global tqs_external
    if {[info exists tqs_external($view)]} {
        foreach x [glob -nocomplain $view.data/*] {
            regsub {.*/} $x {} x
            lappend result $x
        }
    } else {
        mk::loop c tqs.$view {
            lappend result [mk::get $c name]
        }
    }
    return $result
}

    # return the number of keys, like "array size view"
proc TqsSize {view} {
    set result {}
    global tqs_external
    if {[info exists tqs_external($view)]} {
        set result [llength [glob -nocomplain $view.data/*]]
    } else {
        set result [mk::view size tqs.$view]
    }
    return $result
}

    # return an existing value, lookup by key, like "set view(key)"
proc TqsGet {view key} {
    global tqs_external
    if {[info exists tqs_external($view)]} {
        set fd [open $view.data/$key]
        fconfigure $fd -translation binary
        set v [read $fd]
        close $fd
        return $v
    } else {
        set n [mk::select tqs.$view name $key]
        mk::get tqs.$view!$n text ;# throws error if absent
    }
}

    # store a value, create if necessary, like "set view(key) data"
    # the optional last arg can be used to force a specific timestamp
proc TqsSet {view key data {timestamp ""}} {
    global tqs_external
    if {[info exists tqs_external($view)]} {
        set fd [open $view.data/$key w]
        fconfigure $fd -translation binary
        puts -nonewline $fd $data
        close $fd
        # timestamp is ignored
    } else {
        set n [mk::select tqs.$view name $key]
        if {[llength $n] == 0} {
            set n [mk::view size tqs.$view]
        } elseif {[mk::get tqs.$view!$n text] == $data} {
            return ;# no change, ignore
        }
        if {$timestamp == ""} {
            set timestamp [clock seconds]
        }
        mk::set tqs.$view!$n name $key text $data date $timestamp
    }
    tqsTrace $view $key w
    return
}

    # Append a value, create if entry did not exist
proc TqsAppend {view key data} {
    global tqs_external
    if {[info exists tqs_external($view)]} {
        set fd [open $view.data/$key a]
        fconfigure $fd -translation binary
        puts -nonewline $fd $data
        close $fd
    } else {
        set n [mk::select tqs.$view name $key]
        if {[llength $n] > 0} {
            if {[string length $data] == 0} then return ;# no change
            set data "[mk::get tqs.$view!$n text]$data"
        }
        mk::set tqs.$view!$n name $key text $data date [clock seconds]
    }
    tqsTrace $view $key w
    return
}

    # delete an existing entry by key, similar to "unset view(key)"
proc TqsUnset {view key} {
    global tqs_external
    if {[info exists tqs_external($view)]} {
        file delete $view.data/$key
    } else {
        set n [mk::select tqs.$view name $key]
        if {[llength $n] == 0} {
            return ;# no change, ignore
        }
        mk::row delete tqs.$view!$n
    }
    tqsTrace $view $key u
    return
}

    # return all key/value pairs, like "array get view"
    # if set, the optional arg sets up change notification
proc TqsGetAll {view {tracking 0}} {
    set result {}
    global tqs_external
    if {[info exists tqs_external($view)]} {
        foreach x [TqsNames $view] {
            lappend result $x [TqsGet $view $x]
        }
    } else {
        mk::loop c tqs.$view {
            eval lappend result [mk::get $c name text]
        }
    }
    if {$tracking} { tqsSubscribe $view }
    return $result
}

    # like TqsGetAll, returns modification dates instead of contents
    # this can be used by the client to synchronize and track dates
    # if set, the optional arg sets up change notification
proc TqsListing {view {tracking 0}} {
    set result {}
    global tqs_external
    if {[info exists tqs_external($view)]} {
        foreach x [TqsNames $view] {
            lappend result $x [file mtime $view.data/$x]
        }
    } else {
        mk::loop c tqs.$view {
            eval lappend result [mk::get $c name date]
        }
    }
    if {$tracking} { tqsSubscribe $view }
    return $result
}
    
    # called to set up notification for a client
proc tqsSubscribe {view} {
    global tqs_info tqs_notify
    
        # remember the client IP and listening number for this view
    tqsPuts 1 "Notification set up for '$view': $tqs_info(port)"
    lappend tqs_notify($view) $tqs_info(port)
}

    # called to unset all notifications for a client
proc tqsUnsubscribe {port} {
    global tqs_notify
    
    foreach {k v} [array get tqs_notify] {
        set n [lsearch -exact $v $port]
        if {$n >= 0} {
            tqsPuts 1 "  Forget notify for $k"

            if {[llength $v] > 1} {
                set tqs_notify($k) [lreplace $v $n $n]
            } else {
                unset tqs_notify($k)
                tqsPuts 1 "   No more notifications for $k"
            }
        }
    }
}

    # set a number of key/value pairs, like "array set view pairs"
proc TqsSetAll {view pairs} {
    foreach {key value} $pairs {
        TqsSet $view $key $value
    }
}

    # save changes to file now
proc TqsCommit {} {
    global tqs_info
    
    set n [clock clicks]
    mk::file commit tqs
    tqsPuts 1 "Commit done ([expr {[clock clicks] - $n}])"
    
    after cancel TqsCommit
    catch {unset tqs_info(pending)}
    return
}

    # change commit timer, default is to commit with explicit calls
proc TqsTimer {{timer ""}} {
    global tqs_info
    
    after cancel TqsCommit
    
    if {$timer == ""} {
        catch {unset tqs_info(timeout)}
    } else {
        if {[info exists tqs_info(pending)]} {
            set tqs_info(pending) [after $timer TqsCommit]
        }
        set tqs_info(timeout) $timer
    }
}

    # handles tracing of all view changes (there's no read tracing)
    # this is also the place where delayed commits are scheduled
proc tqsTrace {view key operation} {
    global tqs_info tqs_notify
    
    if [info exists tqs_notify($view)] {
        switch $operation {
            w   { set req [list Set $view $key [TqsGet $view $key]] }
            u   { set req [list Unset $view $key] }
        }
        
            # this is the data that gets sent out
        set msg "[string length $req]\n$req"
        
        foreach p $tqs_notify($view) {
            if {$p == $tqs_info(port)} continue ;# skip originator
            
            if [catch {
                tqsPuts 2 [tqsDisplay "Notify $p - $req" 65]
                puts -nonewline $p $msg
                #flush $p
            } error] {
                tqsPuts 1 "Notify to $p failed for $view $key"
                tqsPuts 1 "  Reason: $error"
                catch {close $p}
                tqsUnsubscribe $p
            }
        }
    }
    
    if {![info exists tqs_info(pending)] && 
            [info exists tqs_info(timeout)]} {
        set tqs_info(pending) [after $tqs_info(timeout) TqsCommit]
    }
}

    # called whenever a request comes in
proc tqsRequest {sock} {
    global tqs_info
    
    if {[gets $sock bytes] > 0} {
        set request [read $sock [lindex $bytes 0]]
        if ![eof $sock] {
                # debugging: incoming request
            tqsPuts 1 [tqsDisplay " $request" 65]
            
            set tqs_info(port) $sock
            
            set err [catch {uplevel #0 Tqs$request} reply]
            set msg [list Reply $err $reply]
            puts -nonewline $sock "[string length $msg]\n$msg"
            
                # debugging: returned results
            if {[string length $reply] > 0} {
                tqsPuts 3 "   result: [tqsDisplay $reply 54]"
            }
            
            #flush $sock
            return
        } 
    }
    
    tqsPuts 1 "Closing $sock"
    close $sock 
    tqsUnsubscribe $sock
}

    # called whenever a connection is opened
proc tqsAccept {sock addr port} {
    global tqs_info
    fconfigure $sock -translation binary -buffering none
    fileevent $sock readable [list tqsRequest $sock]
}

    # this can be called to start a background server
proc tqsStart {port} {
    global tqs_notify tqs_external

    array set tqs_notify {}
    
    foreach x [glob -nocomplain *.data] {
        regsub {\.data$} $x {} x
        set tqs_external($x) ""
    }
    
    socket -server tqsAccept $port
}

    # this wraps the server into a standalone, it runs until shutdown
proc tqsRun {port} {
    global tqs_info
    
    set tqs_info(shutdown) [clock seconds]
    
        # these status messages are not disabled if verbose is off
    puts "Tequila server on port $port started."
    tqsStart $port
    vwait tqs_info(shutdown)
    puts "Tequila server on port $port has been shut down."
}

    # client-callable: terminate a server started with "tqsRun"
proc TqsShutdown {} {
    global tqs_info

        # returns number of seconds since the server was started
        # main effect is setting tqs_info(shutdown), which ends vwait
    set tqs_info(shutdown) [expr {[clock seconds]-$tqs_info(shutdown)}]
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# This script can be used standalone, in which case the code below will
# be run, or as part of a scripted document, which expects a "package
# ifneeded tequilas ..." to have been set up.  In that case, the code
# below will not be executed, allowing the caller so set up different
# parameter values before calling tqsRun or tqsStart (background use).
# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

if {[lsearch -exact [package names] tequilas] < 0} {
    package require Mk4tcl
    mk::file open tqs tequilas.dat -nocommit

    set tqs_info(verbose) 0     ;# default logging is off
    TqsTimer 30000              ;# default commit timer is 30 seconds
    tqsRun 20458                ;# default port is 20458
}
