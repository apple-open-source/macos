# beep.tcl - Copyright (C) 2001 Pat Thoyts <patthoyts@users.sourceforge.net>
#
# Provide an BEEP transport for the SOAP package, e.g.,
#
#     SOAP::configure -transport soap.beep \
#         -debug true                      \
#         -logfile ...                     \
#         -logident ...                    \
#         -option-for-mixer::init ...
#
#     SOAP::create echoInteger
#         -uri    http://soapinterop.org/                         \
#         -proxy  soap.beep://qawoor.dbc.mtview.ca.us/soapinterop \
#         -params { inputInteger int }
#
# BEEP support using the beepcore-tcl code from 
# http://sourceforge.net/projects/beepcore-tcl provided by M Rose.
#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the accompanying file `LICENSE'
# for more details.
# -------------------------------------------------------------------------

package require beepcore::log;          # beepcore-tcl
package require beepcore::mixer;        # beepcore-tcl
package require beepcore::peer;         # beepcore-tcl
package require mime;                   # tcllib

namespace eval ::SOAP::Transport::beep {
    variable version 1.0
    variable rcsid {$Id: beep.tcl,v 1.8 2003/09/06 17:08:46 patthoyts Exp $}
    variable options
    variable sessions

    ::SOAP::register soap.beep  [namespace current]
    ::SOAP::register soap.beeps [namespace current]

    # Initialize the transport options.
    if {![info exists options]} {
        array set options {
            -logfile    /dev/null
            -logident   soap
        }
    }

    # beep sessions
    array set sessions {}

    # Declare the additional SOAP method options provided by this transport.
    variable method:options [list \
        debug \
        logfile \
        logident \
        logT \
        mixerT \
        channelT \
        features \
        destroy \
        wait \
    ]
}

# -------------------------------------------------------------------------

# Description:
#  Implement the additional SOAP method configuration options provide
#  for this transport.
#  
proc ::SOAP::Transport::beep::method:configure {procVarName opt value} {
    variable options

    upvar $procVarName procvar
    switch -glob -- $opt {
        -debug - 
        -logfile - 
        -logident {
            set options($opt) $value
	}

        -logT - 
        -mixerT - 
        -channelT -
        -features - 
        -destroy - 
        -wait {
            set procvar([string trimleft $opt -]) $value
        }
        default {
            return -code error "unknown option \"$opt\""
        }
    }
}

# -------------------------------------------------------------------------

