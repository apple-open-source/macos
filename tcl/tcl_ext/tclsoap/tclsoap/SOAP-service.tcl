# SOAP-service.tcl - Copyright (C) 2001 Pat Thoyts <Pat.Thoyts@bigfoot.com>
#
# Provide a SOAP demo service.
#
# This package provides a simple HTTP server that is useful for stand-alone
# testing of HTTP requests (including SOAP requests). This is not meant
# to be a production-quality web server.
#
# Replies to GET requests with the contents of a file in a subdirectory if
# the requested file can be found. Some simple filename extension to MIME
# content-type matching is performed.
#
# POST requests are passed to a handler function, currently only /soap/base64
# is actually valid and this returns a fixed base64 encoded string.
#
# The toplevel procedures are `start', `stop' and `stats' which respectively
# start or stop the service, or provide some statistics on the requests 
# handled so far.
#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the accompanying file `LICENSE'
# for more details.
# -------------------------------------------------------------------------

package provide SOAP::Service 0.4

if { [catch {package require dom 2.0}] } {
    if { [catch {package require dom 1.6}] } {
        error "require dom package greater than 1.6"
    }
}

if { [catch {package require Trf}] } {
    if { [catch {package require base64}] } {
        error "missing required package: base64 command needs to be provided"
    }
}

# -------------------------------------------------------------------------

namespace eval SOAP::Service {
    variable version 1.0
    variable rcs_version { $Id: SOAP-service.tcl,v 1.6 2003/09/06 17:08:46 patthoyts Exp $ }
    variable socket
    variable port
    variable stats
    namespace export start stop stats
}

# -------------------------------------------------------------------------

proc SOAP::Service::start { {server_port {80}} } {
    variable socket
    variable port
    variable stats

    if { [catch { set s $socket }] != 0 } {
        set socket {}
    }
    if { $socket != {} } {
        return -code error "SOAP service already running on socket $socket"
    }

    set port $server_port
    set socket [socket -server [namespace current]::service $port]
    puts "SOAP service started on port $port"

    array set stats {
        zsplat-Base64 0
        error_404     0
        error_500     0
        fault         0
    }

    return $socket
}

# -------------------------------------------------------------------------

proc SOAP::Service::stop {} {
    variable socket
    close $socket
    set socket {}
}

# -------------------------------------------------------------------------

proc SOAP::Service::stats {} {
    variable stats
    set count 0
    foreach uri [array names stats] {
        puts "$uri $stats($uri)"
        incr count $stats($uri)
    }
    return $count
}

# -------------------------------------------------------------------------

proc SOAP::Service::service {channel client_addr client_port} {

    # read the request (if any)
    set request {}
    set line {1}
    while { $line != {} && ! [eof $channel] } {
	gets $channel line
	lappend request $line
    }

    puts "[join $request \n]"

    set http_request [split [lindex $request 0] ]
    set http_action  [lindex $http_request 0]  ;# type of request
    set http_url     [lindex $http_request 1]  ;# what URL requested
    
    switch -- $http_action {
	GET {
	    set reply [get $http_url]
	}
	POST {
            set reply [post $http_url $request $channel]
	}
	default {
	    set reply [error500]
	}
    }
    
    puts $channel "$reply"
    flush $channel
    close $channel
}

# -------------------------------------------------------------------------

proc SOAP::Service::post { url headers channel} {
    # Get the amount of data from the Content-Length header and read it.
    set data {}
    set length [lsearch -regexp $headers {^Content-Length:}]
    if { $length != -1 } {
        set length [split [lindex $headers $length] :]
        set length [expr {[lindex $length 1] + 0}]
    }

    if { $length > 0 } {
        set data [read $channel $length]
    }

    switch -- $url {
        /soap/base64 {
            set reply [base64_service $data]
        }
        default {
            set reply [error404] 
        }
    }
    return $reply
}

# -------------------------------------------------------------------------

proc SOAP::Service::get { path } {
    variable stats
    set path [eval file join [split $path {\\/}] ] ;# make it relative
    if { [file exists $path] && [file readable $path] && [file isfile $path]} {
	set body {}
	set f [open $path "r"]
	while { ! [eof $f] } {
	    gets $f l
	    lappend body $l
	}
	close $f
	set body [join $body "\n"]
	
	set head [join [list \
		"HTTP/1.1 200 OK" \
		"Content-Type: [content_type $path]" \
		"Content-Length: [string length $body]" ] "\n"]
        set reply "${head}\n\n${body}"

        if { [info exists stats($path)] } {
            incr stats($path)
        } else {
            set stats($path) 1
        }

    } else {
        set reply [error404]
    }

    return $reply
}

# -------------------------------------------------------------------------

proc SOAP::Service::content_type { file } {
    set ext [file extension $file]
    switch -- $ext {
	.htm { set type text/html }
	.xml { set type text/xml }
	.jpg { set type image/jpeg }
	.tcl { set type application/x-tcl }
	default { set type text/plain }
    }
    return $type
}

# -------------------------------------------------------------------------

proc SOAP::Service::error404 {} {
    variable stats
    incr stats(error_404)
    set body [join [list \
            "<html><head><title>File not found</title></head>"\
            "<body><h1>Error 404 File not found</h1><p>" \
            "The requested file could not be found on this server." \
            "</p></body></html>" \
            ] "\n" ]
    
    set head [join [list \
	    "HTTP/1.1 404 Error File not found" \
	    "Content-Type: text/html" \
	    "Content-Length: [string length $body]"] "\n"]
    
    return "${head}\n\n${body}"
}

# -------------------------------------------------------------------------

proc SOAP::Service::error500 {} {
    variable stats
    incr stats(error_500)

    set body [list \
	    "Requests must be GET or POST." ]
    set head [list \
	    "HTTP/1.1 500 ERROR Invalid HTTP request type" \
	    "Content-Type: text/html" \
	    "Content-Length: [string length $body]" ]
    return "[join $head \n]\n\n[join $body \n]"
}

# -------------------------------------------------------------------------

proc SOAP::Service::base64_service { request } {
    variable stats
    
    package require SOAP::xpath
    set req [dom::DOMImplementation parse $request]
    set failed [catch {SOAP::xpath::xpath $req "Envelope/Body/zsplat-Base64/*"} result]
    if { $failed } {

        set doc [dom::DOMImplementation create]
        set bod [gen_reply_envelope $doc]
        set flt [dom::document createElement $bod "SOAP-ENV:Fault"]
        set fcd [dom::document createElement $flt "faultcode"]
        dom::document createTextNode $fcd {SOAP-ENV:Client}
        set fst [dom::document createElement $flt "faultstring"]
        dom::document createTextNode $fst {Incorrect number of arguments}
        #set dtl [dom::document createElement $flt "detail"]

        set head {HTTP/1.1 500 Internal Server Error}
        incr stats(fault)
    } else {
        set doc [zsplat_base64_reply [dom::DOMImplementation create] $result]
        set head {HTTP/1.1 200 OK}
        incr stats(zsplat-Base64)
    }

    set prebody [dom::DOMImplementation serialize $doc]
    dom::DOMImplementation destroy $doc            ;# clean up
    regsub {<!DOCTYPE[^>]*>\n} $prebody {} body    ;# SOAP disallows DOCTYPE

    set head [join [list $head \
            "Content-Type: text/xml" \
            "Content-Length: [string length $body]"\
            "" ] "\n" ]
    return "${head}\n${body}"
}

# -------------------------------------------------------------------------

proc SOAP::Service::zsplat_base64_reply { doc msg } {
    set bod [gen_reply_envelope $doc]
    set cmd [dom::document createElement $bod "zsplat:getBase64"]
    dom::element setAttribute $cmd "xmlns:zsplat" "urn:zsplat-Base64"
    dom::element setAttribute $cmd \
	    "SOAP-ENV:encodingStyle" "http://schemas.xmlsoap.org/soap/encoding/"
    set par [dom::document createElement $cmd "return"]
    dom::element setAttribute $par "xsi:type" "xsd:string"
    dom::document createTextNode $par [base64 -mode enc $msg]
    return $doc
    
}

# Mostly this boilerplate code to generate a general SOAP reply

proc SOAP::Service::gen_reply_envelope { doc } {
    set env [dom::document createElement $doc "SOAP-ENV:Envelope"]
    dom::element setAttribute $env \
	    "xmlns:SOAP-ENV" "http://schemas.xmlsoap.org/soap/envelope/"
    dom::element setAttribute $env \
	    "xmlns:xsi"      "http://www.w3.org/1999/XMLSchema-instance"
    dom::element setAttribute $env \
	    "xmlns:xsd"      "http://www.w3.org/1999/XMLSchema"
    set bod [dom::document createElement $env "SOAP-ENV:Body"]
    return $bod
}

# -------------------------------------------------------------------------

# Local variables:
#   indent-tabs-mode: nil
# End:
