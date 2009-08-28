# smtp.tcl - Copyright (C) 2001 Pat Thoyts <patthoyts@users.sourceforge.net>
#
# Provide an SMTP transport for the SOAP package.
#
# e.g.:
#   SOAP::create purchase \
#          -proxy mailto:soap-purchase@localhost
#          -action urn:tclsoap:Purchase
#          -uri urn:tclsoap:Purchase
#          -params {code string auth string}
#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the accompanying file `LICENSE'
# for more details.
# -------------------------------------------------------------------------

package require SOAP
package require mime;                   # tcllib 1.0
package require smtp;                   # tcllib 1.0

namespace eval ::SOAP::Transport::smtp {
    variable version 1.0
    variable rcsid {$Id: smtp.tcl,v 1.5 2008/02/28 22:05:55 andreas_kupries Exp $}
    variable options
    
    SOAP::register mailto [namespace current]

    # Initialize the transport options.
    if {![info exists options]} {
        array set options [list \
            servers  {} \
            headers  {} \
            sender   "$::tcl_platform(user)@[info hostname]" \
        ]

    }

    # Declare the additional SOAP method options provided by this transport.
    variable method:options [list \
        mimeheaders \
        attachments \
    ]
}

# -------------------------------------------------------------------------

# Description:
#  Implement the additional SOAP method configuration options provide
#  for this transport.
# Notes:
#  -mimeheaders - specify extra MIME headers for use with this SOAP
#       method. eg: -mimeheaders [list X-Face $facedata X-Abuse $ab]
#  -attach      - implements SOAP Messages with attachments spec.
#       The argument should be a MIME token returned by mime::initialize.
#
proc ::SOAP::Transport::smtp::method:configure {procVarName opt value} {
    upvar $procVarName procvar
    switch -glob -- $opt {
        -mimeheaders {
            set procvar(mimeheaders) $value
        }
        -attach* {
            set procvar(attachments) $value
        }
        default {
            # not reached
            return -code error "unknown option \"$opt\""
        }
    }
}

# -------------------------------------------------------------------------

# Description:
#   Permit configuration of the SMTP transport.
#
proc ::SOAP::Transport::smtp::configure {args} {
    variable options

    if {[llength $args] == 0} {
        set r {}
        foreach {opt value} [array get options] {
            lappend r "-$opt" $value
        }
        return $r
    }

    foreach {opt value} $args {
        switch -- $opt {
            -servers   {
                set options(servers) $value
            }
            -sender {
                set options(sender) $value
            }
            -headers {
                set options(headers) $value
            }
            default {
                return -code error "invalid option \"$opt\": must be \
                     \"-servers\", \"-headers\" or \"-sender\""
            }
        }
    }
    return {}
}

# -------------------------------------------------------------------------

# Description:
#   SMTP method calls are asynchronous in that the reply will arrive elsewhere
#   (if there is one). This suggests that if a reply is expected there will be
#   a SMTP server running on the caller. Not normally the case. SMTP is 
#   expected to be used for batch transfers of multiple SOAP requests.
#   eg: database updates of point-of-sale (POS) transactions.
# Parameters:
#   procVarName  - SOAP configuration variable identifier.
#   url          - the endpoint address. eg: mailto:user@address
#   soap         - the XML payload for the SOAP message.
# Notes:
#   As this transport method doesn't return anything, we need to quieten the
#   SOAP framework code. I suggest defining a dummy -command procedure to 
#   get the call made as asynchronous so the framework doesn't expect an 
#   answer. The SMTP transport will never attempt to call this procedure.
#
proc ::SOAP::Transport::smtp::xfer {procVarName url soap} {
    variable options

    if {[catch {eval array set addr [mime::parseaddress $url]} msg]} {
        return -code error $msg
    }
    if {$addr(error) != {}} {
        return -code error $addr(error)
    }

    # Clean up the previous request (if any)
    upvar $procVarName procvar
    if {[info exists procvar(smtp)] && $procvar(smtp) != {}} {
        mime::finalize $procvar(smtp) -subordinate all
    }

    set procvar(smtp) [mime::initialize -canonical text/xml -string $soap]

    if {[info exists procvar(attachments)] && $procvar(attachments) != {}} {
        set token $procvar(smtp)
        set procvar(smtp) [mime::initialize -canonical multipart/related \
                               -parts [concat $token $procvar(attachments)]]
    }

    if {[info exists procvar(mimeheaders)]} {
        foreach {key value} $procvar(mimeheaders) {
            mime::setheader $procvar(smtp) $key $value
        }
    }
    
    foreach {key value} $options(headers) {
        mime::setheader $procvar(smtp) $key $value
    }

    if {$procvar(action) != {}} {
        mime::setheader $procvar(smtp) "X-SOAPAction" $procvar(action)
    }

    # Get the SOAP package version
    # FRINK: nocheck
    set version [set [namespace parent [namespace parent]]::version]

    set r [smtp::sendmessage $procvar(smtp) \
               -servers $options(servers) \
               -originator $options(sender) \
               -recipients $addr(address) \
               -header [list "X-Mailer" \
                            "TclSOAP/$version ($::tcl_platform(os))"]]

    if {$r != {}} {
        return -code error "SOAP transport error: $r"
    }

    return {}
}

# -------------------------------------------------------------------------

# Description:
#  Called to release any retained resources from a SOAP method. For the
#  smtp transport this is just the mime token.
# Parameters:
#  methodVarName - the name of the SOAP method configuration array
#
proc ::SOAP::Transport::smtp::method:destroy {methodVarName} {
    upvar $methodVarName procvar
    if {[info exists procvar(smtp)] && $procvar(smtp) != {}} {
        catch { ::mime::finalize $procvar(smtp) -subordinate all }
    }
}

# -------------------------------------------------------------------------

proc ::SOAP::Transport::smtp::dump {methodName type} {
    SOAP::cget $methodName proxy
    if {[catch {SOAP::cget $methodName smtp} token]} {
        set token {}
    }

    if {$token == {}} {
        return -code error "cannot dump: no information is available\
            for \"$methodName\""
    }

    return [mime::buildmessage $token]
}

# -------------------------------------------------------------------------

package provide SOAP::smtp $::SOAP::Transport::smtp::version

# -------------------------------------------------------------------------
# Local variables:
#    mode: tcl
#    indent-tabs-mode: nil
# End:
