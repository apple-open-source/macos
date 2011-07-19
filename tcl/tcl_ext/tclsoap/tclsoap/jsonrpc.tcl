# JSONRPC.tcl 
# Copyright (C) 2005 Ashok P. Nadkarnis <apnadkarni@users.sourceforge.net>
#
# Provide Tcl access to JSON-RPC methods.
# Based on XMLRPC.tcl in the TclSOAP package, this package uses
# much of the common communication transport code from the TclSOAP
# package.

#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the accompanying file `LICENSE'
# for more details.
# -------------------------------------------------------------------------

package require SOAP::Utils
package require SOAP 1.4
package require rpcvar
package require json

namespace eval ::JSONRPC {
    variable version 0.1.0;     # Software version
    variable rcs_version { $Id: jsonrpc.tcl,v 1.4 2008/10/09 11:02:30 apnadkarni Exp $ }

    variable jsonrpc_state;           # Array to hold global stuff
    set jsonrpc_state(request_id) 0;         # Used to identify requests

    namespace export create cget dump configure proxyconfig export
    # catch {namespace import -force [uplevel {namespace current}]::rpcvar::*}
    # catch {namespace import -force ::SOAP::Utils::*}

    # Create the typedefs for a jsonrequest and jsonresponse.
    # Note: Some servers (for example the test server at 
    # http://jsolait.net/wiki/examples/jsonrpc/tester) seem to insist
    # that the id field be a formatted as a string and not an integer
    ::rpcvar::typedef {
        jsonrpc string
        id      string
        params  array
        method  string
    } jsonrequest

    ::rpcvar::typedef {
        jsonrpc string
        id      string
        params  object
        method  string
    } jsonrequest_namedparams

    ::rpcvar::typedef {
        code    int
        message string
        data    any
    } jsonerror

    ::rpcvar::typedef {
        jsonrpc string
        id      string
        result  any
        error   jsonerror
    } jsonresponse

}

# -------------------------------------------------------------------------

# Delegate all these methods to the SOAP package. This does not mean
# all the options for SOAP are supported or meaningful for JSON-RPC but
# the irrelevant ones will be ignored. We do need to override the
# SOAP method call wrapper and unwrapper functions.
proc ::JSONRPC::create {args} {
    set args [linsert $args 1 \
                  -rpcprotocol JSONRPC \
                  -contenttype "application/json-rpc" \
                  -wrapProc [namespace origin \
                                 [namespace parent]::JSONRPC::request] \
                  -parseProc [namespace origin \
                                  [namespace parent]::JSONRPC::parse_response]]
    return [uplevel 1 "SOAP::create $args"]
}

proc ::JSONRPC::configure { args } {
    return [uplevel 1 "SOAP::configure $args"]
}

proc ::JSONRPC::cget { args } {
    return [uplevel 1 "SOAP::cget $args"]
}

proc ::JSONRPC::dump { args } {
    return [uplevel 1 "SOAP::dump $args"]
}

proc ::JSONRPC::proxyconfig { args } {
    return [uplevel 1 "SOAP::proxyconfig $args"]
}

proc ::JSONRPC::export {args} {
    foreach item $args {
        uplevel "set \[namespace current\]::__jsonrpc_exports($item)\
                \[namespace code $item\]"
    }
    return
}

# -------------------------------------------------------------------------

# Description:
#   Prepare an JSON-RPC fault (error) response
# Parameters:
#   faultcode   the JSON-RPC fault code (numeric)
#   faultstring summary of the fault
#   detail      list of {detailName detailInfo}
# Result:
#   Returns the JSON text of the error packet.
#
proc ::JSONRPC::fault {jsonver reqid faultcode faultstring {detail {}}} {
    if {[llength $detail] == 0} {
        set err [list code $faultcode message $faultstring]
    } else {
        set err [list \
                     code $faultcode \
                     message $faultstring \
                     data  [rpcvar::rpcvar struct $detail]]
    }
    return [jsonresponse $jsonver $reqid $err error]
}

proc ::JSONRPC::encode_value {value} {
    set type      [rpcvar::rpctype $value]
    set value     [rpcvar::rpcvalue $value]
    set typeinfo  [rpcvar::typedef -info $type]

    if {[string match {*()} $type] || [string match array $type]} {
        # Arrays have a type suffix of "()"
        set itemtype [string trimright $type ()]
        if {$itemtype == "array"} {
            set itemtype "any"
        }
        set acc {}
        foreach elt $value {
            if {[string match $itemtype "any"]} {
                lappend acc [JSONRPC::encode_value $elt]
            } else {
                lappend acc [JSONRPC::encode_value [rpcvar::rpcvar $itemtype $elt]]
            }
        }
        return "\[[join $acc ,]\]"
    } elseif {[llength $typeinfo] > 1} {
        # a typedef'd struct (object in json)
        array set ti $typeinfo
        set acc {}
        foreach {eltname eltvalue} $value {

            if {![info exists ti($eltname)]} {
                error "Invalid member name: \"$eltname\" is not a member of the $type object typedef." \
                    "" [list JSONRPC local "Invalid object member name"]
            }
            if {$ti($eltname) eq "any"} {
                lappend acc "\"$eltname\":[JSONRPC::encode_value $eltvalue]"
            } else {
                lappend acc "\"$eltname\":[JSONRPC::encode_value [rpcvar::rpcvar $ti($eltname) $eltvalue]]"
            }
        }
        return "{[join $acc ,]}"
    } elseif {[string equal struct $type] || [string equal object $type]} {
        # an undefined struct (json object)
        set acc {}
        foreach {eltname eltvalue} $value {
            lappend acc "\"$eltname\":[JSONRPC::encode_value $eltvalue]"
        }
        return "{[join $acc ,]}"
    } elseif {[string equal $type "string"]} {
        return "\"[JSONRPC::escape $value]\""
    } elseif {$type eq "number" || $type eq "int" || $type eq "float" || $type eq "double"} {
        # Convert hex and octal to standard decimal. Also 
        # canonicalizes floating point
        return [expr $value]
    } elseif {[string match bool* $type]} {
        return [expr {$value ? true : false}]
    } else {
        # All other simple types
        return $value
    }
}

#
# Escapes a JSON string
proc ::JSONRPC::escape {s} {
    # Initialize the map for escaping special characters

    # First do control characters as \u00xx sequences
    for {set i 0} {$i < 32} {incr i} {
        set map([format %c $i]) \\u[format %04x $i]
    }

    # Replace certain well known control characters with \ sequences
    set map([format %c 8]) \\b; # backspace
    set map([format %c 9]) \\t; # tab
    set map([format %c 10]) \\n; # lf
    set map([format %c 12]) \\f; # ff
    set map([format %c 13]) \\r; # cr

    # Other special sequences
    set map(\") {\"}
    set map(\\) {\\}
    set map(/)  {\/}
        
    set [namespace current]::json_escape_map [array get map]

    # Redefine ourselves so we do not initialize every time
    proc ::JSONRPC::escape s {
        variable json_escape_map
        return [string map $json_escape_map $s]
    }

    # Call the redefined proc from the caller's level
    return [uplevel 1 [info level 0]]
}


#
# Description:
#   Procedure to generate the JSON data for a configured JSONRPC procedure.
# Parameters:
#   procVarName - the name of the JSONRPC method variable
#   args        - the arguments for this RPC method
# Result:
#   Payload data containing the JSONRPC method call.
#
proc ::JSONRPC::request {procVarName args} {
    variable jsonrpc_state

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
        return -code error -errorcode [list JSONRPC local "Wrong number of arguments."] $msg
    }
    
    # Construct the typed parameter list. The parameter
    # list is constructed as an array (by position) unless specified
    # otherwise by that caller as an object (by name)
    set plist [list ]
    if { [llength $params] != 0 } {
        if {$procvar(namedparams)} {
            foreach {pname ptype} $params val $args {
                lappend plist $pname [rpcvar::rpcvar $ptype $val]
            }
        } else {
            foreach {pname ptype} $params val $args {
                lappend plist [rpcvar::rpcvar $ptype $val]
            }
        }
    }
    
    # Sent as a JSON object
    # { "jsonrpc" : VERSIONSTRING, "method" : METHODNAMESTRING, "params" : [ PARAMARRAYLIST ], "id" : ID }
    # The params element may be left out if no params according to the spec but
    # some servers object to this so always fill it in.
    # The id element is filled in, but not currently used as the 
    # TclSOAP interface has no way to
    # have the caller associated requests with responses

    # Version 1 JSON has no version field
    set reqflds [list id [incr jsonrpc_state(request_id)] method $name]
    if {$procvar(version) ne "" &&
        ! [string match "1.*" $procvar(version)]} {
        lappend reqflds jsonrpc $procvar(version)
    }

    lappend reqflds params $plist

    if {$procvar(namedparams)} {
        return [encode_value [rpcvar::rpcvar jsonrequest_namedparams $reqflds]]
    } else {
        return [encode_value [rpcvar::rpcvar jsonrequest $reqflds]]
    }
}


# Description:
#   Parse an JSON-RPC response payload. Check for fault response otherwise 
#   extract the value data.
# Parameters:
#   procVarName  - the name of the JSON-RPC method configuration variable
#   payload          - the payload of the response
# Result:
#   The extracted value(s). Array types are converted into lists and struct/object
#   types are turned into lists of name/value pairs suitable for array set
# Notes:
#   The XML-RPC fault response doesn't allow us to add in extra values
#   to the fault struct. So where to put the servers errorInfo?
#
proc ::JSONRPC::parse_response { procVarName payload } {
    upvar $procVarName procvar

    set result {}
    if {$payload == {} && ![string match "http*" $procvar(proxy)]} {
        # This is probably not an error. SMTP and FTP won't return anything
        # HTTP should always return though (I think).
        return {}
    } else {
        if {[catch {set response [::json::json2dict $payload]}]} {
            return -code error \
                -errorcode [list JSONRPC local \
                                "Server response is not well-formed JSON. The response was '$payload'"]
        }
    }

    # The response will have the following fields:
    #  jsonrpc - protocol version, may be missing for V1.0 servers
    #            (ignored)
    #  result - the result value - only if no errors
    #  error - error value on faults
    #  id - the id from the original request (currently ignored

    # Both error and result fields should not be simultaneously present. 
    # But older servers may return various combinations.
    # TBD - check handling of JSON null values
    if {[dict exists $response error] &&
        [dict get $response error] ne "null"} {
        set err [dict get $response error]
        if {[dict exists $err message]} {
            set ermsg "Error response from server: [dict get $err message]"
        } else {
            set ermsg "Server returned an error."
        }
        return -code error -errorcode [list JSONRPC remote [dict get $response error]] $ermsg
    }

    if {![dict exists $response result]} {
        return -code error -errorcode [list JSONRPC local \
            "Server response is not well-formed JSON-RPC response. The response was '$payload'"]
    }
    
    return [dict get $response result]
}


# -------------------------------------------------------------------------
# Description:
#   Parse an JSON-RPC call payload. Extracts method name and parameters.
# Parameters:
#   procVarName  - the name of the JSON-RPC method configuration variable
#   payload      - the payload of the response
# Result:
#   A list containing the context of the request as the first element,
#   the name of the called method as second element,
#   and the extracted parameter(s) as third element. Array types are
#   converted into lists and struct types are turned into lists of
#   name/value pairs suitable for array set
# Notes:
#
proc ::JSONRPC::parse_request { payload } {
    set result {}
    if {[catch {set request [::json::json2dict $payload]}]} {
        return -code error -errorcode [list JSONRPC local "JSON request received with invalid format"] \
            "Client request is not well-formed JSON.\n\
            Call was '$payload'"
    }

    if {! ([dict exists $request method] && [dict exists $request id])} {
        return -code error -errorinfo Server \
            "Client request is not well-formed JSON-RPC request.\n\
            Call was '$payload'"
    }

    set jsonrpcver "1.0"
    if {[dict exists $request jsonrpc]} {
        set jsonrpcver [dict get $request jsonrpc]
    }
    set id     [dict get $request id]
    set method [dict get $request method]
    set params {}
    if {[dict exists $request params]} {
        set params [dict get $request params]
    }

    return [list [list $jsonrpcver $id] $method $params]
}

# ----------------------------------------------------------------
# Description:
#   Build a JSON-RPC response
# Parameters:
#   jsonver - what version to use for formatting
#   reqid   - the request id to which this is the response
#   value   - result to be returned
#   type    - "error" if error. Anything else is a result
# Result:
#   A JSON formatted string
proc ::JSONRPC::jsonresponse {jsonver reqid value {type result}} {
    if {$type eq "error"} {
        set respflds [list error $value id $reqid]
    } else {
        set respflds [list result $value id $reqid]
    }

    if {![string match "1.*" $jsonver]} {
        lappend respflds jsonrpc $jsonver
    }

    return [::JSONRPC::encode_value [rpcvar::rpcvar jsonresponse $respflds]]
}

# Description:
#   Dummy JSONRPC transports to examine the JSONRPC requests generated for use
#   with the test package and for debugging.
# Parameters:
#   procVarName  - JSONRPC method name configuration variable
#   url          - URL of the remote server method implementation
#   soap         - the XML payload for this JSONRPC method call
#
namespace eval JSONRPC::Transport::print {
    variable method:options {}
    proc configure {args} {
        return
    }
    proc xfer { procVarName url payload } {
        puts url:$url
        puts "$payload"
    }
    SOAP::register urn:print [namespace current]
}

namespace eval JSONRPC::Transport::reflect {
    variable method:options {
        contenttype
    }
    proc method:configure args {
        return
    }
    proc configure {args} {
        return
    }
    proc xfer {procVarName url payload} {
        if {[catch {
            foreach {jsoncontext method params} [::JSONRPC::parse_request $payload] break
            foreach {jsonrpcver id} $jsoncontext break
            if {$jsonrpcver eq ""} {
                set jsonrpcver "1.0"
            }
        } msg]} {
            # Could not even parse request - do not return error response
            error msg $::errorInfo $::errorCode
        }

        # Request was parsed, not eval it if supported method
        if {$method eq "calc"} {
            if {[catch {
                set result [eval expr $params]
            } msg]} {
                return [::JSONRPC::fault $jsonrpcver $id 1 $msg [list errorCode $::errorCode errorMessage $msg]]
            }
        } else {
            return [::JSONRPC::fault $jsonrpcver $id -32601 "Method not found."]
        }

        return [::JSONRPC::jsonresponse $jsonrpcver $id [rpcvar::rpcvar string $result]]
    }
    SOAP::register urn:jsonreflect [namespace current]
}

# -------------------------------------------------------------------------

package provide JSONRPC $JSONRPC::version

# -------------------------------------------------------------------------

# Local variables:
#    indent-tabs-mode: nil
# End:
