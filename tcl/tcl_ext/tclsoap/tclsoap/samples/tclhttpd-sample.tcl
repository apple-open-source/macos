# tclhttpd-sample.tcl - Copyright (C) 2001 Pat Thoyts <Pat.Thoyts@bigfoot.com>
#
# Example implementing a SOAP service under tclhttpd using
# the SOAP::Domain 1.4 package
#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the accompanying file `LICENSE'
# for more details.
# -------------------------------------------------------------------------
#
# @(#)$Id: tclhttpd-sample.tcl,v 1.2 2002/02/27 21:29:14 patthoyts Exp $

# Load the SOAP service support framework
package require SOAP::Domain

# Use namespaces to isolate your methods
namespace eval urn:tclsoap:DomainTest {


    proc random {num} {
        if {[catch {expr {rand() * $num}} msg]} {
            return -code error -errorcode Client -errorinfo $msg \
                "invalid arg: \"num\" must be a number"
        }
        return [rpcvar::rpcvar float $msg]
    }

    # We have to publish the public methods...
    SOAP::export random
}

# register this service with tclhttpd
SOAP::Domain::register \
    -prefix    /domaintest \
    -namespace urn:tclsoap:DomainTest \
    -uri       urn:tclsoap:DomainTest

# We can now connect a client and call our exported methods
# e.g.:
#  SOAP::create random \
#         -proxy http://localhost:8015/domaintest \
#         -uri urn:tclsoap:DomainTest \
#         -params {num float}