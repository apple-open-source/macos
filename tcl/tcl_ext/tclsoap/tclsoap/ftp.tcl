# ftp.tcl - Copyright (C) 2001 Pat Thoyts <patthoyts@users.sourceforge.net>
#
# Provide an FTP based transport for the SOAP package.
#
# This is somewhat less complete that the HTTP and SMTP transports.
#
# e.g.:
#   SOAP::create purchase \
#          -proxy ftp://me:passwd@localhost/soapstore/transactions
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
package require ftp;                    # tcllib

namespace eval ::SOAP::Transport::ftp {
    variable version 1.0
    variable rcsid {$Id: ftp.tcl,v 1.4 2008/02/28 22:05:55 andreas_kupries Exp $}
    variable options
    
    ::SOAP::register ftp [namespace current]

    # Initialize the transport options.
    if {![info exists options]} {
        array set options {}
    }

    # Declare the additional SOAP method options provided by this transport.
    variable method:options [list \
        username \
        password \
    ]

    #proc ::ftp::DisplayMsg {handle msg state} {
    #    # log
    #}       
}

# -------------------------------------------------------------------------

# Description:
#  Implement the additional SOAP method configuration options provide
#  for this transport.
# Notes:
#  username - username to login on (can also be part of the URL)
#  password - password for server (can also be part of the URL)
#
proc ::SOAP::Transport::ftp::method:configure {procVarName opt value} {
    upvar $procVarName procvar
    switch -glob -- $opt {
        -user* {
            set procvar(username) $value
        }
        -pass* {
            set procvar(password) $value
        }
        default {
            # not reached.
            return -code error "unknown option \"$opt\""
        }
    }
}
# -------------------------------------------------------------------------

# Description:
#   Permit configuration of the FTP transport.
#
proc ::SOAP::Transport::ftp::configure {args} {
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
            default {
                return -code error "invalid option \"$opt\":\
                    no transport configuration options"
            }
        }
    }
    return {}
}

# -------------------------------------------------------------------------

# Description:
#   Perform a remote procedure call using FTP as the transport protocol.
#   This uses the tcllib ftp package to do the work. FTP transports will
#   be asynchronous in that no answer is available.
#
#   We should deal with FTP proxies some time soon. Can the FTP package
#   handle this?
#
# Parameters:
#   procVarName - the name of the SOAP config array for this method.
#   url         - the SOAP endpoint URL
#   request     - the XML data making up the SOAP request
# Result:
#   The data payload is uploaded to the server using FTP. No
#   response is available.
#
proc ::SOAP::Transport::ftp::xfer {procVarName url soap} {
    variable options
    upvar $procVarName procvar

    set username {} ; set password {}

    if {[info exists procvar(username)]} {
        set username $procvar(username)
    }
    if {[info exists procvar(password)]} {
        set password $procvar(password)
    }

    array set URL [uri::split $url]

    if {$URL(user) != {}} { set username $URL(user) }
    if {$URL(pwd)  != {}} { set password $URL(pwd) }

    set tok [ftp::Open $URL(host) $username $password]
    set r [ftp::Append $tok -data $soap $URL(path)]
    ftp::Close $tok

    if {! $r} {
        return -code error "SOAP transport error: $r"
    }

    return {}
}

# -------------------------------------------------------------------------

# Description:
#  Called to release any retained resources from a SOAP method.
# Parameters:
#  methodVarName - the name of the SOAP method configuration array
#
#proc ::SOAP::Transport::ftp::method:destroy {methodVarName} {
#    upvar $methodVarName procvar
#}

# -------------------------------------------------------------------------

#proc ::SOAP::Transport::ftp::dump {methodName type} {
#}

# -------------------------------------------------------------------------

package provide SOAP::ftp $::SOAP::Transport::ftp::version

# -------------------------------------------------------------------------
# Local variables:
#    mode: tcl
#    indent-tabs-mode: nil
# End:
