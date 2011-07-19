# SOAP-CGI.tcl - Copyright (C) 2001 Pat Thoyts <patthoyts@users.sf.net>
#                Copyright (C) 2008 Andreas Kupries <andreask@activestate.com>
#
# A CGI framework for SOAP and XML-RPC services from TclSOAP
#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the accompanying file `LICENSE'
# for more details.
# -------------------------------------------------------------------------
#

package require SOAP
package require XMLRPC
package require SOAP::Utils
package require SOAP::http

package provide SOAP::CGI 1.0.1

namespace eval ::SOAP {
    namespace eval CGI {

	# -----------------------------------------------------------------
	# Configuration Parameters
	# -----------------------------------------------------------------
	#   soapdir   - the directory searched for SOAP methods
	#   xmlrpcdir - the directory searched for XML-RPC methods
	#   logfile   - a file to update with usage data. 
	#
	#   This framework is such that the same tcl procedure can be called 
	#   for both types of request. The result will be packaged correctly
	#   So these variables can point to the _same_ directory.
	#
	# ** Note **
	#   These directories will be relative to your httpd's cgi-bin
	#   directory.

	variable soapdir       "soap"
	variable soapmapfile   "soapmap.dat"
	variable xmlrpcdir     $soapdir
	variable xmlrpcmapfile "xmlrpcmap.dat"
	variable logfile       "rpc.log"
	
	# -----------------------------------------------------------------

	variable rcsid {
	    $Id: SOAP-CGI.tcl,v 1.17 2009/02/26 23:45:35 andreas_kupries Exp $
	}
	variable methodName  {}
	variable debugging   0
	variable debuginfo   {}
	variable interactive 0
	
	catch {namespace import -force [namespace parent]::Utils::*}

	namespace export log main
    }
}

# -------------------------------------------------------------------------

# Description:
#   Maintain a basic call log so that we can monitor for errors and 
#   popularity.
# Notes:
#   This file will need to be writable by the httpd user. This is usually
#   'nobody' on unix systems, so the logfile will need to be world writeable.
#
proc ::SOAP::CGI::log {protocol action result} {
    variable logfile
    catch {
	if {[info exists logfile] && $logfile != {} && \
		[file writable $logfile]} {
	    set stamp [clock format [clock seconds] \
		    -format {%Y%m%dT%H%M%S} -gmt true]
	    set f [open $logfile "a+"]
	    puts $f [list $stamp $protocol $action $result \
		    $::env(REMOTE_ADDR) $::env(HTTP_USER_AGENT)]
	    close $f
	}
    }
}

# -------------------------------------------------------------------------

# Description:
#   Write a complete html page to stdout, setting the content length correctly.
# Notes:
#   The string length is incremented by the number of newlines as HTTP content
#   assumes CR-NL line endings.
#
proc ::SOAP::CGI::write {html {type text/html} {status {}}} {
    variable debuginfo

    # Do some debug info:
    if {$debuginfo != {}} {
	append html "\n<!-- Debugging Information-->"
	foreach item $debuginfo {
	    append html "\n<!-- $item -->"
	}
    }

    # For errors, status should be "500 Reason Text"
    if {$status != {}} {
	puts "Status: $status"
    }

    puts "SOAPServer: TclSOAP/1.6"
    puts "Content-Type: $type"
    set len [string length $html]
    puts "X-Content-Length: $len"
    incr len [regexp -all "\n" $html]
    puts "Content-Length: $len"

    puts "\n$html"
    catch {flush stdout}
}

# -------------------------------------------------------------------------

# Description:
#   Convert a SOAPAction HTTP header value into a script filename.
#   This is used to identify the file to source for the implementation of
#   a SOAP webservice by looking through a user defined map.
#   Also used to load an equvalent map for XML-RPC based on the class name
# Result:
#   Returns the list for an array with filename, interp and classname elts.
#
proc ::SOAP::CGI::get_implementation_details {mapfile classname} {
    if {[file exists $mapfile]} {
	set f [open $mapfile r]
	while {! [eof $f] } {
	    gets $f line
	    regsub "#.*" $line {} line                 ;# delete comments.
	    regsub -all {[[:space:]]+} $line { } line  ;# fold whitespace
	    set line [string trim $line]
	    if {$line != {}} {
		set line [split $line]
		catch {unset elt}
		set elt(classname) [lindex $line 0]
		set elt(filename)  [string trim [lindex $line 1] "\""]
		set elt(interp)    [lindex $line 2]
		set map($elt(classname)) [array get elt]
	    }
	}
	close $f
    }
    
    if {[catch {set map($classname)} r]} {
	error "\"$classname\" not implemented by this endpoint."
    }

    return $r
}

proc ::SOAP::CGI::soap_implementation {SOAPAction} {
    variable soapmapfile
    variable soapdir

    if {[catch {get_implementation_details $soapmapfile $SOAPAction} detail]} {
	set xml [SOAP::fault "Client" \
		"Invalid SOAPAction header: $detail" {}]
	error $xml {} SOAP
    }
    
    array set impl $detail
    if {$impl(filename) != {}} {
	set impl(filename) [file join $soapdir $impl(filename)]
    }
    return [array get impl]
}

proc ::SOAP::CGI::xmlrpc_implementation {classname} {
    variable xmlrpcmapfile
    variable xmlrpcdir

    if {[catch {get_implementation_details $xmlrpcmapfile $classname} r]} {
	set xml [XMLRPC::fault 500 "Invalid classname: $r" {}]
	error $xml {} XMLRPC
    }

    array set impl $r
    if {$impl(filename) != {}} {
	set impl(filename) [file join $xmlrpcdir $impl(filename)]
    }
    return [array get impl]
}

proc ::SOAP::CGI::createInterp {interp path} {
    safe::setLogCmd [namespace current]::itrace
    set slave [safe::interpCreate $interp]
    safe::interpAddToAccessPath $slave $path
    # override the safe restrictions so we can load our
    # packages (actually the xml package files)
    proc ::safe::CheckFileName {slave file} {
	if {![file exists $file]} {error "file non-existent"}
	if {![file readable $file]} {error "file not readable"}
    }
    return $slave
}

# -------------------------------------------------------------------------

# Description:
#   itrace prints it's arguments to stdout if we were called interactively.
#
proc ::SOAP::CGI::itrace args {
    variable interactive
    if {$interactive} {
	puts $args
    }
}

# Description:
#   dtrace logs debug information for appending to the end of the SOAP/XMLRPC
#   response in a comment. This is not allowed by the standards so is switched
#   on by the use of the SOAPDebug header. You can enable this with:
#     SOAP::configure -transport http -headers {SOAPDebug 1}
#
proc ::SOAP::CGI::dtrace args {
    variable debuginfo
    variable debugging
    if {$debugging} {
	lappend debuginfo $args
    }
}

# -------------------------------------------------------------------------

# Description:
#   Handle UTF-8 and UTF-16 data and convert into unicode for DOM parsing
#   as necessary.
#
proc ::SOAP::CGI::do_encoding {xml} {
    if {[binary scan $xml ccc c0 c1 c2] == 3} {
	if {$c0 == -1 && $c1 == -2} {
	    dtrace "encoding: UTF-16 little endian"
	    set xml [encoding convertfrom unicode $xml]
	} elseif {$c0 == -2 && $c1 == -1} {
	    dtrace "encoding: UTF-16 big endian"
	    binary scan $xml S* xml
	    set xml [encoding convertfrom unicode [binary format s* $xml]]
	} elseif {$c0 == -17 && $c1 == -69 && $c2 == -65} {
	    dtrace "encoding: UTF-8"
	    set xml [encoding convertfrom utf-8 $xml]
	}
    }
    return $xml
}

# -------------------------------------------------------------------------

# Description:
#   Handle incoming XML-RPC requests.
#   We extract the name of the method and the arguments and search for
#   the implementation in $::xmlrpcdir. This is then evaluated and the result
#   is wrapped up and returned or a fault packet is generated.
# Parameters:
#   doc - a DOM tree constructed from the input request XML data.
#
proc ::SOAP::CGI::xmlrpc_call {doc {interp {}}} {
    variable methodName
    if {[catch {

	set methodNode [selectNode $doc "/methodCall/methodName"]
	set methodName [getElementValue $methodNode]
	set methodNamespace {}

	# Get the parameters.
	set paramsNode [selectNode $doc "/methodCall/params"]
	set argValues {}
	if {$paramsNode != {}} {
	    set argValues [decomposeXMLRPC $paramsNode]
	}
	catch {deleteDocument $doc}

	# Check for a permitted methodname. This is defined by being in the
	# XMLRPC::export list for the given namespace. We must do this to
	# prevent clients arbitrarily calling tcl commands.
	#
	if {[catch {
	    interp eval $interp \
		    set ${methodNamespace}::__xmlrpc_exports($methodName)
	} fqdn]} {
	    error "Invalid request: \
		    method \"${methodNamespace}::${methodName}\" not found"\
	}

	# evaluate the method
	set msg [interp eval $interp $fqdn $argValues]

	# generate a reply packet
	set reply [XMLRPC::reply \
		       [newDocument] \
		       {urn:xmlrpc-cgi} "${methodName}Response" $msg]

	set xml [generateXML  $reply]
	catch {deleteDocument $reply}

    } msg]} {
	set detail [list "errorCode" $::errorCode "stackTrace" $::errorInfo]
	set xml [XMLRPC::fault 500 "$msg" $detail]
	error $xml {} XMLRPC
    }

    # publish the answer
    return $xml
}

# -------------------------------------------------------------------------

# Description:
#   Handle the Head section of a SOAP request. If there is a problem we 
#   shall throw an error.
# Parameters:
#   doc
#   mandate - boolean: if true then throw an error for any mustUnderstand
#
proc ::SOAP::CGI::soap_header {doc {mandate 0}} {
    dtrace "Handling SOAP Header"
    set result {}
    foreach elt [selectNode $doc "SENV:/Envelope/SENV:Header/*"] {
	set eltName [getElementName $elt]
	set actor [getElementAttribute $elt actor]
	dtrace "SOAP actor $eltName = $actor"

	# If it's not for me, don't handle the header.
	if {$actor == "" || [string match $actor \
		"http://schemas.xmlsoap.org/soap/actor/next"]} {
	
	    # Check for Mandatory Headers.
	    set mustUnderstand [getElementAttribute $elt mustUnderstand]	    
	    dtrace "SOAP mustUnderstand $eltName $mustUnderstand"

	    # add to the list of suitable headers.
	    lappend result [getElementName $elt] [getElementValue $elt]

	    
	    ## Until we know what to do with such headers, we will have to
	    ## Fault.
	    if {$mustUnderstand == 1 && $mandate == 1} {
	    	error "Mandatory header $eltName not understood." \
	    		{} MustUnderstand
	    }
	}
    }
    return $result
}

# -------------------------------------------------------------------------

# Description:
#   Handle incoming SOAP requests.
#   We extract the name of the SOAP method and the arguments and search for
#   the implementation in the specified namespace. This is then evaluated
#   and the result is wrapped up and returned or a SOAP Fault is generated.
# Parameters:
#   doc - a DOM tree constructed from the input request XML data.
#
proc ::SOAP::CGI::soap_call {doc {interp {}}} {
    variable methodName
    set headers {}
    if {[catch {

	# Check SOAP version by examining the namespace of the Envelope elt.
	set envnode [selectNode $doc "/SENV:Envelope"]
	if {$envnode != {}} {
	    #set envns [dom::node cget $envnode -namespaceURI]
	    set envns [namespaceURI $envnode]
	    if {$envns != "" && \
		    ! [string match $envns \
		    "http://schemas.xmlsoap.org/soap/envelope/"]} {
		error "The SOAP Envelope namespace does not match the\
			SOAP version 1.1 namespace." {} VersionMismatch
	    }
	}

	# Check for Header elements
	if {[set headerNode [selectNode $doc "/SENV:Envelope/SENV:Header"]] != {}} {
	    set headers [soap_header $doc 0]
	    dtrace "headers: $headers"
	}

	# Get the method name from the XML request.
        # Ensure we only select the first child element (Vico.Klump@risa.de)
	set methodNodes [selectNode $doc "/SENV:Envelope/SENV:Body/*"]
        set methodNode [lindex $methodNodes 0]
	set methodName [nodeName $methodNode]

	# Get the XML namespace for this method.
	set methodNamespace [namespaceURI $methodNode]
	dtrace "methodinfo: ${methodNamespace}::${methodName}"

	# Extract the parameters.
	set old [$doc selectNodesNamespaces]
	$doc selectNodesNamespaces [linsert $old 0 MNAME $methodNamespace]
	set argNodes [selectNode $doc "/SENV:Envelope/SENV:Body/MNAME:${methodName}/*"]
	$doc selectNodesNamespaces $old

	set argValues {}
	foreach node $argNodes {
	    lappend argValues [decomposeSoap $node]
	}

	# Check for a permitted methodname. This is defined by being in the
	# SOAP::export list for the given namespace. We must do this to prevent
	# clients arbitrarily calling tcl commands like 'eval' or 'error'
	#
        if {[catch {
	    interp eval $interp \
		    set ${methodNamespace}::__soap_exports($methodName)
	} fqdn]} {
	    dtrace "method not found: $fqdn"
	    error "Invalid SOAP request:\
		    method \"${methodNamespace}::${methodName}\" not found" \
		{} "Client"
	}

	# evaluate the method
	set msg [interp eval $interp $fqdn $argValues]

	# check for mustUnderstand headers that were not understood.
	# This will raise an error for any such header elements.
	if {$headerNode != {}} {
	    soap_header $doc 1
	}

	# generate a reply packet
	set reply [SOAP::reply \
		[newDocument] \
		$methodNamespace "${methodName}Response" $msg]
	set xml [generateXML $reply]
	catch {deleteDocument $reply}
	catch {deleteDocument $doc}
	
    } msg]} {
	# Handle errors the SOAP way.
	#
	set detail [list "errorCode" $::errorCode "stackTrace" $::errorInfo]
	set code [lindex $detail 1]
	switch -exact -- $code {
	    "VersionMismatch" {
		set code "SOAP-ENV:VersionMismatch"
	    }
	    "MustUnderstand" {
		set code "SOAP-ENV:MustUnderstand"
	    }
	    "Client" {
		set code "SOAP-ENV:Client"
	    }
	    "Server" {
		set code "SOAP-ENV:Server"
	    }
	}
	set xml [SOAP::fault $code "$msg" $detail]
	return -code error -errorcode SOAP $xml
    }

    # publish the answer
    return $xml
}

# -------------------------------------------------------------------------

# Description:
#   Prepare the interpreter for XML-RPC method invocation. We try to identify
#   a Tcl file to source for the implementation of the method by using the 
#   XML-RPC class name (the bit before the dot) and looking it up in the
#   xmlrpcmap file. This file also tells us if we should use a safe 
#   interpreter for this method.
#
proc ::SOAP::CGI::xmlrpc_invocation {doc} {
    global env
    variable xmlrpcdir

    array set impl {filename {} interp {}}

    # Identify the classname part of the methodname
    set methodNode [selectNode $doc "/methodCall/methodName"]
    set methodName [getElementValue $methodNode]
    set className {}
    if {[regexp {.*\.} $methodName className]} {
	set className [string trim $className .]
    }
    set files {}
    if {$className != {}} {
	array set impl [xmlrpc_implementation $className]
	set files $impl(filename)
    }
    if {$files == {}} {
	set files [glob $xmlrpcdir/*]
    }
    # Do we want to use a safe interpreter?
    if {$impl(interp) != {}} {
	createInterp $impl(interp) $xmlrpcdir
    }
    dtrace "Interp: '$impl(interp)' - Files required: $files"

    # Source the XML-RPC implementation files at global level.
    foreach file $files {
	if {[file isfile $file] && [file readable $file]} {
	    itrace "debug: sourcing $file"
	    if {[catch {
		interp eval $impl(interp)\
			namespace eval :: \
			"source [list $file]"
	    } msg]} {
		itrace "warning: failed to source \"$file\""
		dtrace "failed to source \"$file\": $msg"
	    }
	}
    }
    set result [xmlrpc_call $doc $impl(interp)]
    if {$impl(interp) != {}} {
	safe::interpDelete $impl(interp)
    }
    return $result
}

# -------------------------------------------------------------------------

# Description:
#   Load in the SOAP method implementation file on the basis of the
#   SOAPAction header. We use this header plus a map file to decide
#   what file to source, or if we should source all the files in the
#   soapdir directory. The map also provides for evaluating this method in
#   a safe slave interpreter for extra security if needed.
#   See the cgi-bin/soapmap.dat file for more details.
#
proc ::SOAP::CGI::soap_invocation {doc} {
    global env
    variable soapdir

    # Obtain the SOAPAction header and strip the quotes.
    set SOAPAction {}
    if {[info exists env(HTTP_SOAPACTION)]} {
	set SOAPAction $env(HTTP_SOAPACTION)
    }
    set SOAPAction [string trim $SOAPAction "\""]
    itrace "SOAPAction set to \"$SOAPAction\""
    dtrace "SOAPAction set to \"$SOAPAction\""
    
    array set impl {filename {} interp {}}
    
    # Use the SOAPAction HTTP header to identify the files to source or
    # if it's null, source the lot.
    if {$SOAPAction == {} } {
	set files [glob [file join $soapdir *]] 
    } else {
	array set impl [soap_implementation $SOAPAction]
	set files $impl(filename)
	if {$files == {}} {
	    set files [glob [file join $soapdir *]]
	}
	itrace "interp: $impl(interp): files: $files"
	
	# Do we want to use a safe interpreter?
	if {$impl(interp) != {}} {
	    createInterp $impl(interp) $soapdir
	}
    }
    dtrace "Interp: '$impl(interp)' - Files required: $files"
    
    foreach file $files {
	if {[file isfile $file] && [file readable $file]} {
	    itrace "debug: sourcing \"$file\""
	    if {[catch {
		interp eval $impl(interp) \
			namespace eval :: \
			"source [list $file]"
	    } msg]} {
		itrace "warning: $msg"
		dtrace "Failed to source \"$file\": $msg"
	    }
	}
    }
    
    set result [soap_call $doc $impl(interp)]
    if {$impl(interp) != {}} {
	safe::interpDelete $impl(interp)
    }
    return $result
}

# -------------------------------------------------------------------------

# Description:
#    Examine the incoming data and decide which protocol handler to call.
#    Everything is evaluated in a large catch. If any errors are thrown we
#    will wrap them up in a suitable reply. At this stage we return
#    HTML for errors.
# Parameters:
#    xml - for testing purposes we can source this file and provide XML
#          as this parameter. Normally this will not be used.
#
proc ::SOAP::CGI::main {{xml {}} {debug 0}} {
    catch {package require tcllib} ;# re-eval the pkgIndex
    package require ncgi
    global env
    variable soapdir
    variable xmlrpcdir
    variable methodName
    variable debugging $debug
    variable debuginfo {}
    variable interactive 1

    if { [catch {
	
	# Get the POSTed XML data and parse into a DOM tree.
	if {$xml == {}} {
	    set xml [ncgi::query]	    
	    set interactive 0      ;# false if this is a CGI request

	    # Debugging can be set by the HTTP header "SOAPDebug: 1"
	    if {[info exists env(HTTP_SOAPDEBUG)]} {
		set debugging 1
	    }
	}

	set doc [parseXML [do_encoding $xml]]
	
	# Identify the type of request - SOAP or XML-RPC, load the
	# implementation and call.
	if {[selectNode $doc "/SENV:Envelope"] != {}} {
	    set result [soap_invocation $doc]
	    log "SOAP" $methodName "ok"
	} elseif {[selectNode $doc "/methodCall"] != {}} {
	    set result [xmlrpc_invocation $doc]
	    log "XMLRPC" $methodName "ok"
	} else {
	    deleteDocument $doc
	    error "invalid protocol: the XML data is neither SOAP not XML-RPC"
	}

	# Send the answer to the caller
	write $result text/xml

    } msg]} {
	
	# if the error was thrown from either of the protocol
	# handlers then the error code is set to indicate that the
	# message is a properly encoded SOAP/XMLRPC Fault.
	# If its a CGI problem, then be a CGI error.
	switch -- $::errorCode {
	    SOAP   {
		write $msg text/xml "500 SOAP Error"
		catch {
		    set doc [parseXML $msg]
		    set r [decomposeSoap [selectNode $doc /SENV:Envelope/SENV:Body/*]]
		} msg
		log "SOAP" [list $methodName $msg] "error" 
	    }
	    XMLRPC {
		write $msg text/xml "500 XML-RPC Error"
		catch {
		    set doc [parseXML $msg]
		    set r [getElementNamedValues [selectNode $doc \
			    /methodResponse/*]]
		} msg
		log "XMLRPC" [list $methodName $msg] "error" 
	    }
	    default {
		variable rcsid

		set html "<!doctype HTML public \"-//W3O//DTD W3 HTML 2.0//EN\">\n"
		append html "<html>\n<head>\n<title>CGI Error</title>\n</head>\n<body>"
		append html "<h1>CGI Error</h1>\n<p>$msg</p>\n"
		append html "<br />\n<pre>$::errorInfo</pre>\n"
		append html "<p><font size=\"-1\">$rcsid</font></p>"
		append html "</body>\n</html>"
		write $html text/html "500 Internal Server Error"
		
		log "unknown" [string range $xml 0 60] "error"
	    }
	}
    }
}

# -------------------------------------------------------------------------
#
# Local variables:
# mode: tcl
# End:
