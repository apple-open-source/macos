# rpcvar.tcl - Copyright (C) 2001 Pat Thoyts <patthoyts@users.sourceforge.net>
#
# Provide a mechanism for passing hints as to the XML-RPC or SOAP value type
# from the user code to the TclSOAP framework.
#
# This package is intended to be imported into the SOAP and XMLRPC namespaces
# where the rpctype command can be overridden to restrict the types to the
# correct names. The client user should then be using SOAP::rpcvalue or
# XMLRPC::rpctype to assign type information.
#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the accompanying file `LICENSE'
# for more details.
# -------------------------------------------------------------------------

namespace eval ::rpcvar {
    variable version 1.2
    variable magic "rpcvar$version"
    variable rcs_id {$Id: rpcvar.tcl,v 1.13 2003/09/06 17:08:46 patthoyts Exp $}
    variable typedefs
    variable typens
    variable enums

    # Initialise the core types
    proc _init {xmlns typename} {
        variable typedefs ; variable typens
        set typedefs($typename) {}      ;# basic types have no typelist
        set typens($typename) $xmlns    ;# set the namespace for this type
    }

    namespace export rpcvar is_rpcvar rpctype rpcsubtype rpcvalue \
            rpcnamespace rpcattributes rpcvalidate rpcheaders typedef \
            schema_set
}

# -------------------------------------------------------------------------

# Description:
#   Create a typed variable with optionally an XML namespace for SOAP types.
# Syntax:
#   rpcvar ?-namespace soap-uri? ?-attributes list? type value
#   rpcvar -paramlist name rpcvalue ?name rpcvalue ...?
# Parameters:
#   namespace  - the SOAP XML namespace for this type
#   attributes - a list of attribute name/value pairs for this element 
#   type       - the XML-RPC or SOAP type of this value
#   value      - the value being typed or, for struct type, either a list
#                of name-value pairs, or the name of the Tcl array.
# Result:
#   Returns a reference to the newly created typed variable
#
proc ::rpcvar::rpcvar {args} {
    variable magic

    set xmlns {}
    set head {}
    set paramlist false
    array set attr {}
    while {[string match -* [lindex $args 0]]} {
        switch -glob -- [lindex $args 0] {
            -n* { set xmlns [Pop args 1] }
            -a* { array set attr [Pop args 1] }
            -h* { set head [concat $head [Pop args 1]] }
            -p* { set paramlist true }
            --  { Pop args ;  break }
            default { return -code error "unknown option \"[lindex $args 0]\""}
        }
        Pop args
    }

    if {$paramlist} {
        set type PARAMLIST
        set value $args
    } else {

        if {[llength $args] != 2} {
            return -code error "wrong # args: \
                should be \"rpcvar ?-namespace uri? type value\""
        }

        set type [lindex $args 0]
        set value [lindex $args 1]

        # For struct types (or typedefs that are structs) accept an array name or a list.
        if {$type != "string" && [uplevel array exists [list $value]]} {
            set value [uplevel array get [list $value]]
        }
        
        if {! [rpcvalidate $type $value]} {
            error "type mismatch: \"$value\" is not appropriate to the \"$type\"\
                type."
        }
    }
    return [list $magic $xmlns [array get attr] $head $type $value]
}

# -------------------------------------------------------------------------
# Description:
#  Pop the nth element off a list. Used in options processing.
#
proc ::rpcvar::Pop {varname {nth 0}} {
    upvar $varname args
    set r [lindex $args $nth]
    set args [lreplace $args $nth $nth]
    return $r
}

# -------------------------------------------------------------------------

# Description:
#   Examine a variable to see if is a reference to a typed variable
# Parameters:
#   varref - reference to the object to be tested
# Result:
#   Returns 1 if the object is a typed value or 0 if not
#
proc ::rpcvar::is_rpcvar { varref } {
    variable magic
    set failed [catch {lindex $varref 0} ref_magic]
    if { ! $failed && $ref_magic == $magic } {
        return 1
    }
    return 0
}

# -------------------------------------------------------------------------

# Description:
#   Guess the SOAP or XML-RPC type of the input.
#   For some simple types we can guess the value type. For others we have
#   to use a typed variable. 
# Parameters:
#   arg  - the value for which we are trying to assign a  type.
# Returns:
#   The XML-RPC type is one of int, boolean, double, string,
#   dateTime.iso8601, base64, struct or array. However, we only return one
#   of struct, int, double, boolean or string unless we were passed a 
#   typed variable.
#
proc ::rpcvar::rpctype { arg } {
    set type {}
    if { [is_rpcvar $arg] } {
        set type [lindex $arg 4]
    } elseif {[uplevel array exists [list $arg]]} {
        set type "struct"
    } elseif {[string is integer -strict $arg]} {
        set type "int"
    } elseif {[string is double -strict $arg]} {
        # See: http://www.w3.org/TR/xmlschema-2/#float
        if {[expr {(abs($arg) > (pow(2,24)*pow(2,-149))) 
            && (abs($arg) < (pow(2,24)*pow(2,104)))}]} {
            set type "float"
        } else {
            set type "double"
        }
    } elseif {[string is boolean -strict $arg]} { 
        set type "boolean"
    } else {
        set type "string"
    }
    return $type
}

# -------------------------------------------------------------------------

# Description:
#   --- IT DOESN'T WORK LIKE THIS NOW -- DELETE ME ?!
#   --- we declare arrays as int() and struct() or MyType()
#   --- Still used in SOAP.tcl
#   ---
#   If the value is not a typed variable, then there cannot be a subtype.
#   otherwise we are looking for array(int) or struct(Typename) etc.
# Result:
#   Either the subtype of an array, or an empty string.
#
proc ::rpcvar::rpcsubtype { arg } {
    set subtype {}
    if {[is_rpcvar $arg]} {
        regexp {([^(]+)(\((.+)\))?} [lindex $arg 4] -> type -> subtype
    }
    return $subtype
}

# -------------------------------------------------------------------------

# Description:
#   Retrieve the value from a typed variable or return the input.
# Parameters:
#   arg - either a value or a reference to a typed variable for which to 
#         return the value
# Result:
#   Returns the value of a typed variable.
#   If arg is not a typed variable it return the contents of arg
#
proc ::rpcvar::rpcvalue { arg } {
    if { [is_rpcvar $arg] } {
        return [lindex $arg 5]
    } else {
        return $arg
    }
}
# -------------------------------------------------------------------------

# Description:
#   Retrieve the xml namespace assigned to this variable. This is only used
#   by SOAP.
# Parameters:
#   varref - reference to an RPC typed variable.
# Result:
#   Returns the set namespace or an empty value is no namespace is assigned.
#
proc ::rpcvar::rpcnamespace { varref } {
    set xmlns {}
    if { [is_rpcvar $varref] } {
        set xmlns [lindex $varref 1]
    }
    return $xmlns
}

# -------------------------------------------------------------------------

# Description:
#   Retrieve the XML attributes assigned to this variable. This is only
#   relevant to SOAP.
# Parameters:
#   varref - reference to an RPC typed variable.
# Result:
#   Returns the list of name/value pairs for the assigned attributes. The
#   list is suitable for use in array set.
#
proc ::rpcvar::rpcattributes { varref } {
    set attrs {}
    if {[is_rpcvar $varref]} {
        set attrs [lindex $varref 2]
    }
    return $attrs
}

# -------------------------------------------------------------------------

# Description:
#   Retrieve the optional list of SOAP Header elements defined for this
#   variable. The intent of this mechanism is to allow a returning procedure
#   to specify SOAP Header elements if required.
# Results:
#
proc ::rpcvar::rpcheaders { varref } {
    set head {}
    if {[is_rpcvar $varref]} {
        set head [lindex $varref 3]
    }
    return $head
}

# -------------------------------------------------------------------------

# Description:
#   Define a SOAP type for use with the TclSOAP package. This allows you
#   to specify the SOAP XML namespace and typename for a chunk of data and
#   enables the TclSOAP client code to determine the SOAP type imformation
#   to put on request data.
# Options:
#   -enum             - flag the type as an enumerated type
#   -exists typename  - boolean true if typename is defined
#   -info typename    - return the definition of typename
# Parameters
#   typelist          - list of the type information needed to define the 
#                       new type.
#   typename          - the name of the new type
# Notes:
#   If the typename has already been defined then it will be overwritten.
#   For enumerated types, the typelist is the list of valid enumerator names.
#   Each enumerator may be a two element list, in which case the first element
#   is the name and the second is the integer value.
#
proc ::rpcvar::typedef {args} {
    variable typedefs
    variable typens
    variable enums

    set namespace {}
    set enum 0
    while {[string match -* [lindex $args 0]]} {
        switch -glob -- [lindex $args 0] {
            -n* {
                set namespace [lindex $args 1]
                set args [lreplace $args 0 0]
                if {[llength $args] == 1} {
                    if {[catch {set typens($namespace)} r]} {
                        set r {}
                    }
                    return $r
                }
            }
            -ex* {
                set typename [lindex $args 1]
                return [info exists typedefs($typename)]
            }
            -en* {
                set enum 1
            }
            -i* {
                set typename [lindex $args 1]
                if {[catch {set typedefs($typename)} typeinfo]} {
                    set typeinfo {}
                }
                return $typeinfo
            }
            --  { 
                set args [lreplace $args 0 0]
                break 
            }
            default { return -code error "unknown option \"[lindex $args 0]\""}
        }
        set args [lreplace $args 0 0]
    }

    if {[llength $args] != 2} {
        return -code error "wrong # args: should be \
                \"typedef ?-namespace uri? ?-enum? typelist typename\n\
                \                     or \"typedef ?-exists? ?-info? typename\""
    }

    set typelist [lindex $args 0]
    set typename [lindex $args 1]

    if {$enum} {
        set typedefs($typename) enum
        set enums($typename) $typelist
    } else {
        set typedefs($typename) $typelist
    }
    set typens($typename) $namespace

    return $typename
}

# -------------------------------------------------------------------------

# Description:
#   Check that the value is suitable for type. Basically for enum's
# Result:
#   Returns a boolean true/false value.
proc ::rpcvar::rpcvalidate {type value} {
    variable enums
    if {[typedef -info $type] == "enum"} {
        if {[lsearch -exact $enums($type) $value] == -1} {
            return 0
        }
    }
    return 1
}

# -------------------------------------------------------------------------
#  typdef usage:
#
#  typedef -namespace urn:tclsoap-Test float TclFloat
#
#  typedef -enum -namespace urn:tclsoap-Test {red {green 3} {blue 9}} Colour
#
#  typedef {
#      larry     integer
#      moe       integer
#      curly     integer
#  } Stooges
#  => SOAP::create m -params {myStruct Stooges}
#  => m {larry 23 curly -98 moe 9}
#
#  typedef -namespace urn:soapinterop.org {
#      varInt    integer
#      varFloat  float
#      varString string
#  } SOAPStruct;    
#
#  => SOAP::create zm ... -params {myStruct SOAPStruct}
#  => zm {varInt 2 varFloat 2.2 varString "hello"}
#
#  typedef {
#      arrInt     int()
#      stooges    Stooges()
#      arrString  string()
#      arrColours Colour()
#  } arrStruct
#  => SOAP::create m -params {myStruct arrStruct}
#  => m {arrInt {1 2 3 4 5} \
#        stooges { \
#          {moe 1 larry 2 curly 3} \
#          {moe 1 larry 2 curly 3} \
#        } \
#        arrString {One Two Three} \
#        arrColours {red blue green}\
#    }

# -------------------------------------------------------------------------

proc ::rpcvar::default_schemas {soapenv} {

    if {[string match $soapenv "http://schemas.xmlsoap.org/soap/encoding/"]} {
        # SOAP 1.1
        return [list \
                    "xmlns:xsd"      "http://www.w3.org/1999/XMLSchema" \
                    "xmlns:xsi"      "http://www.w3.org/1999/XMLSchema-instance" ]
    }

    if {[string match $soapenv "http://www.w3.org/2001/06/soap-encoding"]} {        
        # SOAP 1.2
        return [list \
                    "xmlns:xsd"      "http://www.w3.org/2001/XMLSchema" \
                    "xmlns:xsi"      "http://www.w3.org/2001/XMLSchema-instance" ]
    }

    return -code error "invalid soap version: \"$soapenv\" is not a valid SOAP URL"
}

# initialize with the SOAP 1.1 encodings for xsd and SOAP-ENC
#
proc ::rpcvar::init_builtins {} {
    # The xsi types from http://www.w3.org/TR/xmlschema-2/ section 3.2 & 3.3
    # the uri's for these are http://www.w33.org/2001/XMLSchema#int etc
    set xsd2001 [list \
            string normalizedString boolean decimal integer float double \
            duration dateTime time date gYearMonth gYear gMonthDay gDay \
            gMonth hexBinary base64Binary anyURI QName NOTATION \
            token language NMTOKEN NMTOKENS Name NCName ID IDREF IDREFS \
            ENTITY ENTITIES nonPositiveInteger negativeInteger long int \
            short byte nonNegativeInteger unsignedLong unsignedInt \
            unsignedShort unsignedByte positiveInteger anyType anySimpleType]
    
    # The SOAP 1.1 encoding: uri = http://www.w3.org/1999/XMLSchema
    set xsd1999 [list \
            string boolean float double decimal timeDuration \
            recurringDuration binary uriReference ID IDREF ENITY NOTATION \
            QName language IDREFS ENTITIES NMTOKEN NMTOKENS Name NCName \
            integer nonPositiveInteger negativeInteger long int short byte \
            nonNegativeInteger unsignedLong unsignedInt unsignedShort \
            unsignedByte positiveInteger timeInstant time timePeriod date \
            month year century recurringDate recurringDay]

    # SOAP 1.1 encoding: uri = http://schemas.xmlsoap.org/soap/encoding/
    set soapenc [list \
            arrayCoordinate Array Struct base64 string boolean float double \
            decimal timeDuration recurringDuration binary uriReference ID \
            IDREF ENTITY NOTATION QName language IDREFS ENTITIES NMTOKEN \
            NMTOKENS Name NCName integer nonPositiveInteger negativeInteger \
            long int short byte nonNegativeInteger unsignedLong unsignedShort \
            unsignedByte positiveInteger timeInstant time timePeriod date \
            month year century recurringDate recurringDay ur-type]

    foreach type $soapenc {
        _init SOAP-ENC $type
    }

    foreach type $xsd1999 {
        _init xsd $type
    }
}

# Initialize the core SOAP types. xsd and SOAP-ENC namespace names are
# pre-defined within the TclSOAP framework. All other namespaces will
# have to be fully specified
if {! [info exists ::rpcvar::typedefs]} {
    ::rpcvar::init_builtins
}


# -------------------------------------------------------------------------
# -------------------------------------------------------------------------
namespace eval ::types {
    variable types
    namespace export typedef
}

proc ::types::typedef {args} {
    variable types
    array set opts {namespace {}}
    while {[string match -* [set option [lindex $args 0]]]} {
        switch -glob -- $option {
            -n* { set opts(namespace) [Pop args 1] }
            -ex* {
                set typename [lindex $args 1]
                if {[string length $opts(namespace)] > 0} {
                    set typename $opts(namespace):$typename
                }
                return [info exists types($typename)]
            }
            -i* {
                set namespace *
                set typename [lindex $args 1]
                if {[string length $opts(namespace)] > 0} {
                    set namespace $opts(namespace)
                }
                set typename $namespace:$typename
                if {[catch {array get types $typename} typeinfo]} {
                    set typeinfo {}
                }
                return $typeinfo
            }
            -- { Pop args ; break }
            default {
                set options [join [lsort [array names opts]] ", -"]
                return -code error "bad option $option:\
                    must be one of -$options"
            }
        }
        Pop args
    }
    
    if {[llength $args] != 2} {
        return -code error "wrong # args: should be \
                \"typedef ?-namespace uri? ?-enum? typelist typename\n\
                \                     or \"typedef ?-exists? ?-info? typename\""
    }

    set typelist [lindex $args 0]
    set typename [lindex $args 1]

    set types($opts(namespace):$typename) $typelist
    return $typename
}

proc ::types::SetupBuiltins {} {
    # The xsi types from http://www.w3.org/TR/xmlschema-2/ section 3.2 & 3.3
    # the uri's for these are http://www.w3.org/2001/XMLSchema#int etc
    set xsd2001 [list \
            string normalizedString boolean decimal integer float double \
            duration dateTime time date gYearMonth gYear gMonthDay gDay \
            gMonth hexBinary base64Binary anyURI QName NOTATION \
            token language NMTOKEN NMTOKENS Name NCName ID IDREF IDREFS \
            ENTITY ENTITIES nonPositiveInteger negativeInteger long int \
            short byte nonNegativeInteger unsignedLong unsignedInt \
            unsignedShort unsignedByte positiveInteger anyType anySimpleType]
    foreach type $xsd2001 {
        typedef -namespace http://www.w3.org/2001/XMLSchema $type $type
    }
    
    # The SOAP 1.1 encoding: uri = http://www.w3.org/1999/XMLSchema
    set xsd1999 [list \
            string boolean float double decimal timeDuration \
            recurringDuration binary uriReference ID IDREF ENITY NOTATION \
            QName language IDREFS ENTITIES NMTOKEN NMTOKENS Name NCName \
            integer nonPositiveInteger negativeInteger long int short byte \
            nonNegativeInteger unsignedLong unsignedInt unsignedShort \
            unsignedByte positiveInteger timeInstant time timePeriod date \
            month year century recurringDate recurringDay]
    foreach type $xsd1999 {
        typedef -namespace http://www.w3.org/1999/XMLSchema $type $type
    }

    # SOAP 1.1 encoding: uri = http://schemas.xmlsoap.org/soap/encoding/
    set soapenc [list \
            arrayCoordinate Array Struct base64 string boolean float double \
            decimal timeDuration recurringDuration binary uriReference ID \
            IDREF ENTITY NOTATION QName language IDREFS ENTITIES NMTOKEN \
            NMTOKENS Name NCName integer nonPositiveInteger negativeInteger \
            long int short byte nonNegativeInteger unsignedLong unsignedShort \
            unsignedByte positiveInteger timeInstant time timePeriod date \
            month year century recurringDate recurringDay ur-type]
    foreach type $soapenc {
        typedef -namespace http://schemas.xmlsoap.org/soap/encoding/ \
            $type $type
    }
}

proc ::types::Pop {varname {nth 0}} {
    upvar $varname args
    set r [lindex $args $nth]
    set args [lreplace $args $nth $nth]
    return $r
}

# -------------------------------------------------------------------------

package provide rpcvar $::rpcvar::version

# -------------------------------------------------------------------------
# Local variables:
#    indent-tabs-mode: nil
# End:
