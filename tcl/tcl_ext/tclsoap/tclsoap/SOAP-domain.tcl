# SOAP-domain.tcl - Copyright (C) 2001 Pat Thoyts <patthoyts@users.sf.net>
#
# SOAP Domain Service module for the tclhttpd web server.
#
# Get the server to require the SOAP::Domain package and call 
# SOAP::Domain::register to register the domain handler with the server.
# ie: put the following in a file in tclhttpd/custom
#    package require SOAP::Domain
#    SOAP::Domain::register /soap
#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the accompanying file `LICENSE'
# for more details.
# -------------------------------------------------------------------------

package require SOAP::CGI;              # TclSOAP 1.6
package require rpcvar;                 # TclSOAP 1.6
package require log;                    # tcllib 1.0

namespace eval ::SOAP::Domain {
    variable version 1.4  ;# package version number
    variable debug 0      ;# flag to toggle debug output
    variable rcs_id {$Id: SOAP-domain.tcl,v 1.14 2003/09/06 17:08:46 patthoyts Exp $}

    namespace export register

    catch {namespace import -force [namespace parent]::Utils::*}
    catch {namespace import -force [uplevel {namespace current}]::rpcvar::*}
}

# -------------------------------------------------------------------------

# Register this package with tclhttpd.
#
# eg: register -prefix /soap ?-namespace ::zsplat? ?-interp slave?
#
# -prefix is the URL prefix for the SOAP methods to be implemented under
# -interp is the Tcl slave interpreter to use ( {} for the current interp)
# -namespace is the Tcl namespace look for the implementations under
#            (default is global)
# -uri    the XML namespace for these methods. Defaults to the Tcl interpreter
#         and namespace name.
#
proc ::SOAP::Domain::register {args} {

    if { [llength $args] < 1 } {
        return -code error "invalid # args:\
              should be \"register ?option value  ...?\""
    }

    # set the default options. These work out to be the current interpreter,
    # toplevel namespace and under /soap URL
    array set opts [list \
            -prefix /soap \
            -namespace {::} \
            -interp {} \
            -uri {^} ]

    # process the arguments
    foreach {opt value} $args {
        switch -glob -- $opt {
            -pre* {set opts(-prefix) $value}
            -nam* {set opts(-namespace) ::$value}
            -int* {set opts(-interp) $value}
            -uri  {set opts(-uri) $value}
            default {
                set names [join [array names opts -*] ", "]
                return -code error "unrecognised option \"$opt\":\
                       must be one of $names"
            }
        }
    }

    # Construct a URI if not supplied (as indicated by the funny character)
    # gives interpname hyphen namespace path (with more hyphens)
    if { $opts(-uri) == {^} } {
        set opts(-uri) 
        regsub -all -- {::+} "$opts(-interp)::$opts(-namespace)" {-} r
        set opts(-uri) [string trim $r -]
    }

    # Generate the fully qualified name of our options array variable.
    set optname [namespace current]::opts$opts(-prefix)

    # check we didn't already have this registered.
    if { [info exists $optname] } {
        return -code error "URL prefix \"$opts(-prefix)\" already registered"
    }

    # set up the URL domain handler procedure.
    # As interp eval {} evaluates in the current interpreter we can define
    # both a slave interpreter _and_ a specific namespace if we need.

    # If required create a slave interpreter.
    if { $opts(-interp) != {} } {
        catch {interp create -- $opts(-interp)}
    }
    
    # Now create a command in the slave interpreter's target namespace that
    # links to our implementation in this interpreter in the SOAP::Domain
    # namespace.
    interp alias $opts(-interp) $opts(-namespace)::URLhandler \
            {} [namespace current]::domain_handler $optname

    # Register the URL handler with tclhttpd now.
    Url_PrefixInstall $opts(-prefix) \
        [list interp eval $opts(-interp) $opts(-namespace)::URLhandler]

    # log the uri/domain registration
    array set [namespace current]::opts$opts(-prefix) [array get opts]

    return $opts(-prefix)
}

# -------------------------------------------------------------------------

# SOAP URL Domain handler
#
# Called from the namespace or interpreter domain_handler to perform the
# work.
# optsname the qualified name of the options array set up during registration.
# sock     socket back to the client
# suffix   the remainder of the url once the prefix was stripped.
#
proc ::SOAP::Domain::domain_handler {optsname sock args} {
    variable debug
    upvar \#0 Httpd$sock data
    upvar \#0 $optsname options
    
    
    # if suffix is {} then it fails to make it through the various evals.
    set suffix [lindex $args 0]
    
    # check this is an XML post
    set failed [catch {set type $data(mime,content-type)} msg]
    if { $failed } {
        set msg "Invalid SOAP request: not XML data"
        log::log debug $msg
        Httpd_ReturnData $sock text/xml [SOAP::fault SOAP-ENV:Client $msg] 500
        return $failed
    }
    
    # make sure we were sent some XML
    set failed [catch {set query $data(query)} msg]
    if { $failed } {
        set msg "Invalid SOAP request: no data sent"
        log::log debug $msg
        Httpd_ReturnData $sock text/xml [SOAP::fault SOAP-ENV:Client $msg] 500
        return $failed
    }

    # Check that we have a properly registered domain
    if { ! [info exists options] } {
        set msg "Internal server error: domain improperly registered"
        log::log debug $msg
        Httpd_ReturnData $sock text/xml [SOAP::fault SOAP-ENV:Server $msg] 500
        return 1
    }        

    # Parse the XML into a DOM tree.
    set doc [dom::DOMImplementation parse $query]
    if { $debug } { set ::doc $doc }

    # Call the procedure and convert errors into SOAP Faults and the return
    # data into a SOAP return packet.
    set failed [catch {SOAP::CGI::soap_call $doc $options(-interp)} msg]
    Httpd_ReturnData $sock text/xml $msg [expr {$failed ? 500 : 200}]

    catch {dom::DOMImplementation destroy $doc}
    return $failed
}

# -------------------------------------------------------------------------

package provide SOAP::Domain $::SOAP::Domain::version

# -------------------------------------------------------------------------

# Local variables:
#   mode: tcl
#   indent-tabs-mode: nil
# End:
