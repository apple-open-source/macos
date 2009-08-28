# SOAP.tcl - Copyright (C) 2001 Pat Thoyts <patthoyts@users.sourceforge.net>
#
# Provide Tcl access to SOAP 1.1 methods.
#
# See http://tclsoap.sourceforge.net/ or doc/TclSOAP.html for usage details.
#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the accompanying file `LICENSE'
# for more details.
# -------------------------------------------------------------------------

package require http 2.0;               # tcl 8.n
package require log;                    # tcllib 1.0
package require uri;                    # tcllib 1.0
catch {package require uri::urn};       # tcllib 1.2
package require SOAP::Utils;            # TclSOAP
package require rpcvar;                 # TclSOAP 

# -------------------------------------------------------------------------

namespace eval ::SOAP {variable domVersion}
if {[catch {package require SOAP::dom 1.0} ::SOAP::domVersion]} {
    if { [catch {package require dom 2.0} ::SOAP::domVersion]} {
        if { [catch {package require dom 1.6} ::SOAP::domVersion]} {
            error "require dom package greater than 1.6"
        }
        package require SOAP::xpath;    # TclSOAP
    }
    proc ::SOAP::createDocument {name} {
        set doc [dom::DOMImplementation create]
        return [dom::document createElement $doc $name]
    }
}

# -------------------------------------------------------------------------

namespace eval ::SOAP {
    variable version 1.6.7
    variable logLevel warning
    variable rcs_version { $Id: SOAP.tcl,v 1.49 2008/02/28 22:05:55 andreas_kupries Exp $ }

    namespace export create cget dump configure proxyconfig export
    catch {namespace import -force Utils::*} ;# catch to allow pkg_mkIndex.
    catch {namespace import -force [uplevel {namespace current}]::rpcvar::*}
}

# -------------------------------------------------------------------------

# Description:
#  Register the namespace for handling SOAP methods using 'scheme' as a 
#  transport. See the http.tcl and smtp.tcl files for examples of how 
#  to plug in a new scheme.
#  A SOAP transport package requires an 'xfer' method for performing the
#  SOAP method call and a 'configure' method for setting any transport
#  specific options via SOAP::configure -transport.
#  You may also have a 'dump' method to help with debugging.
# Parameters:
#  scheme    - should be a URI scheme (in fact it must be recognised by the
#              then uri package from tcllib)
#  namespace - the namespace within which the transport methods are defined.
#
proc ::SOAP::register {scheme namespace} {
    variable transports
    set transports($scheme) $namespace
}

# Description:
# Internal method to return the namespace hosting a SOAP transport using
# the URL scheme 'scheme'.
#
proc ::SOAP::schemeloc {scheme} {
    variable transports
    if {[info exists transports($scheme)]} {
        return $transports($scheme)
    } else {
        return -code error "invalid transport scheme:\
            \"$scheme\" is not registered. Try one of [array names transports]"
    }
}

# Description:
#  Check for the existence of a SOAP Transport specific procedure.
#  If the named proc exists then the fully qualified name is returned
#  otherwise an empty string is returned.
#  Used by SOAP::destroy, SOAP::wait and others.
#
proc ::SOAP::transportHook {procVarName cmdname} {
    upvar $procVarName procvar
    
    array set URL [uri::split $procvar(proxy)]
    if {$URL(scheme) == "urn"} {
        set URL(scheme) "$a(scheme):$a(nid)"
    }
    set cmd [schemeloc $URL(scheme)]::$cmdname
    if {[info command $cmd] == {}} {
        set cmd {}
    }
    return $cmd
}
# -------------------------------------------------------------------------

# Description:
#   Called from SOAP package methods, shift up to the callers level and
#   get the fully namespace qualified name for the given proc / var
# Parameters:
#   name - the name of a Tcl entity, or list of command and arguments
# Result:
#   Fully qualified namespace path for the named entity. If the name 
#   parameter is a list the the first element is namespace qualified
#   and the remainder of the list is unchanged.
#
proc ::SOAP::qualifyNamespace {name} {
    if {$name != {}} {
        set name [lreplace $name 0 0 \
                [uplevel 2 namespace origin [lindex $name 0]]]
    }
    return $name
}

# -------------------------------------------------------------------------

# Description:
#  An interal procedure to mangle and SOAP method name and it's namespace
#  and generate a name for use as a specific SOAP variable. This ensures 
#  that similarly named methods in different namespaces do not conflict
#  within the SOAP package.
# Parameters:
#  methodName - the SOAP method name
#
proc ::SOAP::methodVarName {methodName} {
    if {[catch {uplevel 2 namespace origin $methodName} name]} {
        return -code error "invalid method name:\
            \"$methodName\" is not a SOAP method"
    }
    regsub -all {::+} $name {_} name
    return [namespace current]::$name
}

# -------------------------------------------------------------------------

# Description:
#  Set the amount of logging you would like to see. This is for debugging
#  the SOAP package. We use the tcllib log package for this so the level
#  must be one of log::levels. The default is 'warning'.
# Parameters:
#  level - one of log::levels. See the tcllib log package documentation.
#
proc ::SOAP::setLogLevel {level} {
    variable logLevel
    set logLevel $level
    log::lvSuppressLE emergency 0
    log::lvSuppressLE $logLevel 1
    log::lvSuppress $logLevel 0
    return $logLevel
}
if {[info exists SOAP::logLevel]} {
    SOAP::setLogLevel $SOAP::logLevel
}

# -------------------------------------------------------------------------

# Description:
#  Retrieve configuration variables from the SOAP package. The options
#  are all as found for SOAP::configure.
#
# FIXME: do for -transport as well!
#
proc ::SOAP::cget { args } {

    if { [llength $args] != 2 } {
        return -code error "wrong # args:\
            should be \"cget methodName optionName\""
    }

    set methodName [lindex $args 0]
    set optionName [lindex $args 1]
    set configVarName [methodVarName $methodName]

    # FRINK: nocheck
    if {[catch {set [subst $configVarName]([string trimleft $optionName "-"])} result]} {
        # kenstir@synchonicity.com: Fixed typo.
        return -code error "unknown option \"$optionName\""
    }
    return $result
}

# -------------------------------------------------------------------------

# Description:
#  Dump out information concerning the last SOAP transaction for a
#  SOAP method. What you can dump depends on the transport involved.
# Parameters:
#  ?-option?  - specify type of data to dump.
#  methodName - the SOAP method to dump data from.
# Notes:
#  Delegates to the transport namespace to a 'dump' procedure.
#
proc ::SOAP::dump {args} {
    if {[llength $args] == 1} {
        set type -reply
        set methodName [lindex $args 0]
    } elseif { [llength $args] == 2 } {
        set type [lindex $args 0]
        set methodName [lindex $args 1]
    } else {
        return -code error "wrong # args:\
           should be \"dump ?option? methodName\""
    }

    # call the transports 'dump' proc if found
    set procVarName [methodVarName $methodName]
    if {[set cmd [transportHook $procVarName dump]] != {}} {
        $cmd $methodName $type
    } else {
        return -code error "no dump available:\
            the configured transport has no 'dump' procedure defined"
    }
}

# -------------------------------------------------------------------------

# Description:
#   Configure or display a SOAP method options.
# Parameters:
#   procName - the SOAP method Tcl procedure name
#   args     - list of option name / option pairs
# Result:
#   Sets up a configuration array for the SOAP method.
#
proc ::SOAP::configure { procName args } {
    variable transports

    # The list of valid options, used in the error messsage
    set options { uri proxy params name transport action \
                  wrapProc replyProc parseProc postProc \
                  command errorCommand schemas version \
                  encoding }

    if { $procName == "-transport" } {
        set scheme [lindex $args 0]
        set config "[schemeloc $scheme]::configure"
        if {[info command $config] != {}} {
            return [eval $config [lrange $args 1 end]]
        } else {
            return -code error "invalid transport:\
                \"$scheme\" is not a valid SOAP transport method."
        }
    }

    if { [string match "-logLevel" $procName] } {
        if {[llength $args] > 0} {
            setLogLevel [lindex $args 0]
        }
        variable logLevel
        return $logLevel
    }

    # construct the name of the options array from the procName.
    set procVarName "[uplevel namespace current]::$procName"
    regsub -all {::+} $procVarName {_} procVarName
    set procVarName [namespace current]::$procVarName

    # Check that the named method has actually been defined
    if {! [array exists $procVarName]} {
        return -code error "invalid command: \"$procName\" not defined"
    }
    upvar $procVarName procvar

    # Add in transport plugin defined options and locate the
    # configuration hook procedure if one exists.
    set scheme [eval getTransportFromArgs $procVarName $args]
    if {$scheme != {}} {
        set transport_opts "[schemeloc $scheme]::method:options"
        if {[info exists $transport_opts]} {
            # FRINK: nocheck
            set options [concat $options [set $transport_opts]]
        }
        set transportHook "[schemeloc $scheme]::method:configure"
    }

    # if no args - print out the current settings.
    if { [llength $args] == 0 } {
        set r {}
        foreach opt $options {
            if {[info exists procvar($opt)]} {
                lappend r -$opt $procvar($opt)
            }
        }
        return $r
    }

    foreach {opt value} $args {
        switch -glob -- $opt {
            -uri       { set procvar(uri) $value }
            -proxy     { set procvar(proxy) $value }
            -param*    { set procvar(params) $value }
            -trans*    { set procvar(transport) $value }
            -name      { set procvar(name) $value }
            -action    { set procvar(action) $value }
            -schema*   { set procvar(schemas) $value }
            -ver*      { set procvar(version) $value }
            -enc*      { set procvar(encoding) $value }
            -wrap*     { set procvar(wrapProc) [qualifyNamespace $value] }
            -rep*      { set procvar(replyProc) [qualifyNamespace $value] }
            -parse*    { set procvar(parseProc) [qualifyNamespace $value] }
            -post*     { set procvar(postProc) [qualifyNamespace $value] }
            -com*      { set procvar(command) [qualifyNamespace $value] }
            -err*      { 
                set procvar(errorCommand) [qualifyNamespace $value] 
            }
            default {
                # might be better to delete the args as we process them
                # and then call this once with all the remaining args.
                # Still - this will work fine.
                if {[info exists transportHook] 
                    && [info command $transportHook] != {}} {
                    if {[catch {eval $transportHook $procVarName \
                                    [list $opt] [list $value]}]} {
                        return -code error "unknown option \"$opt\":\
                            must be one of ${options}"
                    }
                } else {
                    return -code error "unknown option \"$opt\":\
                        must be one of ${options}"
                }
            }
        }
    }

    if { $procvar(name) == {} } { 
        set procvar(name) $procName
    }

    # If the transport proc is not overridden then set based upon the proxy
    # scheme registered by SOAP::register.
    if { $procvar(transport) == {} } {
        set xferProc "[schemeloc $scheme]::xfer"
        if {[info command $xferProc] != {}} {
            set procvar(transport) $xferProc
        } else {
            return -code error "invalid transport:\
                \"$scheme\" is improperly registered"
        }
    } 
    
    # The default version is SOAP 1.1
    if { $procvar(version) == {} } {
        set procvar(version) SOAP1.1
    }
    # Canonicalize the SOAP version URI
    switch -glob -- $procvar(version) {
        SOAP1.1 - 1.1 {
            set procvar(version) "http://schemas.xmlsoap.org/soap/envelope/" 
        }
        SOAP1.2 - 1.2 {
            set procvar(version) "http://www.w3.org/2001/06/soap-envelope" 
        }
    }

    # Default SOAP encoding is SOAP 1.1
    if { $procvar(encoding) == {} } {
        set procvar(encoding) SOAP1.1
    }
    switch -glob -- $procvar(encoding) {
        SOAP1.1 - 1.1 {
            set procvar(encoding) "http://schemas.xmlsoap.org/soap/encoding/"
        }
        SOAP1.2 - 1.2 {
            set procvar(encoding) "http://www.w3.org/2001/06/soap-encoding" 
        }
    }

    # Select the default parser unless one is specified
    if { $procvar(parseProc) == {} } {
        set procvar(parseProc) [namespace current]::parse_soap_response
    } 

    # If no request wrapper is set, use the default SOAP wrap proc.
    if { $procvar(wrapProc) == {} } {
        set procvar(wrapProc) [namespace current]::soap_request
    }

    # Create the Tcl procedure that maps to this RPC method.
    uplevel 1 "proc $procName { args } {eval [namespace current]::invoke $procVarName \$args}"

    # return the fully qualified command name created.
    return [uplevel 1 "namespace which $procName"]
}

# -------------------------------------------------------------------------

# Description:
#  Create a Tcl wrapper for a SOAP methodcall. This constructs a Tcl command
#  and the necessary data structures to support the method call using the 
#  specified transport.
#
proc ::SOAP::create { args } {
    if { [llength $args] < 1 } {
        return -code error "wrong # args:\
            should be \"create procName ?options?\""
    } else {
        set procName [lindex $args 0]
        set args [lreplace $args 0 0]
    }

    set ns "[uplevel namespace current]::$procName"
    regsub -all {::+} $ns {_} varName
    set varName [namespace current]::$varName
    array set $varName {}
    array set $varName {uri       {}} ;# the XML namespace URI for this method 
    array set $varName {proxy     {}} ;# URL for the location of a provider
    array set $varName {params    {}} ;# name/type pairs for the parameters
    array set $varName {transport {}} ;# transport procedure for this method
    array set $varName {name      {}} ;# SOAP method name
    array set $varName {action    {}} ;# Contents of the SOAPAction header
    array set $varName {wrapProc  {}} ;# encode request into XML for sending
    array set $varName {replyProc {}} ;# post process the raw XML result
    array set $varName {parseProc {}} ;# parse raw XML and extract the values
    array set $varName {postProc  {}} ;# post process the parsed result
    array set $varName {command   {}} ;# asynchronous reply handler
    array set $varName {errorCommand {}} ;# asynchronous error handler
    array set $varName {headers   {}} ;# SOAP Head elements returned.
    array set $varName {schemas   {}} ;# List of SOAP Schemas in force
    array set $varName {version   {}} ;# SOAP Version in force (URI)
    array set $varName {encoding  {}} ;# SOAP Encoding (URI)

    set scheme [eval getTransportFromArgs $varName $args]
    if {$scheme != {}} {
        # Add any transport defined method options
        set transportOptions "[schemeloc $scheme]::method:options"
        # FRINK: nocheck
        foreach opt [set $transportOptions] {
            array set $varName [list $opt {}]
        }
        
        # Call any transport defined construction proc
        set createHook "[schemeloc $scheme]::method:create"
        if {[info command $createHook] != {}} {
            eval $createHook $varName $args
        }
    }

    # call configure from the callers level so it can get the namespace.
    return [uplevel 1 "[namespace current]::configure $procName $args"]
}

# Identify the transport protocol so we can include transport specific
# creation code.
proc getTransportFromArgs {procVarName args} {
    upvar $procVarName procvar
    set uri {}
    set scheme {}
    if {$procvar(proxy) != {}} {
        set uri $procvar(proxy)
    } elseif {[set n [lsearch -exact $args -proxy]] != -1} {
        incr n
        set uri [lindex $args $n]
    }
    if {$uri != {}} {
        array set URL [uri::split $uri]
        if {$URL(scheme) == "urn"} {
            set URL(scheme) $URL(scheme):$URL(nid)
        }
        set scheme $URL(scheme)
    }
    return $scheme
}

# -------------------------------------------------------------------------

# Description:
#   Export a list of procedure names as SOAP endpoints. This is only used
#   in the SOAP server code to specify the subset of Tcl commands that should
#   be accessible via a SOAP call.
# Parameters:
#   args - a list of tcl commands to be made available as SOAP endpoints.
#
proc ::SOAP::export {args} {
    foreach item $args {
        uplevel "set \[namespace current\]::__soap_exports($item)\
                \[namespace code $item\]"
    }
    return
}

# -------------------------------------------------------------------------

# Description:
#  Reverse the SOAP::create command by deleting the SOAP method binding and
#  freeing up any allocated resources. This needs to delegate to the
#  transports cleanup procedure if one is defined as well.
# Parameters:
#  methodName - the name of the SOAP method command
#
proc ::SOAP::destroy {methodName} {
    set procVarName [methodVarName $methodName]

    # Delete the SOAP command
    uplevel rename $methodName {{}}

    # Call the transport specific method destructor (if any)
    if {[set cmd [transportHook $procVarName method:destroy]] != {}} {
        $cmd $procVarName
    }

    # Delete the SOAP method configuration array
    # FRINK: nocheck
    unset $procVarName
}

# -------------------------------------------------------------------------

# Description:
#  Wait for any pending asynchronous method calls.
# Parameters:
#  methodName - the method binding we are interested in.
#
proc ::SOAP::wait {methodName} {
    set procVarName [methodVarName $methodName]

    # Call the transport specific method wait proc (if any)
    if {[set cmd [transportHook $procVarName wait]] != {}} {
        $cmd $procVarName
    }
}

# -------------------------------------------------------------------------

# Description:
#   Make a SOAP method call using the configured transport.
#   See also 'invoke2' for the reply handling which may be asynchronous.
# Parameters:
#   procName  - the SOAP method configuration variable path
#   args      - the parameter list for the SOAP method call
# Returns:
#   Returns the parsed and processed result of the method call
#
proc ::SOAP::invoke { procVarName args } {
    set procName [lindex [split $procVarName {_}] end]
    if {![array exists $procVarName]} {
        return -code error "invalid command: \"$procName\" not defined"
    }

    upvar $procVarName procvar

    # Get the URL
    set url $procvar(proxy)

    # Get the XML data containing our request by calling the -wrapProc 
    # procedure
    set req [eval "$procvar(wrapProc) $procVarName $args"]

    # Send the SOAP packet (req) using the configured transport procedure
    set transport $procvar(transport)
    set reply [$transport $procVarName $url $req]

    # Check for an async command handler. If async then return now,
    # otherwise call the invoke second stage immediately.
    if { $procvar(command) != {} } {
        return $reply
    }
    return [invoke2 $procVarName $reply]
}

# -------------------------------------------------------------------------

# Description:
#   The second stage of the method invocation deals with unwrapping the
#   reply packet that has been received from the remote service.
# Parameters:
#   procVarName - the SOAP method configuration variable path
#   reply       - the raw data returned from the remote service
# Notes:
#   This has been separated from `invoke' to support asynchronous
#   transports. It calls the various unwrapping hooks in turn.
#
proc ::SOAP::invoke2 {procVarName reply} {
    set ::lastReply $reply

    set procName [lindex [split $procVarName {_}] end]
    upvar $procVarName procvar

    # Post-process the raw XML using -replyProc
    if { $procvar(replyProc) != {} } {
        set reply [$procvar(replyProc) $procVarName $reply]
    }

    # Call the relevant parser to extract the returned values
    set parseProc $procvar(parseProc)
    if { $parseProc == {} } {
        set parseProc parse_soap_response
    }
    set r [$parseProc $procVarName $reply]

    # Post process the parsed reply using -postProc
    if { $procvar(postProc) != {} } {
        set r [$procvar(postProc) $procVarName $r]
    }

    return $r
}

# -------------------------------------------------------------------------

# Description:
#   Dummy SOAP transports to examine the SOAP requests generated for use
#   with the test package and for debugging.
# Parameters:
#   procVarName  - SOAP method name configuration variable
#   url          - URL of the remote server method implementation
#   soap         - the XML payload for this SOAP method call
#
namespace eval SOAP::Transport::print {
    variable method:options {}
    proc configure {args} {
        return
    }
    proc xfer { procVarName url soap } {
        puts "$soap"
    }
    SOAP::register urn:print [namespace current]
}

namespace eval SOAP::Transport::reflect {
    variable method:options {}
    proc configure {args} {
        return
    }
    proc xfer {procVarName url soap} {
        return $soap
    }
    SOAP::register urn:reflect [namespace current]
}

# -------------------------------------------------------------------------

# Description:
#   Setup SOAP HTTP transport for an authenticating proxy HTTP server.
#   At present the SOAP package only supports Basic authentication and this
#   dialog is used to configure the proxy information.
# Parameters:
#   none

proc ::SOAP::proxyconfig {} {
    package require Tk
    if { [catch {package require base64}] } {
        return -code error "proxyconfig requires the tcllib base64 package."
    }
    toplevel .tx
    wm title .tx "Proxy Authentication Configuration"
    set m [message .tx.m1 -relief groove -justify left -width 6c -aspect 200 \
            -text "Enter details of your proxy server (if any) and your\
                   username and password if it is needed by the proxy."]
    set f1 [frame .tx.f1]
    set f2 [frame .tx.f2]
    button $f2.b -text "OK" -command {destroy .tx}
    pack $f2.b -side right
    label $f1.l1 -text "Proxy (host:port)"
    label $f1.l2 -text "Username"
    label $f1.l3 -text "Password"
    entry $f1.e1 -textvariable SOAP::conf_proxy
    entry $f1.e2 -textvariable SOAP::conf_userid
    entry $f1.e3 -textvariable SOAP::conf_passwd -show {*}
    grid $f1.l1 -column 0 -row 0 -sticky e
    grid $f1.l2 -column 0 -row 1 -sticky e
    grid $f1.l3 -column 0 -row 2 -sticky e
    grid $f1.e1 -column 1 -row 0 -sticky news
    grid $f1.e2 -column 1 -row 1 -sticky news
    grid $f1.e3 -column 1 -row 2 -sticky news
    grid columnconfigure $f1 1 -weight 1
    pack $f2 -side bottom -fill x
    pack $m  -side top -fill x -expand 1
    pack $f1 -side top -anchor n -fill both -expand 1
    
    #bind .tx <Enter> "$f2.b invoke"

    tkwait window .tx
    SOAP::configure -transport http -proxy $SOAP::conf_proxy
    if { [info exists SOAP::conf_userid] } {
        SOAP::configure -transport http \
            -headers [list "Proxy-Authorization" \
            "Basic [lindex [base64::encode ${SOAP::conf_userid}:${SOAP::conf_passwd}] 0]" ]
    }
    unset SOAP::conf_passwd
}

# -------------------------------------------------------------------------

# Description:
#   Prepare a SOAP fault message
# Parameters:
#   faultcode   - the SOAP faultcode e.g: SOAP-ENV:Client
#   faultstring - summary of the fault
#   detail      - list of {detailName detailInfo}
# Result:
#   returns the XML text of the SOAP Fault packet.
# 
proc ::SOAP::fault {faultcode faultstring {detail {}}} {
    set doc [dom::DOMImplementation create]
    set bod [reply_envelope $doc]
    set flt [dom::document createElement $bod "SOAP-ENV:Fault"]
    set fcd [dom::document createElement $flt "faultcode"]
    dom::document createTextNode $fcd $faultcode
    set fst [dom::document createElement $flt "faultstring"]
    dom::document createTextNode $fst $faultstring

    if { $detail != {} } {
        set dtl0 [dom::document createElement $flt "detail"]
        set dtl  [dom::document createElement $dtl0 "e:errorInfo"]
        dom::element setAttribute $dtl "xmlns:e" "urn:TclSOAP-ErrorInfo"
        
        foreach {detailName detailInfo} $detail {
            set err [dom::document createElement $dtl $detailName]
            dom::document createTextNode $err $detailInfo
        }
    }
    
    # serialize the DOM document and return the XML text
    regsub "<!DOCTYPE\[^>\]*>\n" [dom::DOMImplementation serialize $doc] {} r
    dom::DOMImplementation destroy $doc
    return $r
}

# -------------------------------------------------------------------------

# Description:
#   Generate the common portion of a SOAP replay packet
# Parameters:
#   doc   - the document element of a DOM document
# Result:
#   returns the body node
#
proc ::SOAP::reply_envelope { doc } {
    set env [dom::document createElement $doc "SOAP-ENV:Envelope"]
    dom::element setAttribute $env \
            "xmlns:SOAP-ENV" "http://schemas.xmlsoap.org/soap/envelope/"
    dom::element setAttribute $env \
            "xmlns:xsi"      "http://www.w3.org/1999/XMLSchema-instance"
    dom::element setAttribute $env \
            "xmlns:xsd"      "http://www.w3.org/1999/XMLSchema"
    dom::element setAttribute $env \
            "xmlns:SOAP-ENC" "http://schemas.xmlsoap.org/soap/encoding/"
    set bod [dom::document createElement $env "SOAP-ENV:Body"]
    return $bod
}

# -------------------------------------------------------------------------

# Description:
#   Generate a SOAP reply packet. Uses 'rpcvar' variable type information to
#   manage complex data structures and arrays.
# Parameters:
#   doc         empty DOM document element
#   uri         URI of the SOAP method
#   methodName  the SOAP method name
#   result      the reply data
# Result:
#   returns the DOM document root
#
proc ::SOAP::reply { doc uri methodName result } {
    set bod [reply_envelope $doc]
    set cmd [dom::document createElement $bod "ns:$methodName"]
    dom::element setAttribute $cmd "xmlns:ns" $uri
    dom::element setAttribute $cmd \
            "SOAP-ENV:encodingStyle" \
            "http://schemas.xmlsoap.org/soap/encoding/"

    # insert the results into the DOM tree (unless it's a void result)
    if {$result != {}} {
        # Some methods may return a parameter list of name - value pairs.
        if {[rpctype $result] == "PARAMLIST"} {
            foreach {resultName resultValue} [rpcvalue $result] {
                set retnode [dom::document createElement $cmd $resultName]
                SOAP::insert_value $retnode $resultValue
            }
        } else {
            set retnode [dom::document createElement $cmd "return"]
            SOAP::insert_value $retnode $result
        }
    }

    return $doc
}

# -------------------------------------------------------------------------

# Description:
#   Procedure to generate the XML data for a configured SOAP procedure.
#   This is the default SOAP -wrapProc procedure
# Parameters:
#   procVarName - the path of the SOAP method configuration variable
#   args        - the arguments for this SOAP method
# Result:
#   XML data containing the SOAP method call.
# Notes:
#   We permit a small number of option to be specified on the method call
#   itself. -headers is used to set SOAP Header elements and -attr can be
#   used to set additional XML attributes on the method element (needed for
#   UDDI.)
#
proc ::SOAP::soap_request {procVarName args} {
    upvar $procVarName procvar

    set procName [lindex [split $procVarName {_}] end]
    set params  $procvar(params)
    set name    $procvar(name)
    set uri     $procvar(uri)
    set soapenv $procvar(version)
    set soapenc $procvar(encoding)

    # Check for options (ie: -header) give up on the fist non-matching arg.
    array set opts {-headers {} -attributes {}}
    while {[string match -* [lindex $args 0]]} {
        switch -glob -- [lindex $args 0] {
            -header* {
                set opts(-headers) [concat $opts(-headers) [lindex $args 1]]
                set args [lreplace $args 0 0]
            }
            -attr* {
                set opts(-attributes) [concat $opts(-attributes) [lindex $args 1]]
                set args [lreplace $args 0 0]
            }
            -- {
                set args [lreplace $args 0 0]
                break
            }
            default {
                # stop option processing at the first invalid option.
                break
            }
        }
        set args [lreplace $args 0 0]
    }

    # check for variable number of params and set the num required.
    if {[lindex $params end] == "args"} {
        set n_params [expr {( [llength $params] - 1 ) / 2}]
    } else {
        set n_params [expr {[llength $params] / 2}]
    }

    # check we have the correct number of parameters supplied.
    if {[llength $args] < $n_params} {
        set msg "wrong # args: should be \"$procName"
        foreach { id type } $params {
            append msg " " $id
        }
        append msg "\""
        return -code error $msg
    }

    set doc [dom::DOMImplementation create]
    set envx [dom::document createElement $doc "SOAP-ENV:Envelope"]

    dom::element setAttribute $envx "xmlns:SOAP-ENV" $soapenv
    dom::element setAttribute $envx "xmlns:SOAP-ENC" $soapenc
    dom::element setAttribute $envx "SOAP-ENV:encodingStyle" $soapenc

    # The set of namespaces depends upon the SOAP encoding as specified by
    # the encoding option and the user specified set of relevant schemas.
    foreach {nsname url} [concat \
                              [rpcvar::default_schemas $soapenc] \
                              $procvar(schemas)] {
        if {! [string match "xmlns:*" $nsname]} {
            set nsname "xmlns:$nsname"
        }
        dom::element setAttribute $envx $nsname $url
    }

    # Insert the Header elements (if any)
    if {$opts(-headers) != {}} {
        set headelt [dom::document createElement $envx "SOAP-ENV:Header"]
        foreach {hname hvalue} $opts(-headers) {
            set hnode [dom::document createElement $headelt $hname]
            insert_value $hnode $hvalue
        }
    }

    # Insert the body element and atributes.
    set bod [dom::document createElement $envx "SOAP-ENV:Body"]
    if {$uri == ""} {
        # don't use a namespace prefix if we don't have a namespace.
        set cmd [dom::document createElement $bod "$name" ]
    } else {
        set cmd [dom::document createElement $bod "ns:$name" ]
        dom::element setAttribute $cmd "xmlns:ns" $uri
    }

    # Insert any method attributes
    if {$opts(-attributes) != {}} {
        foreach {atname atvalue} $opts(-attributes) {
            dom::element setAttribute $cmd $atname $atvalue
        }
    }

    # insert the parameters.
    set param_no 0
    foreach {key type} $params {
        set val [lindex $args $param_no]
        set d_param [dom::document createElement $cmd $key]
        insert_value $d_param [rpcvar $type $val]
        incr param_no
    }

    # We have to strip out the DOCTYPE element though. It would be better to
    # remove the DOM node for this, but that didn't work.
    set prereq [dom::DOMImplementation serialize $doc]
    set req {}
    dom::DOMImplementation destroy $doc              ;# clean up
    regsub "<!DOCTYPE\[^>\]*>\r?\n?" $prereq {} req  ;# hack

    set req [encoding convertto utf-8 $req]          ;# make it UTF-8
    return $req                                      ;# return the XML data
}

# -------------------------------------------------------------------------

# Description:
#   Procedure to generate the XML data for a configured XML-RPC procedure.
# Parameters:
#   procVarName - the name of the XML-RPC method variable
#   args        - the arguments for this RPC method
# Result:
#   XML data containing the XML-RPC method call.
#
proc ::SOAP::xmlrpc_request {procVarName args} {
    upvar $procVarName procvar

    set procName [lindex [split $procVarName {_}] end]
    set params $procvar(params)
    set name   $procvar(name)
    
    if { [llength $args] != [expr { [llength $params] / 2 } ]} {
        set msg "wrong # args: should be \"$procName"
        foreach { id type } $params {
            append msg " " $id
        }
        append msg "\""
        return -code error $msg
    }
    
    set doc [dom::DOMImplementation create]
    set d_root [dom::document createElement $doc "methodCall"]
    set d_meth [dom::document createElement $d_root "methodName"]
    dom::document createTextNode $d_meth $name
    
    if { [llength $params] != 0 } {
        set d_params [dom::document createElement $d_root "params"]
    }
    
    set param_no 0
    foreach {key type} $params {
        set val [lindex $args $param_no]
        set d_param [dom::document createElement $d_params "param"]
        XMLRPC::insert_value $d_param [rpcvar $type $val]
        incr param_no
    }

    # We have to strip out the DOCTYPE element though. It would be better to
    # remove the DOM element, but that didn't work.
    set prereq [dom::DOMImplementation serialize $doc]
    set req {}
    dom::DOMImplementation destroy $doc          ;# clean up
    regsub "<!DOCTYPE\[^>\]*>\n" $prereq {} req  ;# hack

    return $req                                  ;# return the XML data
}

# -------------------------------------------------------------------------

# Description:
#   Parse a SOAP response payload. Check for Fault response otherwise 
#   extract the value data.
# Parameters:
#   procVarName  - the name of the SOAP method configuration variable
#   xml          - the XML payload of the response
# Result:
#   The returned value data.
# Notes:
#   Needs work to cope with struct or array types.
#
proc ::SOAP::parse_soap_response { procVarName xml } {
    upvar $procVarName procvar

    # Sometimes Fault packets come back with HTTP code 200
    #
    # kenstir@synchronicity.com: Catch xml parse errors and present a
    #   friendlier message.  The parse method throws awful messages like
    #   "{invalid attribute list} around line 16".
    if {$xml == {} && ![string match "http*" $procvar(proxy)]} {
        # This is probably not an error. SMTP and FTP won't return anything
        # HTTP should always return though (I think).
        return {}
    } else {
        if {[catch {set doc [dom::DOMImplementation parse $xml]}]} {
            return -code error -errorcode Server \
                "Server response is not well-formed XML.\nresponse was $xml"
        }
    }

    set faultNode [selectNode $doc "/Envelope/Body/Fault"]
    if {$faultNode != {}} {
        array set fault [decomposeSoap $faultNode]
        dom::DOMImplementation destroy $doc
        if {![info exists fault(detail)]} { set fault(detail) {}}
        return -code error -errorinfo $fault(detail) \
            [list $fault(faultcode) $fault(faultstring)]
    }

    # If there is a header element then make it available via SOAP::getHeader
    set headerNode [selectNode $doc "/Envelope/Header"]
    if {$headerNode != {} \
            && [string match \
                    "http://schemas.xmlsoap.org/soap/envelope/" \
                    [namespaceURI $headerNode]]} {
        set procvar(headers) [decomposeSoap $headerNode]
    } else {
        set procvar(headers) {}
    }
    
    set result {}

    if {[info exists procvar(name)]} {
        set responseName "$procvar(name)Response"
    } else {
        set responseName "*"
    }
    set responseNode [selectNode $doc "/Envelope/Body/$responseName"]
    if {$responseNode == {}} {
        set responseNode [lindex [selectNode $doc "/Envelope/Body/*"] 0]
    }

    set nodes [getElements $responseNode]
    foreach node $nodes {
        set r [decomposeSoap $node]
        if {$result == {}} { set result $r } else { lappend result $r }
    }

    dom::DOMImplementation destroy $doc
    return $result
}

# -------------------------------------------------------------------------

# Description:
#   Parse an XML-RPC response payload. Check for fault response otherwise 
#   extract the value data.
# Parameters:
#   procVarName  - the name of the XML-RPC method configuration variable
#   xml          - the XML payload of the response
# Result:
#   The extracted value(s). Array types are converted into lists and struct
#   types are turned into lists of name/value pairs suitable for array set
# Notes:
#   The XML-RPC fault response doesn't allow us to add in extra values
#   to the fault struct. So where to put the servers errorInfo?
#
proc ::SOAP::parse_xmlrpc_response { procVarName xml } {
    upvar $procVarName procvar
    set result {}
    if {$xml == {} && ![string match "http*" $procvar(proxy)]} {
        # This is probably not an error. SMTP and FTP won't return anything
        # HTTP should always return though (I think).
        return {}
    } else {
        if {[catch {set doc [dom::DOMImplementation parse $xml]}]} {
            return -code error -errorcode Server \
                "Server response is not well-formed XML.\n\
                  response was $xml"
        }
    }

    set faultNode [selectNode $doc "/methodResponse/fault"]
    if {$faultNode != {}} {
        array set err [lindex [decomposeXMLRPC \
                [selectNode $doc /methodResponse]] 0]
        dom::DOMImplementation destroy $doc
        return -code error \
            -errorcode $err(faultCode) \
            -errorinfo $err(faultString) \
            "Received XML-RPC Error"
    }
    
    # Recurse over each params/param/value
    set n_params 0
    foreach valueNode [selectNode $doc \
            "/methodResponse/params/param/value"] {
        lappend result [xmlrpc_value_from_node $valueNode]
        incr n_params
    }
    dom::DOMImplementation destroy $doc

    # If (as is usual) there is only one param, simplify things for the user
    # ie: sort {one two three} should return a 3 element list, not a single
    # element list whose first element has 3 elements!
    if {$n_params == 1} {set result [lindex $result 0]}
    return $result
}

# -------------------------------------------------------------------------
# Description:
#   Parse an XML-RPC call payload. Extracts method name and parameters.
# Parameters:
#   procVarName  - the name of the XML-RPC method configuration variable
#   xml          - the XML payload of the response
# Result:
#   A list containing the name of the called method as first element
#   and the extracted parameter(s) as second element. Array types are
#   converted into lists and struct types are turned into lists of
#   name/value pairs suitable for array set
# Notes:
#
proc ::SOAP::parse_xmlrpc_request { xml } {
    set result {}
    if {[catch {set doc [dom::DOMImplementation parse $xml]}]} {
        return -code error -errorinfo Server \
            "Client request is not well-formed XML.\n\
            call was $xml"
    }

    set methodNode [selectNode $doc "/methodCall/methodName"]
    set methodName [getElementValue $methodNode]

    # Get the parameters.

    # If there is only one parameter, simplify things for the user,
    # ie: sort {one two three} should return a 3 element list, not a
    # single element list whose first element has 3 elements!

    set paramsNode [selectNode $doc "/methodCall/params"]
    set paramValues {}
    if {$paramsNode != {}} {
	set paramValues [decomposeXMLRPC $paramsNode]
    }
    if {[llength $paramValues] == 1} {
        set paramValues [lindex $paramValues 0]
    }

    catch {dom::DOMImplementation destroy $doc}

    return [list $methodName $paramValues]
}

# -------------------------------------------------------------------------

### NB: this procedure needs to be moved into XMLRPC namespace

# Description:
#   Retrieve the value under the given <value> node.
# Parameters:
#   valueNode - reference to a <value> element in the response dom tree
# Result:
#   Either a single value or a list of values. Arrays expand into a list
#   of values, structs to a list of name/value pairs.
# Notes:
#   Called recursively when processing arrays and structs.
#
proc ::SOAP::xmlrpc_value_from_node {valueNode} {
    set value {}
    set elts [getElements $valueNode]

    if {[llength $elts] != 1} {
        return [getElementValue $valueNode]
    }
    set typeElement [lindex $elts 0]
    set type [dom::node cget $typeElement -nodeName]

    if {$type == "array"} {
        set dataElement [lindex [getElements $typeElement] 0]
        foreach valueElement [getElements $dataElement] {
            lappend value [xmlrpc_value_from_node $valueElement]
        }
    } elseif {$type == "struct"} {
        # struct type has 1+ members which have a name and a value elt.
        foreach memberElement [getElements $typeElement] {
            set params [getElements $memberElement]
            foreach param $params {
                set nodeName [dom::node cget $param -nodeName]
                if { $nodeName == "name"} {
                    set pname [getElementValue $param]
                } elseif { $nodeName == "value" } {
                    set pvalue [xmlrpc_value_from_node $param]
                }
            }
            lappend value $pname $pvalue
        }
    } else {
        set value [getElementValue $typeElement]
    }
    return $value
}

# -------------------------------------------------------------------------

proc ::SOAP::insert_headers {node headers} {
    set doc [SOAP::Utils::getDocumentElement $node]
    if {[set h [selectNode $doc /Envelope/Header]] == {}} {
        set e [dom::document cget $doc -documentElement]
        set h [dom::document createElement $e "SOAP-ENV:Header"]
    }
    foreach {name value} $headers {
        if {$name != {}} {
            set elt [dom::document createElement $h $name]
            insert_value $elt $value
        }
    }
}

# -------------------------------------------------------------------------

proc ::SOAP::insert_value {node value} {

    set type     [rpctype $value]
    set subtype  [rpcsubtype $value]
    set attrs    [rpcattributes $value]
    set headers  [rpcheaders $value]
    set value    [rpcvalue $value]
    set typeinfo [typedef -info $type]
    set typexmlns [typedef -namespace $type]

    # Handle any header elements
    if {$headers != {}} {
        insert_headers $node $headers
    }
    
    # If the rpcvar namespace is a URI then assign it a tag and ensure we
    # have our colon only when required.
    if {$typexmlns != {} && [regexp : $typexmlns]} {
        dom::element setAttribute $node "xmlns:t" $typexmlns
        set typexmlns t
    }
    if {$typexmlns != {}} { append typexmlns : }

    # If there are any attributes assigned, apply them.
    if {$attrs != {}} {
        foreach {aname avalue} $attrs {
            dom::element setAttribute $node $aname $avalue
        }
    }

    if {[string match {*()} $typeinfo] || [string match {*()} $type] 
        || [string match array $type]} {
        # array type: arrays are indicated by one or more () suffixes or
        # the word 'array' (depreciated)

        if {[string length $typeinfo] == 0} {
            set dimensions [regexp -all -- {\(\)} $type]
            set itemtype [string trimright $type ()]
            if {$itemtype == "array"} {
                set itemtype ur-type
                set dimensions 1
            }
        } else {
            set dimensions [regexp -all -- {\(\)} $typeinfo]
            set itemtype [string trimright $typeinfo ()]
        }
        
        # Look up the typedef info of the item type
        set itemxmlns [typedef -namespace $itemtype]
        if {$itemxmlns != {} && [regexp : $itemxmlns]} {
            dom::element setAttribute $node "xmlns:i" $itemxmlns
            set itemxmlns i
        }
        
        # Currently we do not support non-0 offsets into the array.
        # This is because I don;t know how I should present this to the
        # user. It's got to be a dynamic attribute on the value.
        dom::element setAttribute $node \
                "xmlns:SOAP-ENC" "http://schemas.xmlsoap.org/soap/encoding/"
        dom::element setAttribute $node "xsi:type" "SOAP-ENC:Array"
        dom::element setAttribute $node "SOAP-ENC:offset" "\[0\]"

        # we need to break a multi-dim array into r0c0,r0c1,r1c0,r1c1
        # so list0 followed by list1 etc.
        # FIX ME
        set arrayType "$itemxmlns:$itemtype"
        #for {set cn 0} {$cn < $dimensions} {incr cn}
        append arrayType "\[[llength $value]\]"
        dom::element setAttribute $node "SOAP-ENC:arrayType" $arrayType

        foreach elt $value {
            set d_elt [dom::document createElement $node "item"]
            if {[string match "ur-type" $itemtype]} {
                insert_value $d_elt $elt
            } else {
                insert_value $d_elt [rpcvar $itemtype $elt]
            }
        }
    } elseif {[llength $typeinfo] > 1} {
        # a typedef'd struct.
        if {$typexmlns != {}} {
            dom::element setAttribute $node "xsi:type" "${typexmlns}${type}"
        }
        array set ti $typeinfo
        # Bounds checking - <simon@e-ppraisal.com>
        if {[llength $typeinfo] != [llength $value]} {
            return -code error "wrong # args:\
                type $type contains \"$typeinfo\""
        }
        foreach {eltname eltvalue} $value {
            set d_elt [dom::document createElement $node $eltname]
            if {![info exists ti($eltname)]} {
                return -code error "invalid member name:\
                    \"$eltname\" is not a member of the $type type."
            }
            insert_value $d_elt [rpcvar $ti($eltname) $eltvalue]
        }
    } elseif {$type == "struct"} {
        # an unspecified struct
        foreach {eltname eltvalue} $value {
            set d_elt [dom::document createElement $node $eltname]
            insert_value $d_elt $eltvalue
        }
    } else {
        # simple type or typedef'd enumeration
        if {$typexmlns != {}} {
            dom::element setAttribute $node "xsi:type" "${typexmlns}${type}"
        }
        dom::document createTextNode $node $value
    }
}

# -------------------------------------------------------------------------

package provide SOAP $::SOAP::version

if {[catch {package present SOAP::http}]} {
    package require SOAP::http;             # TclSOAP 1.6.2+
}

# -------------------------------------------------------------------------

# Local variables:
#    indent-tabs-mode: nil
# End:
