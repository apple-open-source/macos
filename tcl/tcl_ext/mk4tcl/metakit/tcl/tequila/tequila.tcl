# Copyright (C) 1999-2000 Jean-Claude Wippler <jcw@equi4.com>
#
# Tequila  -  client interface to the Tequila server

package provide tequila 1.5

namespace eval tequila {
    namespace export open close do attach

    variable _socket
    variable _reply
    
        # setup communication with the tequila server
    proc open {addr port} {
        variable _socket
        set _socket [socket $addr $port] 
        fconfigure $_socket -translation binary -buffering none
        fileevent $_socket readable tequila::privRequest
    }

        # setup callback for when link to server fails
    proc failure {cmd} {
        variable _socket
        trace variable _socket u $cmd
    }

        # terminate communication (this is usually not needed)
    proc close {} {
        variable _socket
        ::close $_socket 
    }

        # set up to pass almost all MK requests through to the server
        # note that mk::loop is not implemented, is only works locally
        # added 20-02-2000
    proc proxy {} {
        namespace eval ::mk {
            foreach i {file view row cursor get set select channel} {
                proc $i {args} "eval ::tequila::do Remote $i \$args"
            }
        }
    }
    
        # send a request to the server and wait for a response
    proc do {args} {
        variable _socket
        variable _reply ""
        
        catch {
            puts -nonewline $_socket "[string length $args]\n$args"
            while {[string length $_reply] == 0} {
                vwait tequila::_reply
            }
        }
        
        set error 0
        set results ""
        foreach {error results} $_reply break
        
        if {[string compare $error 0] == 0} {
            return $results
        }
        
        if {[string length $results] > 0} {
            error $results 
        }
        
        error "Failed network request to the server."
    }

        # prepare for automatic change propagation
    proc attach {array args} {
        array set opts {-fetch 1 -tracking 1 -type S}
        array set opts $args
        
        global $array
        do Define $array 0 $opts(-type)
        
        if {$opts(-fetch)} {
            set command GetAll
        } else {
            set command Listing
        }
        
        array set $array [do $command $array $opts(-tracking)]
        
        trace variable $array wu tequila::privTracer
    }

        # called whenever a request comes in (private)
    proc privRequest {} {
        variable _socket
        variable _reply
        
        if {[gets $_socket bytes] > 0} {
            set request [read $_socket $bytes]
            if ![eof $_socket] {
                uplevel #0 tequila::privCallBack_$request
                return
            } 
        }
            # trouble, make sure we stop a pending request
        set _reply [list 1 "Lost connection with the tequila server."]
        ::close $_socket 
        unset _socket
    }

        # handles traces to propagate changes to the server (private)
    proc privTracer {a e op} {
        if {$e != ""} {
            switch $op {
                w   { do Set $a $e [set ::${a}($e)] }
                u   { do Unset $a $e }
            }
        }
    }

        # called by the server to return a result
    proc privCallBack_Reply {args} {
        variable _reply
        set _reply $args
    }

        # called by the server to propagate an element write
    proc privCallBack_Set {a e v} {
        global $a
        if {![info exists ${a}($e)] || [set ${a}($e)] != $v} {
            trace vdelete $a wu tequila::privTracer
            set ${a}($e) $v    
            trace variable $a wu tequila::privTracer
        }
    }

        # called by the server to propagate an element delete
    proc privCallBack_Unset {a e} {
        global $a
        if {[info exists ${a}($e)]} {
            trace vdelete $a wu tequila::privTracer
            unset ${a}($e)
            trace variable $a wu tequila::privTracer
        }
    }
}

