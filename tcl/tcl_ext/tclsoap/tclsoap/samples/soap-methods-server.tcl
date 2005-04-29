# soap-methods-server.tcl
#                   - Copyright (C) 2001 Pat Thoyts <Pat.Thoyts@bigfoot.com>
#
# Provides examples of SOAP methods for use with SOAP::Domain under the
# tclhttpd web sever.
#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the accompanying file `LICENSE'
# for more details.
# -------------------------------------------------------------------------
#
# @(#)$Id: soap-methods-server.tcl,v 1.4 2002/02/27 21:29:14 patthoyts Exp $

# Load the SOAP URL domain handler into the web server and register it under
# the /soap URL. All methods need to be defined in the SOAP::Domain
# namespace and begin with /. Thus my /base64 procedure will be called 
# via the URL http://localhost:8015/soap/base64
#
package require SOAP::Domain
package require rpcvar
package require base64

SOAP::Domain::register          \
    -prefix    /soap            \
    -namespace urn:tclsoap:Test \
    -uri       urn:tclsoap:Test

namespace eval urn:tclsoap:Test {

    namespace import -force ::rpcvar::*

    SOAP::export base64 time rcsid square sum sort platform xml

}

# -------------------------------------------------------------------------
# base64 - convert the input string parameter to a base64 encoded string
#
proc urn:tclsoap:Test::base64 {text} {
    return [rpcvar base64 [base64::encode $text]]
}

# -------------------------------------------------------------------------
# time - return the servers idea of the time
#
proc urn:tclsoap:Test::time {} {
    return [clock format [clock seconds]]
}

# -------------------------------------------------------------------------
# rcsid - return the RCS version string for this package
#
proc urn:tclsoap:Test::rcsid {} {
    return ${::SOAP::Domain::rcs_id}
}

# -------------------------------------------------------------------------
# square - test validation of numerical methods.
#
proc urn:tclsoap:Test::square {num} {
    if { [catch {expr $num + 0}] } {
        return -code error -errorcode Client "parameter num must be a number"
    }
    return [expr {$num * $num}]
}

# -------------------------------------------------------------------------
# sum - test two parameter method
#
proc urn:tclsoap:Test::sum {lhs rhs} {
    return [expr {$lhs + $rhs}]
}

# -------------------------------------------------------------------------
# sort - sort a list
#
proc urn:tclsoap:Test::sort {myArray} {
    return [rpcvar "array" [lsort $myArray]]
}

# -------------------------------------------------------------------------
# platform - return a structure.
#
proc urn:tclsoap:Test::platform {} {
    return [rpcvar struct ::tcl_platform]
}

# -------------------------------------------------------------------------
# xml - return some XML data. Just to show it's not a problem.
#
proc urn:tclsoap:Test::xml {} {
    set xml {<?xml version="1.0" ?>
<memos>
   <memo>
      <subject>test memo one</subject>
      <body>The body of the memo.</body>
   </memo>
   <memo>
      <subject>test memo two</subject>
      <body>Memo body with specials: &quot; &amp; &apos; and &lt;&gt;</body>
   </memo>
</memos>
}
    return $xml
}
