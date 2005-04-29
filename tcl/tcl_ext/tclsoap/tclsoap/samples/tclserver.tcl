# tclserver.tcl - Copyright (C) 2001 Pat Thoyts <Pat.Thoyts@bigfoot.com>
#
# Provide webservices from tclhttpd using the SOAP::CGI package.
# This is equivalent to the `rpc' script in cgi-bin that provides the same
# webservices via CGI.
#
# TODO:
#  This should be optimised somewhat. At the moment we are sourceing the
#  service implementation files each time a request arrives ala. CGI. However,
#  running under tclhttpd we are using the _same_ process each time. So 
#  we should source the implementations _one_ time and then simply call the 
#  procedures in the correct namespace.
#  This might need some refactoring in SOAP-CGI.tcl.
#
#
# $Id: tclserver.tcl,v 1.2 2001/08/05 22:54:25 patthoyts Exp $

package require SOAP::CGI

# Set to the cgi-bin or wherever.
set root [file join $::env(HOME) lib tcl tclsoap cgi-bin]

set SOAP::CGI::soapdir       [file join $root soap]
set SOAP::CGI::soapmapfile   [file join $root soapmap.dat]
set SOAP::CGI::xmlrpcdir     [file join $root soap]
set SOAP::CGI::xmlrpcmapfile [file join $root xmlrpcmap.dat]
catch {unset SOAP::CGI::logfile}

Url_PrefixInstall /RPC rpc_handler

proc rpc_handler {sock args} {
    upvar \#0 Httpd$sock data
    
    if {[catch {
	set query $data(query)
	set doc [dom::DOMImplementation parse $query]
	
	if {[SOAP::CGI::selectNode $doc "/Envelope"] != {}} {
	    set result [SOAP::CGI::soap_invocation $doc]
	} elseif {[SOAP::CGI::selectNode $doc "/methodCall"] != {}} {
	    set result [SOAP::CGI::xmlrpc_invocation $doc]
	}
	
	Httpd_ReturnData $sock text/xml $result 200
    } err]} {
	Httpd_ReturnData $sock text/xml $err 500
    }
}