# Description:
#  Transport defined SOAP method creation hook. We initialize the 
#  method:options that were declared above and do any transport specific
#  initialization for the method.
# Parameters:
#  procVarName - the name of the method configuration array
#  args        - the argument list that was given to SOAP::create
#
proc ::SOAP::Transport::beep::method:create {procVarName args} {
    global debugP
    variable sessions
    upvar $procVarName procvar

    if { ![info exists debugP] } {
	set debugP 0
    }
    
    # procvar(proxy) will not have been set yet so:
    set ndx [lsearch -exact $args -proxy]
    incr ndx 1
    if {$ndx == 0} {
        return -code error "invalid arguments:\
            the \"-proxy URL\" argument is required"
    } else {
        set procvar(proxy) [lindex $args $ndx]
    }
    array set URL [uri::split $procvar(proxy)]
    
    # create a logging object, if necessary
    if { [set logT $procvar(logT)] == {} } {
	set logT [set procvar(logT) \
		      [::beepcore::log::init \
                           [set [namespace current]::options(-logfile)] \
                           [set [namespace current]::options(-logident)]]]
    }

    #
    # when the RFC issues, update the default port number...
    # -- this has now occurred: RFC 3288
    #
    if { $URL(port) == {} } {
	set URL(port) 605
    }
    if { $URL(path) == {} } {
	set URL(path) /
    }

    switch -- $URL(scheme) {
	soap.beep {
	    set privacy none
	}

	soap.beeps {
	    set privacy strong
	}
    } 
    array set options [array get [namespace current]::options]
    unset options(-logfile) \
        options(-logident)
    array set options [list -port	 $URL(port) \
                            -privacy	 $privacy   \
                            -servername  $URL(host)]
    
    set procName [lindex [split $procVarName {_}] end]
    set procFQName [string map {_ ::} $procVarName]

    # see if we have a session already cached
    set signature ""
    foreach option [lsort [array names options]] {
	append signature $option $options($option)
    }
    foreach mixerT [array name sessions] {
	catch { unset props }
	array set props $sessions($mixerT)

	if { ($props(host) != $URL(host)) \
		|| ($props(resource) != $URL(path)) \
		|| ($props(signature) != $signature) } {
	    continue
	}

	if { $procvar(mixerT) == $mixerT } {
	    ::beepcore::log::entry $logT debug [lindex [info level 0] 0] "$procName noop"

	    return
	}

	incr props(refcnt)
	set sessions($mixerT) [array get props]
	::beepcore::log::entry $logT debug [lindex [info level 0] 0] \
	     "$procName using session $mixerT, refcnt now $props(refcnt)"

	set procvar(mixerT) $mixerT
	set procvar(channelT) $props(channelT)
	set procvar(features) $props(features)

	return
    }

    # start a new session
    switch -- [catch { eval [list ::beepcore::mixer::init $logT $URL(host)] \
			    [array get options] } mixerT] {
	0 {
	    set props(host) $URL(host)
	    set props(resource) $URL(path)
	    set props(signature) ""
	    foreach option [lsort [array names options]] {
		append props(signature) $option $options($option)
	    }
	    set props(features) {}
	    set props(refcnt) 1
	    set sessions($mixerT) [array get props]

	    set procvar(mixerT) $mixerT
	    ::beepcore::log::entry $logT debug [lindex [info level 0] 0] \
		 "$procName adding $mixerT to session cache, host $URL(host)"
	}

	7 {
	    array set parse $mixerT
	    ::beepcore::log::entry $logT user \
			 "beepcore::mixer::init $parse(code): $parse(diagnostic)"

	    return -code error $parse(diagnostic)
	}

	default {
	    ::beepcore::log::entry $logT error beepcore::mixer::init $mixerT

	    return -code error $mixerT
	}
    }

    # create the channel
    set profile http://iana.org/beep/soap

    set doc [dom::DOMImplementation create]
    set bootmsg [dom::document createElement $doc bootmsg]
    dom::element setAttribute $bootmsg resource /$URL(path)
    set data [dom::DOMImplementation serialize $doc]
    if { [set x [string first [set y "<!DOCTYPE bootmsg>\n"] $data]] >= 0 } {
	set data [string range $data [expr $x+[string length $y]] end]
    }
    dom::DOMImplementation destroy $doc

    switch -- [set code [catch { ::beepcore::mixer::create $mixerT $profile $data } \
			       channelT]] {
	0 {
	    set props(channelT) $channelT
	    set sessions($mixerT) [array get props]

	    set procvar(channelT) $channelT
	}

	7 {
	    array set parse $channelT
	    ::beepcore::log::entry $logT user \
			 "beepcore::mixer::create $parse(code): $parse(diagnostic)"

            # We can't call SOAP::destroy because we havn't created a SOAP
            # method yet. The local destroy proc will clean up for us.
	    method:destroy $procVarName
	    return -code error $parse(diagnostic)
	}

	default {
	    ::beepcore::log::entry $logT error beepcore::mixer::create $channelT

	    method:destroy $procVarName
	    return -code error $channelT
	}
    }

    # parse the response
    if { [catch { ::beepcore::peer::getprop $channelT datum } data] } {
	::beepcore::log::entry $logT error beepcore::peer::getprop $data

	method:destroy $procVarName
	return -code error $data
    }
    if { [catch { dom::DOMImplementation parse $data } doc] } {
	::beepcore::log::entry $logT error dom::parse $doc

	method:destroy $procVarName
	return -code error "bootrpy is invalid xml: $doc"
    }
    if { [set node [SOAP::selectNode $doc /bootrpy]] != {} } {
	catch {
	    set props(features) \
		[set [subst $procVarName](features) \
			    [set [dom::node cget $node -attributes](features)]]
	    set sessions($mixerT) [array get props]
	}

	dom::DOMImplementation destroy $doc
    } elseif { [set node [SOAP::selectNode $doc /error]] != {} } {
	if { [catch { set code [set [dom::node cget $node -attributes](code)]
		      set diagnostic [SOAP::getElementValue $node] }] } {
	    set code 500
	    set diagnostic "unable to parse boot reply"
	}

	::beepcore::log::entry $logT user "$code: $diagnostic"

	dom::DOMImplementation destroy $doc

        method:destroy $procVarName
	return -code error "$code: $diagnostic"
    } else {
	dom::DOMImplementation destroy $doc

	method:destroy $procVarName
	return -code error "invalid protocol: the boot reply is invalid"
    }
}

# -------------------------------------------------------------------------

# Description:
#  Configure any beep transport specific settings.
#  Anything that works for mixer::init works for us...
#
proc ::SOAP::Transport::beep::configure {args} {
    variable options

    if {[llength $args] == 0} {
	return [array get options]
    }
    array set options $args
    return {}
}

# -------------------------------------------------------------------------

# Description:
#  Called to release any retained resources from a SOAP method.
# Parameters:
#  methodVarName - the name of the SOAP method configuration array
#
proc ::SOAP::Transport::beep::method:destroy {methodVarName} {
    variable sessions
    upvar $methodVarName procvar

    set procName [lindex [split $methodVarName {_}] end]

    set mixerT $procvar(mixerT)
    set logT   $procvar(logT)

    if {[catch {::beepcore::mixer::wait $mixerT -timeout 0} result]} {
        ::beepcore::log::entry $logT error beepcore::mixer::wait $result
    }

    array set props $sessions($mixerT)
    if {[incr props(refcnt) -1] > 0} {
	set sessions($mixerT) [array get props]
	::beepcore::log::entry $logT debug [lindex [info level 0] 0]\
	     "$procName no longer using session $mixerT, refcnt now $props(refcnt)"
	return
    }

    unset sessions($mixerT)
    ::beepcore::log::entry $logT debug [lindex [info level 0] 0] \
	"$procName removing $mixerT from session cache"

    if { [catch { ::beepcore::mixer::fin $mixerT } result] } {
	::beepcore::log::entry $logT error beepcore::mixer::fin $result
    }
    set procvar(mixerT) {}
}

# -------------------------------------------------------------------------

# Description:
#   Do the SOAP RPC call using the BEEP transport.
# Parameters:
#   procVarName  - SOAP configuration variable identifier.
#   url          - the endpoint address. eg: mailto:user@address
#   soap         - the XML payload for the SOAP message.
# Notes:
#
proc ::SOAP::Transport::beep::xfer {procVarName url request} {
    upvar $procVarName procvar

    if {$procvar(command) != {}} {
	set rpyV "[namespace current]::async $procVarName"
    } else {
	set rpyV {}
    }

    set mixerT   $procvar(mixerT)
    set channelT $procvar(channelT)
    set logT     $procvar(logT)

    if {[set x [string first [set y "?>\n"] $request]] >= 0 } {
	set request [string range $request [expr $x+[string length $y]] end]
    }
    set reqT [::mime::initialize -canonical application/xml -string $request]

    switch -- [set code [catch { ::beepcore::peer::message $channelT $reqT \
				       -replyCallback $rpyV } rspT]] {
	0 {
	    ::mime::finalize $reqT

	    if { $rpyV != {} } {
		return
	    }

	    set content [::mime::getproperty $rspT content]
	    set response [::mime::getbody $rspT]

	    ::mime::finalize $rspT

	    if {[string compare $content application/xml]} {
		return -code error "not application/xml reply, not $content"
	    }

	    return $response
	}

	7 {
	    array set parse [::beepcore::mixer::errscan $mixerT $rspT]
	    ::beepcore::log::entry $logT user "$parse(code): $parse(diagnostic)"

	    ::mime::finalize $reqT
	    ::mime::finalize $rspT
	    return -code error "$parse(code): $parse(diagnostic)"
	}

	default {
	    ::beepcore::log::entry $logT error beepcore::peer::message $rspT

	    ::mime::finalize $reqT
	    return -code error $rspT
	}
    }
}

proc ::SOAP::Transport::beep::async {procVarName channelT args} {
    upvar $procVarName procvar

    if { [catch { eval [list async2 $procVarName] $args } result] } {
	if { $procvar(errorCommand) != {} } {
	    set errorCommand $procvar(errorCommand)
	    if { ![catch { eval $errorCommand [list $result] } result] } {
		return
	    }
	}

	bgerror $result
    }
}

proc ::SOAP::Transport::beep::async2 {procVarName args} {
    upvar $procVarName procvar
    array set argv $args

    switch -- $argv(status) {
	positive {
	    set content [::mime::getproperty $argv(mimeT) content]
	    set reply [::mime::getbody $argv(mimeT)]
	    ::mime::finalize $argv(mimeT)

	    if {[string compare $content application/xml]} {
		return -code error "not application/xml reply, not $content"
	    }

	    set reply [SOAP::invoke2 $procVarName $reply]
	    return [eval $procvar(command) [list $reply]]	
	}

	negative {
	    set mixerT $procvar(mixerT)
	    set logT $procvar(logT)

	    array set parse [::beepcore::mixer::errscan $mixerT $argv(mimeT)]
	    ::beepcore::log::entry $logT user "$parse(code): $parse(diagnostic)"

	    ::mime::finalize $argv(mimeT)
	    return -code error "$parse(code): $parse(diagnostic)"
	}

	default {
	    ::mime::finalize $argv(mimeT)

	    return -code error "not expecting $argv(status) reply"
	}
    }
}

# -------------------------------------------------------------------------

proc ::SOAP::Transport::beep::wait {procVarName} {
    upvar $procVarName procvar
    ::beepcore::mixer::wait $procvar(mixerT)
}

# -------------------------------------------------------------------------
# Extend the uri package to support our beep URL's. I don't think these are
# official scheme names. If they are then we can add them into the tcllib
# code - in the meantime...

catch {
    ::uri::register {soap.beep soap.beeps beep} {
        variable schemepart "//.*"
        variable url "(soap.)?beeps?:${schemepart}"
    }
}

proc ::uri::SplitSoap.beep {url} {
    return [SplitHttp $url]
}

proc ::uri::SplitSoap.beeps {url} {
    return [SplitHttp $url]
}
proc ::uri::SplitBeep {url} {
    return [SplitHttp $url]
}

# -------------------------------------------------------------------------

package provide SOAP::beep $SOAP::Transport::beep::version

# -------------------------------------------------------------------------
# Local Variables:
#   indent-tabs-mode: nil
# End:
