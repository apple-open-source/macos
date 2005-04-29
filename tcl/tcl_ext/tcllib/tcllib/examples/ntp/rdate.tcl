# rdate.tcl - Copyright (C) 2003 Pat Thoyts <patthoyts@users.sourceforge.net>
#
# NAME
#  rdate - set the system's date from a remote host
#
# SYNOPSIS
#  rdate [-psa] [-ut] host
#
# DESCRIPTION
#  Rdate displays and sets the local date and time from the host name or ad-
#  dress given as the argument. It uses the RFC868 protocol which is usually
#  implemented as a built-in service of inetd(8).
#
#  Available options:
#
#  -p      Do not set, just print the remote time
#
##  -s      Do not print the time.
##
##  -a      Use the adjtime(2) call to gradually skew the local time to the
##          remote time rather than just hopping.
#
#  -u      Use UDP
#
#  -t      Use TCP
#
# -------------------------------------------------------------------------
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# -------------------------------------------------------------------------
#
# $Id: rdate.tcl,v 1.2 2004/01/15 06:36:12 andreas_kupries Exp $

package require time;                   # tcllib 1.4

proc rdate {args} {
    # process the command line options.
    array set opts {-p 0 -s 0 -a 0 -t 0 -u x}
    while {[string match -* [set option [lindex $args 0]]]} {
        switch -exact -- $option {
            -p { set opts(-p) 1 }
            -u { set opts(-t) 0 }
            -t { set opts(-t) 1 }
            -s { return -code error "not implemented: use rdate(8)" }
            -a { return -code error "not implemented: use rdate(8)" }
            -- { ::time::Pop args; break }
            default {
                set err [join [lsort [array names opts -*]] ", "]
                return -code error "bad option $option: must be $err"
            }
        }
        ::time::Pop args
    }

    # Check that we have a host to talk to.
    if {[llength $args] != 1} {
        return -code error "wrong \# args: "
    }
    set host [lindex $args 0]

    # Construct the time command - optionally force the protocol to tcp
    set cmd ::time::gettime
    if {$opts(-t)} {
        lappend cmd -protocol tcp
    }
    lappend cmd $host

    # Perform the RFC 868 query (synchronously)
    set tok [eval $cmd]

    # Check for errors or extract the time in the unix epoch.
    set t 0
    if {[::time::status $tok] == "ok"} {
        set t [::time::unixtime $tok]
        ::time::cleanup $tok
    } else {
        set msg [::time::error $tok]
        ::time::cleanup $tok
        return -code error $msg 
    }

    # Display the time.
    if {$opts(-p)} {
        puts [clock format $t]
    }

    return
}

if {! $tcl_interactive} {
    eval rdate $argv
}
