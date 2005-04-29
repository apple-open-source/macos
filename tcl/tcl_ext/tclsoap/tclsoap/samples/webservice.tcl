# webservice.tcl - Copyright (C) 2001 Pat Thoyts <Pat.Thoyts@bigfoot.com>
#
# Client side to the TclSOAP public webservice SOAP interface.
#
# Usage: register a SOAPAction (equates to a namespace and file)
#        upload a tclscript using 'save' to provide the implementation for you
#        SOAPaction.
#        Call your methods via cgi-bin/webservices.  The action registed will
#        define what file is sourced into the Tcl interpreter to satisfy your
#        requests.
#        If you need to recover the Tcl script, use 'read'
#        If you have had enough, use 'unregister' (when I write it!)
#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the accompanying file `LICENSE'
# for more details.
# -------------------------------------------------------------------------
#
# @(#)$Id: webservice.tcl,v 1.3 2001/12/09 23:28:59 patthoyts Exp $

package require SOAP
package require SOAP::http

namespace eval webservices {

    variable uri    urn:tclsoap:webservices
    variable action urn:tclsoap:webservices
    #variable proxy  http://tclsoap.sourceforge.net/cgi-bin/rpc
    variable proxy  http://localhost/cgi-bin/rpc

    SOAP::create register \
	    -uri $uri \
	    -proxy $proxy \
	    -action $action \
	    -params {email string passwd string action string}
    
    SOAP::create save \
	    -uri $uri \
	    -proxy $proxy \
	    -action $action \
	    -params {email string passwd string action string filedata string}

    SOAP::create read \
	    -uri $uri \
	    -proxy $proxy \
	    -action $action \
	    -params {email string passwd string action string}

    SOAP::create unregister \
	    -uri $uri \
	    -proxy $proxy \
	    -action $action \
	    -params {email string passwd string action}
}

# -------------------------------------------------------------------------
#
# Local variables:
# mode: tcl
# End:
