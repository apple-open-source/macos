# time.tcl - Copyright (C) 2003 Pat Thoyts <patthoyts@users.sourceforge.net>
#
# Client for the Time protocol. See RFC 868
#
# -------------------------------------------------------------------------
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# -------------------------------------------------------------------------
#
# $Id: time.tcl,v 1.9.2.2 2004/05/27 02:47:46 andreas_kupries Exp $

package require Tcl 8.0;                # tcl minimum version
package require log;                    # tcllib 1.3

namespace eval ::time {
    variable version 1.0.3
    variable rcsid {$Id: time.tcl,v 1.9.2.2 2004/05/27 02:47:46 andreas_kupries Exp $}

    namespace export configure gettime server cleanup

    variable options
    if {![info exists options]} {
        array set options {
            -timeserver {}
            -port       37
            -protocol   tcp
            -timeout    10000
            -command    {}
            -loglevel   warning
        }
        if {![catch {package require udp}]} {
            set options(-protocol) udp
        }
        log::lvSuppressLE emergency 0
        log::lvSuppressLE $options(-loglevel) 1
        log::lvSuppress $options(-loglevel) 0
    }

    # Store conversions for other epochs. Currently only unix - but maybe
    # there are some others out there.
    variable epoch
    if {![info exists epoch]} {
        array set epoch {
            unix 2208988800
        }
    }

    # The id for the next token.
    variable uid
    if {![info exists uid]} {
        set uid 0
    }
}

# -------------------------------------------------------------------------

# Description:
#  Retrieve configuration settings for the time package.
#
proc ::time::cget {optionname} {
    return [configure $optionname]
}

# Description:
#  Configure the package.
#  With no options, returns a list of all current settings.
#
proc ::time::configure {args} {
    variable options
    set r {}
    set cget 0

    if {[llength $args] < 1} {
        foreach opt [lsort [array names options]] {
            lappend r $opt $options($opt)
        }
        return $r
    }

    if {[llength $args] == 1} {
        set cget 1
    }

    while {[string match -* [set option [lindex $args 0]]]} {
        switch -glob -- $option {
            -port     { set r [SetOrGet -port $cget] }
            -timeout  { set r [SetOrGet -timeout $cget] }
            -protocol { set r [SetOrGet -protocol $cget] }
            -command  { set r [SetOrGet -command $cget] }
            -loglevel {
                if {$cget} {
                    return $options(-loglevel)
                } else {
                    set options(-loglevel) [Pop args 1]
                    log::lvSuppressLE emergency 0
                    log::lvSuppressLE $options(-loglevel) 1
                    log::lvSuppress $options(-loglevel) 0
                }
            }
            --        { Pop args ; break }
            default {
                set err [join [lsort [array names State -*]] ", "]
                return -code error "bad option $option: must be $err"
            }
        }
        Pop args
    }
    
    return $r
}

# Set/get package options.
proc ::time::SetOrGet {option {cget 0}} {
    upvar options options
    upvar args args
    if {$cget} {
        return $options($option)
    } else {
        set options($option) [Pop args 1]
    }
    return {}
}

# -------------------------------------------------------------------------

proc ::time::gettime {args} {
    variable options
    variable uid
    set token [namespace current]::[incr uid]
    variable $token
    upvar 0 $token State

    array set State [array get options]
    set State(status) unconnected
    set State(data) {}

    while {[string match -* [set option [lindex $args 0]]]} {
        switch -glob -- $option {
            -port     { set State(-port) [Pop args 1] }
            -timeout  { set State(-timeout) [Pop args 1] }
            -proto*   { set State(-protocol) [Pop args 1] }
            -command  { set State(-command) [Pop args 1] }
            --        { Pop args ; break }
            default {
                set err [join [lsort [array names State -*]] ", "]
                return -code error "bad option $option: must be $err"
            }
        }
        Pop args
    }
    
    set len [llength $args]
    if {$len < 1 || $len > 2} {
        return -code error "wrong # args:
              \"gettime ?options? timeserver ?port?\""
    }
    set State(-timeserver) [lindex $args 0]
    if {$len == 2} {
        set State(-port) [lindex $args 1]
    }

    return [QueryTime $token]
}

proc ::time::QueryTime {token} {
    variable $token
    upvar 0 $token State

    if {$State(-protocol) == "udp"} {
        set State(sock) [udp_open]
        udp_conf $State(sock) $State(-timeserver) $State(-port)
    } else {
        if {[catch {
            set State(sock) [socket $State(-timeserver) $State(-port)]
        } sockerror]} {
            set State(status) error
            set State(error) $sockerror
            return $token
        }
    }

    # setup the timeout
    if {$State(-timeout) > 0} {
        set State(after) [after $State(-timeout) \
                              [list [namespace origin reset] $token timeout]]
    }

    set State(status) connect
    fconfigure $State(sock) -translation binary -buffering none

    if {$State(-protocol) == "udp"} {
        puts -nonewline $State(sock) "abcd"
    }

    fileevent $State(sock) readable \
        [list [namespace origin ClientReadEvent] $token]

    if {$State(-command) == {}} {
        wait $token
    }

    return $token
}

proc ::time::unixtime {{token {}}} {
    variable $token
    variable epoch
    upvar 0 $token State
    if {$State(status) != "ok"} {
        return -code error $State(error)
    }
    if {[binary scan $State(data) I r] < 1} {
        return -code error "Unable to scan data"
    }
    return [expr {int($r - $epoch(unix))}]
}

proc ::time::status {token} {
    variable $token
    upvar 0 $token State
    return $State(status)
}

proc ::time::error {token} {
    variable $token
    upvar 0 $token State
    set r {}
    if {[info exists State(error)]} {
        set r $State(error)
    }
    return $r
}

proc ::time::wait {token} {
    variable $token
    upvar 0 $token State

    if {$State(status) == "connect"} {
        vwait [subst $token](status)
    }

    return $State(status)
}

proc ::time::reset {token {why reset}} {
    variable $token
    upvar 0 $token State
    set reason {}
    set State(status) $why
    catch {fileevent $State(sock) readable {}}
    if {$why == "timeout"} {
        set reason "timeout ocurred"
    }
    Finish $token $reason
}

# Description:
#  Remove any state associated with this token.
#
proc ::time::cleanup {token} {
    variable $token
    upvar 0 $token State
    if {[info exists State]} {
        unset State
    }
}

# -------------------------------------------------------------------------

proc ::time::ClientReadEvent {token} {
    variable $token
    upvar 0 $token State

    append State(data) [read $State(sock)]
    if {[string length $State(data)] < 4} {return}

    #FIX ME: acquire peer data?
    set State(status) ok
    Finish $token
    return
}

proc ::time::Finish {token {errormsg {}}} {
    variable $token
    upvar 0 $token State
    global errorInfo errorCode

    if {[string length $errormsg] > 0} {
	set State(error) $errormsg
	set State(status) error
    }
    catch {close $State(sock)}
    catch {after cancel $State(after)}
    if {[info exists State(-command)] && $State(-command) != {}} {
        if {[catch {eval $State(-command) {$token}} err]} {
            if {[string length $errormsg] == 0} {
                set State(error) [list $err $errorInfo $errorCode]
                set State(status) error
            }
        }
        if {[info exists State(-command)]} {
            unset State(-command)
        }
    }
}

# -------------------------------------------------------------------------
# Description:
#  Pop the nth element off a list. Used in options processing.
#
proc ::time::Pop {varname {nth 0}} {
    upvar $varname args
    set r [lindex $args $nth]
    set args [lreplace $args $nth $nth]
    return $r
}

# -------------------------------------------------------------------------

package provide time $::time::version

# -------------------------------------------------------------------------
# Local variables:
#   mode: tcl
#   indent-tabs-mode: nil
# End:
