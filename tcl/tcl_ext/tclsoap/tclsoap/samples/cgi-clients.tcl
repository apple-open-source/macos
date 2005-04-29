# cgi-clients.tcl - Copyright (C) 2001 Pat Thoyts <Pat.Thoyts@bigfoot.com>
#
# Construct the client commands for my CGI samples.
#
# If you live behind a firewall and have an authenticating proxy web server
# try executing SOAP::proxyconfig and filling in the fields. This sets
# up the SOAP package to send the correct headers for the proxy to 
# forward the packets (provided it is using the `Basic' encoding scheme).
#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the accompanying file `LICENSE'
# for more details.
# -------------------------------------------------------------------------
#
# @(#)$Id: cgi-clients.tcl,v 1.2 2001/12/08 01:19:02 patthoyts Exp $

package require SOAP
package require XMLRPC
package require SOAP::http

# -------------------------------------------------------------------------

# Some of my simple cgi test services.

proc define_sample_clients {proxy} {
    set uri    "urn:tclsoap-Test"
    set action "urn:tclsoap-Test"
    
    SOAP::create rpctime -uri $uri -name time -action $action -proxy $proxy \
            -params {}
    SOAP::create square  -uri $uri -action $action -proxy $proxy \
            -params {num double}
    SOAP::create sum  -uri $uri -action $action -proxy $proxy \
            -params {lhs double rhs double}
    SOAP::create platform -uri $uri -action $action -proxy $proxy -params {}
    SOAP::create printenv -uri $uri -action $action -proxy $proxy -params {}
    SOAP::create mistake  -uri $uri -action $action -proxy $proxy -params {}

    XMLRPC::create tclsoap.rpctime -proxy $proxy -params {}
    XMLRPC::create tclsoap.square -proxy $proxy -params {num double}
    XMLRPC::create tclsoap.sum -proxy $proxy  \
            -params {lhs double rhs double}
    XMLRPC::create tclsoap.platform -proxy $proxy -params {}
    XMLRPC::create tclsoap.printenv -proxy $proxy -params {}
    XMLRPC::create tclsoap.mistake -proxy $proxy -params {}
}

define_sample_clients http://localhost/cgi-bin/rpc

# -------------------------------------------------------------------------

# Local variables:
#   indent-tabs-mode: nil
# End: