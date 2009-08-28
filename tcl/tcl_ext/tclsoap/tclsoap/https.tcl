# https.tcl - Trivial modifications (by David Bleicher) to the HTTP
#             transport to enable SSL support.  Original copyright
#             follows:
#
# Copyright (C) 2002 David Bleicher
# Copyright (C) 2001 Pat Thoyts <patthoyts@users.sourceforge.net>
#
# The SOAP HTTPS Transport.
#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the accompanying file `LICENSE'
# for more details.
# -------------------------------------------------------------------------

package require SOAP
package require http;                   # tcl
package require tls;			# Required for SSL support

namespace eval ::SOAP::Transport::https {
    variable version 1.0
    variable rcsid {$Id: https.tcl,v 1.3 2008/02/28 22:05:55 andreas_kupries Exp $}
    variable options

    ::SOAP::register https [namespace current]

    # Initialize the transport options.
    if {![info exists options]} {
        array set options {
            headers  {}
            proxy    {}
            progress {}
            timeout  0
        }
    }

    # Declare the additional SOAP method options provided by this transport.
    variable method:options [list \
        httpheaders \
        timeout     \
    ]
    
    # Provide missing code for http < 2.3
    if {[info proc ::http::ncode] == {}} {
        namespace eval ::http {
            proc ncode {token} {
                return [lindex [split [code $token]] 1]
            }
        }
    }
}

# -------------------------------------------------------------------------

# Description:
#  Implement the additional SOAP method configuration options provide
#  for this transport.
# Notes:
#  -httpheaders - additional HTTP headers may be defined for specific
#       SOAP methods. Argument should be a two element list made of
#       the header name and value eg: [list Cookie $cookiedata]
# -timeout - the method can override the transport defined http timeout.
#       Set to {} to use the transport timeout, 0 for infinity.
proc ::SOAP::Transport::https::method:configure {procVarName opt value} {
    upvar $procVarName procvar
    switch -glob -- $opt {
        -httpheaders {
            set procvar(httpheaders) $value
        }
        -timeout {
            set procvar(timeout) $value
        }
        default {
            # not reached.
            return -code error "unknown option \"$opt\""
        }
    }
}

# -------------------------------------------------------------------------

# Description:
#  Configure any https transport specific settings.
#
proc ::SOAP::Transport::https::configure {args} {
    variable options

    if {[llength $args] == 0} {
        set r {}
        foreach {opt value} [array get options] {
            lappend r "-$opt" $value
        }
        return $r
    }

    foreach {opt value} $args {
        switch -- $opt {
            -proxy - -timeout -  -progress {
                set options([string trimleft $opt -]) $value
            }
            -headers {
                set options(headers) $value
            }
            default {
                return -code error "invalid option \"$opt\":\
                      must be \"-proxy host:port\" or \"-headers list\""
            }
        }
    }
    return {}
}

#  Configure any TLS sepcific parameters (e.g. certificate location)
#  Pass any values received to the "::tls::init" proceure
#  --Added by David Bleicher 26/03/02
proc ::SOAP::Transport::https::tlsConfigure {args} {
    set validopts [list -cafile -cadir -certfile -cipher -command -keyfile \
                       -model -request -require -server -ssl2 -ssl3 -tls1]
    set tlsoptslist "::tls::init"
    foreach {opt value} $args {
        if {[lsearch $validopts $opt] == -1} {
            return -code error "Invalid option \"$opt\"\
                            (valid options are: $validopts)\n"
        }
        lappend tlsoptslist $opt $value
    }
    eval $tlsoptslist
}


# -------------------------------------------------------------------------

# Description:
#   Perform a remote procedure call using HTTP as the transport protocol.
#   This uses the Tcl http package to do the work. If the SOAP method has
#   the -command option set to something then the call is made 
#   asynchronously and the result data passed to the users callback
#   procedure.
#   If you have an HTTP proxy to deal with then you should set up the 
#   SOAP::Transport::https::filter procedure and proxy variable to suit.
#   This can be done using SOAP::proxyconfig.
# Parameters:
#   procVarName - the name of the SOAP config array for this method.
#   url         - the SOAP endpoint URL
#   request     - the XML data making up the SOAP request
# Result:
#   The request data is POSTed to the SOAP provider via HTTP using any
#   configured proxy host. If the HTTP returns an error code then an error
#   is raised otherwise the reply data is returned. If the method has
#   been configured to be asynchronous then the async handler is called
#   once the http request completes.
#
proc ::SOAP::Transport::https::xfer { procVarName url request } {
    variable options
    upvar $procVarName procvar
    
    # Get the SOAP package version
    # FRINK: nocheck
    set version [set [namespace parent [namespace parent]]::version]
    
    # setup the HTTP POST request
    ::http::config -useragent "TclSOAP/$version ($::tcl_platform(os))"
    
    # If a proxy was configured, use it.
    if { [info exists options(proxy)] && $options(proxy) != {} } {
        ::http::config -proxyfilter [namespace origin filter]
    }
    
    # Check for an HTTP progress callback.
    set local_progress {}
    if { [info exists options(progress)] && $options(progress) != {} } {
        set local_progress "-progress [list $options(progress)]"
    }
    
    # Check for a timeout. Method timeout overrides transport timeout.
    set timeout $options(timeout)
    if {$procvar(timeout) != {}} {
        set timeout $procvar(timeout)
    }

    # There may be http headers configured. eg: for proxy servers
    # eg: SOAP::configure -transport http -headers 
    #    [list "Proxy-Authorization" [basic_authorization]]
    set local_headers {}
    if {[info exists options(headers)]} {
        set local_headers $options(headers)
    }
    if {[info exists procvar(httpheaders)]} {
        set local_headers [concat $local_headers $procvar(httpheaders)]
    }
    
    # Add mandatory SOAPAction header (SOAP 1.1). This may be empty otherwise
    # must be in quotes.
    set action $procvar(action)
    if { $action != {} } { 
        set action [string trim $action "\""]
        set action "\"$action\""
        lappend local_headers "SOAPAction" $action
    }
    
    # cleanup the last http request
    if {[info exists procvar(http)] && $procvar(http) != {}} {
        catch {::http::cleanup $procvar(http)}
    }
    
    # Check for an asynchronous handler and perform the transfer.
    # If async - return immediately.
    set command {}
    if {$procvar(command) != {}} {
        set command "-command {[namespace current]::asynchronous $procVarName}"
    }
   
    # Must test for the port and register an SSL socket with TLS
    # If there is a port number in the url, use it, otherwise use 443
    # --Added by David Bleicher 26/03/02
    regexp -nocase {^(https://)?([^:/]+)(:([0-9]+))?(/.*)} $url tX_x tX_protocol tX_server tX_y tX_port tX_path
    if {[string length $tX_port] == 0} {set tX_port 443}
    ::http::register https $tX_port ::tls::socket

    set token [eval ::http::geturl_followRedirects [list $url] \
                   -headers [list $local_headers] \
                   -type text/xml \
                   -timeout $timeout \
                   -query [list $request] \
                   $local_progress $command]
    
    # store the http structure reference for possible access later.
    set procvar(http) $token
    
    if { $command != {}} {
        return {} 
    }

    log::log debug "[::http::status $token] - [::http::code $token]"

    # Check for Proxy Authentication requests and handle it.
    if {[::http::ncode $token] == 407} {
        SOAP::proxyconfig
        return [xfer $procVarName $url $request]
    }

    # Some other sort of error ...
    switch -exact -- [::http::status $token] {
        timeout {
            return -code error "error: SOAP https transport timed out\
                after $timeout ms"
        }
        ok {
        }
        default {
            return -code error "SOAP transport error: \"[::http::code $token]\""
        }
    }

    return [::http::data $token]
}

# this proc contributed by [Donal Fellows]
proc ::http::geturl_followRedirects {url args} {
    set limit 10
    while {$limit > 0} {
        set token [eval [list ::http::geturl $url] $args]
        switch -glob -- [ncode $token] {
            30[1237] {
                incr limit -1
                ### redirect - see below ### 
            }
            default  { return $token }
        }
        upvar #0 $token state
        array set meta $state(meta)
        if {![info exist meta(Location)]} {
            return $token
        }
        set url $meta(Location)
        unset meta
    }
    return -code error "maximum relocation depth reached: site loop?"
}


# -------------------------------------------------------------------------

# Description:
#    Asynchronous http handler command.
proc ::SOAP::Transport::https::asynchronous {procVarName token} {
    upvar $procVarName procvar

    if {[catch {asynchronous2 $procVarName $token} msg]} {
        if {$procvar(errorCommand) != {}} {
            set errorCommand $procvar(errorCommand)
            if {[catch {eval $errorCommand [list $msg]} result]} {
                bgerror $result
            }
        } else {
            bgerror $msg
        }
    }
    return $msg
}

proc ::SOAP::Transport::https::asynchronous2 {procVarName token} {
    upvar $procVarName procvar
    set procName [lindex [split $procVarName {_}] end]

    # Some other sort of error ...
    if {[::http::status $token] != "ok"} {
         return -code error "SOAP transport error: \"[::http::code $token]\""
    }

    set reply [::http::data $token]

    # Call the second part of invoke to unwrap the packet data.
    set reply [SOAP::invoke2 $procVarName $reply]

    # Call the users handler.
    set command $procvar(command)
    return [eval $command [list $reply]]
}

# -------------------------------------------------------------------------

# Description:
#   Handle a proxy server. If the -proxy options is set then this is used
#   to override the http package configuration.
# Notes:
#   Needs expansion to use a list of non-proxied sites or a list of
#   {regexp proxy} or something.
#   The proxy variable in this namespace is set up by 
#   SOAP::configure -transport http.
#
proc ::SOAP::Transport::https::filter {host} {
    variable options
    if { [string match "localhost*" $host] \
             || [string match "127.*" $host] } {
        return {}
    }
    return [lrange [split $options(proxy) {:}] 0 1]
}

# -------------------------------------------------------------------------

# Description:
#   Support asynchronous transactions and permit waiting for completed
#   calls.
# Parameters:
#
proc ::SOAP::Transport::https::wait {procVarName} {
    upvar $procVarName procvar
    http::wait $procvar(http)
}
# -------------------------------------------------------------------------

# Description:
#  Called to release any retained resources from a SOAP method. For the
#  http transport this is just the http token.
# Parameters:
#  methodVarName - the name of the SOAP method configuration array
#
proc ::SOAP::Transport::https::method:destroy {methodVarName} {
    upvar $methodVarName procvar
    if {[info exists procvar(http)] && $procvar(http) != {}} {
        catch {::http::cleanup $procvar(http)}
    }
}

# -------------------------------------------------------------------------

proc ::SOAP::Transport::https::dump {methodName type} {
    SOAP::cget $methodName proxy
    if {[catch {SOAP::cget $methodName http} token]} {
        set token {}
    }

    if { $token == {} } {
        return -code error "cannot dump: no information is available\
            for \"$methodName\""
    }

    set result {}
    switch -glob -- $type {
        -meta   {set result [lindex [array get $token meta] 1]}
        -qu*    -
        -req*   {set result [lindex [array get $token -query] 1]}
        -rep*   {set result [::http::data $token]}
        default {
            return -code error "unrecognised option: must be one of \
                    \"-meta\", \"-request\" or \"-reply\""
        }
    }

    return $result
}

# -------------------------------------------------------------------------

package provide SOAP::https $::SOAP::Transport::https::version

# -------------------------------------------------------------------------
# Local variables:
#    mode: tcl
#    indent-tabs-mode: nil
# End:
