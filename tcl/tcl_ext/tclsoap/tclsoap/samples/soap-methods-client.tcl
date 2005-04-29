# soap-methods-client.tcl 
#                  - Copyright (C) 2001 Pat Thoyts <Pat.Thoyts@bigfoot.com>
#
#  Setup the client side of the sample services provided through the
#  SOAP::Domain package.
#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the accompanying file `LICENSE'
# for more details.
# -------------------------------------------------------------------------
#
# @(#)$Id: soap-methods-client.tcl,v 1.5 2002/02/27 21:29:14 patthoyts Exp $

package require SOAP
package require SOAP::http

# Description:
#   Setup the client methods for our sample services. Optionally specify the
#   serving host.
#
namespace eval test {
    
    variable uri urn:tclsoap:Test
    variable methods {}
    
    proc init {proxy} {
        variable methods
        variable uri

        set name rcsid
        lappend methods [ SOAP::create $name -name rcsid -uri $uri \
                              -proxy $proxy -params {} ]
        
        set name base64
        lappend methods [ SOAP::create $name -name base64 -uri $uri \
                              -proxy $proxy -params {msg string} ]
        
        set name time
        lappend methods [ SOAP::create $name -name time -uri $uri \
                              -proxy $proxy -params {} ]
        
        set name square
        lappend methods [ SOAP::create $name -name square -uri $uri \
                              -proxy $proxy -params {num double} ]
        
        set name sum
        lappend methods [ SOAP::create $name -name sum -uri $uri \
                              -proxy $proxy -params {lhs double rhs double} ]
        
        set name sort
        lappend methods [ SOAP::create $name -name sort -uri $uri \
                          -proxy $proxy -params {myArray array} ]
        
        set name platform
        lappend methods [ SOAP::create $name -name platform -uri $uri \
                              -proxy $proxy -params {} ]
        
        set name xml
        lappend methods [ SOAP::create $name -name xml -uri $uri \
                              -proxy $proxy -params {} ]
        
        return $methods
    }
}

puts "Call 'test::init endpoint-url'"

# -------------------------------------------------------------------------
#
# Local variables:
# mode: tcl
# End:
