# XMLRPC.tcl - Copyright (C) 2001 Pat Thoyts <patthoyts@users.sourceforge.net>
#
# Provide Tcl access to XML-RPC provided methods.
#
# See http://tclsoap.sourceforge.net/ for usage details.
#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the accompanying file `LICENSE'
# for more details.
# -------------------------------------------------------------------------

package require SOAP 1.4
package require rpcvar

namespace eval ::XMLRPC {
    variable version 1.0
    variable rcs_version { $Id: XMLRPC.tcl,v 1.8 2003/09/06 17:08:46 patthoyts Exp $ }

    namespace export create cget dump configure proxyconfig export
    catch {namespace import -force [uplevel {namespace current}]::rpcvar::*}
}

# -------------------------------------------------------------------------

# Delegate all these methods to the SOAP package. The only difference between
# a SOAP and XML-RPC call are the method call wrapper and unwrapper.

proc ::XMLRPC::create {args} {
    set args [linsert $args 1 \
            -wrapProc [namespace origin \
                [namespace parent]::SOAP::xmlrpc_request] \
            -parseProc [namespace origin \
                [namespace parent]::SOAP::parse_xmlrpc_response]]
    return [uplevel 1 "SOAP::create $args"]
}

proc ::XMLRPC::configure { args } {
    return [uplevel 1 "SOAP::configure $args"]
}

proc ::XMLRPC::cget { args } {
    return [uplevel 1 "SOAP::cget $args"] 
}

proc ::XMLRPC::dump { args } {
    return [uplevel 1 "SOAP::dump $args"] 
}

proc ::XMLRPC::proxyconfig { args } {
    return [uplevel 1 "SOAP::proxyconfig $args"] 
}

proc ::XMLRPC::export {args} {
    foreach item $args {
        uplevel "set \[namespace current\]::__xmlrpc_exports($item)\
                \[namespace code $item\]"
    }
    return
}

# -------------------------------------------------------------------------

# Description:
#   Prepare an XML-RPC fault response
# Parameters:
#   faultcode   the XML-RPC fault code (numeric)
#   faultstring summary of the fault
#   detail      list of {detailName detailInfo}
# Result:
#   Returns the XML text of the SOAP Fault packet.
#
proc ::XMLRPC::fault {faultcode faultstring {detail {}}} {
    set xml [join [list \
	    "<?xml version=\"1.0\" ?>" \
	    "<methodResponse>" \
	    "  <fault>" \
	    "    <value>" \
	    "      <struct>" \
	    "        <member>" \
	    "           <name>faultCode</name>"\
	    "           <value><int>${faultcode}</int></value>" \
	    "        </member>" \
	    "        <member>" \
	    "           <name>faultString</name>"\
	    "           <value><string>${faultstring}</string></value>" \
	    "        </member>" \
	    "      </struct> "\
	    "    </value>" \
	    "  </fault>" \
	    "</methodResponse>"] "\n"]
    return $xml
}

# -------------------------------------------------------------------------

# Description:
#   Generate a reply packet for a simple reply containing one result element
# Parameters:
#   doc         empty DOM document element
#   uri         URI of the SOAP method
#   methodName  the SOAP method name
#   result      the reply data
# Result:
#   Returns the DOM document root of the generated reply packet
#
proc ::XMLRPC::_reply {doc uri methodName result} {
    set d_root [dom::document createElement $doc "methodResponse"]
    set d_params [dom::document createElement $d_root "params"]
    set d_param [dom::document createElement $d_params "param"]
    insert_value $d_param $result
    return $doc
}

# -------------------------------------------------------------------------
# Description:
#   Generate a reply packet for a reply containing multiple result elements
# Parameters:
#   doc         empty DOM document element
#   uri         URI of the SOAP method
#   methodName  the SOAP method name
#   args        the reply data, one element per result.
# Result:
#   Returns the DOM document root of the generated reply packet
#
proc ::XMLRPC::reply {doc uri methodName args} {
    set d_root   [dom::document createElement $doc  "methodResponse"]
    set d_params [dom::document createElement $d_root "params"]

    foreach result $args {
        set d_param  [dom::document createElement $d_params "param"]
        insert_value $d_param $result
    }
    return $doc
}

# -------------------------------------------------------------------------

# node is the <param> element
proc ::XMLRPC::insert_value {node value} {

    set type      [rpctype $value]
    set value     [rpcvalue $value]
    set typeinfo  [typedef -info $type]

    set value_elt [dom::document createElement $node "value"]

    if {[string match {*()} $type] || [string match array $type]} {
        # array type: arrays are indicated by a () suffix of the word 'array'
        set itemtype [string trimright $type ()]
        if {$itemtype == "array"} {
            set itemtype "any"
        }
        set array_elt [dom::document createElement $value_elt "array"]
        set data_elt [dom::document createElement $array_elt "data"]
        foreach elt $value {
            if {[string match $itemtype "any"] || \
                [string match $itemtype "ur-type"] || \
                [string match $itemtype "anyType"]} {
                XMLRPC::insert_value $data_elt $elt
            } else {
                XMLRPC::insert_value $data_elt [rpcvar $itemtype $elt]
            }
        }
    } elseif {[llength $typeinfo] > 1} {
        # a typedef'd struct
        set struct_elt [dom::document createElement $value_elt "struct"]
        array set ti $typeinfo
        foreach {eltname eltvalue} $value {
            set member_elt [dom::document createElement $struct_elt "member"]
            set name_elt [dom::document createElement $member_elt "name"]
            dom::document createTextNode $name_elt $eltname
            if {![info exists ti($eltname)]} {
                error "invalid member name: \"$eltname\" is not a member of\
                        the $type type."
            }
            XMLRPC::insert_value $member_elt [rpcvar $ti($eltname) $eltvalue]
        }

    } elseif {[string match struct $type]} {
        # an undefined struct
        set struct_elt [dom::document createElement $value_elt "struct"]
        foreach {eltname eltvalue} $value {
            set member_elt [dom::document createElement $struct_elt "member"]
            set name_elt [dom::document createElement $member_elt "name"]
            dom::document createTextNode $name_elt $eltname
            XMLRPC::insert_value $member_elt $eltvalue
        }
    } else {
        # simple type.
        set type_elt  [dom::document createElement $value_elt $type]
        dom::document createTextNode $type_elt $value
    }    
}

# -------------------------------------------------------------------------

package provide XMLRPC $XMLRPC::version

# -------------------------------------------------------------------------

# Local variables:
#    indent-tabs-mode: nil
# End:
