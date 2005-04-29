# soapinterop.tcl - Copyright (C) 2001 Pat Thoyts <Pat.Thoyts@bigfoot.com>
#
# The XMethods SOAP Interoperability Lab Test Suite. Round 1 and the
# SOAP Interoperability Laboratory Test Suite: Round 2 (base, B and C)
#
# See http://www.xmethods.net/soapbuilders/proposal.html
#
# General Guidelines:
# * SOAPAction should be present, and quoted. 
#
# * Method namespace should be a well-formed , legal URI 
#
# * Each server implementation is free to use a SOAPAction value and
#   method namespace value of its own choosing.  However, if the
#   implementation has no preference, we suggest using a SOAPAction
#   value of "urn:soapinterop" and a method namespace of
#   "http://soapinterop.org/"
#
# * For this method set, implementations may carry explicit typing
#   information on the wire, but should not require it for incoming
#   messages.
#
# * Since we are dealing strictly with Section 5 encoding, encodingStyle
#   should be present and set to
#   http://schemas.xmlsoap.org/soap/encoding/
#
# * WSDL is NOT required on the server side.  Implementations that
#   require WSDL for binding are responsible for creating it locally on
#   the client.
#
# The SOAPStruct struct is defined as:
# <complexType name="SOAPStruct">
#   <all>
#     <element name="varString" type="xsd:string" />
#     <element name="varInt" type="xsd:int" /> 
#     <element name="varFloat" type="xsd:float" /> 
#   </all>
# </complexType>
#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the accompanying file `LICENSE'
# for more details.
# -------------------------------------------------------------------------
#
# @(#)$Id: soapinterop.tcl,v 1.4 2002/02/26 22:48:38 patthoyts Exp $

package require SOAP
package require rpcvar

namespace eval http://soapinterop.org/ {
namespace import -force ::rpcvar::*

    # expose the SOAP methods
    SOAP::export echoInteger echoFloat echoString echoStruct \
	    echoIntegerArray echoFloatArray echoStringArray echoStructArray \
	    echoVoid echoDate echoBase64 \
            echoStructAsSimpleTypes echoSimpleTypesAsStruct \
            echo2DStringArray echoNestedStruct echoNestedArray

    typedef -namespace http://soapinterop.org/xsd { \
	    varString string \
	    varInt    int \
	    varFloat  float \
    } SOAPStruct

    # Description:
    #  Echo a string parameter.
    #
    proc echoString {inputString} {
	return $inputString
    }

    # Description:
    #  Echo an integer parameter.
    #
    proc echoInteger {inputInteger} {
	if {! [string is integer -strict $inputInteger]} {
	    return -code error "invalid arg: \"inputInteger\" must be an integer"
	}
	return $inputInteger
    }
    
    # Description:
    #  Echo a float parameter.
    #
    proc echoFloat {inputFloat} {
	if {! [string is double -strict $inputFloat]} {
	    return -code error "invalid arg: \"inputFloat\" must be a float"
	}
	return $inputFloat
    }

    # Description:
    #  Echo a base64 encoded binary value.
    #
    proc echoBase64 {inputBase64} {
	return [rpcvar "base64" $inputBase64]
    }

    # Description:
    #  Echo a SOAP date value.
    #
    proc echoDate {inputDate} {
	return [rpcvar "dateTime" $inputDate]
    }

    # Description:
    #   echoVoid is used in the base interop tests as a void method taking no
    #   arguments and returning no value. It is also the base for the proposal C
    #   interop tests where we are testing the SOAP Header processing.
    #
    proc echoVoid {} {
	# handle headers. Any headers destined for us should be removed
	# from the dom tree because later the CGI framework is going to 
	# check and any header with mustUnderstand == 1 && actor == me
	# will throw an exception.

	set header {}
	set headerNode [uplevel 2 set headerNode]

	if {$headerNode != {}} {
	    if {[set node [lindex [SOAP::selectNode $headerNode "echoMeStringRequest"] 0]] != {}} {
		set actor [SOAP::getElementAttribute $node actor]
		if {$actor == {} || \
			[string match $actor "http://schemas.xmlsoap.org/soap/actor/next"] != 0} {
		    if {[string match "http://soapinterop.org/echoheader/" [SOAP::namespaceURI $node]]} {
			
			lappend header "s:echoMeStringResponse" \
			    [rpcvar -attribute {xmlns:s "http://soapinterop.org/echoheader/"} \
				 string [SOAP::decomposeSoap $node]]
			# having successfully processed the node, delete it
			dom::node removeChild $headerNode $node
		    }
		}
	    } elseif {[set node [SOAP::selectNode $headerNode "echoMeStructRequest"]] != {}} {
		set actor [SOAP::getElementAttribute $node actor]
		if {$actor == {} || \
			[string match $actor "http://schemas.xmlsoap.org/soap/actor/next"] != 0} {
		    if {[string match "http://soapinterop.org/echoheader/" [SOAP::namespaceURI $node]]} {
			
			lappend header "s:echoMeStructResponse" \
			    [rpcvar -attribute {xmlns:s "http://soapinterop.org/echoheader/"} \
				 string [SOAP::decomposeSoap $node]]
			# having successfully processed the node, delete it
			dom::node removeChild $headerNode $node
		    }
		}
	    }
	}

	return [rpcvar -header $header int {}]
    }

    # Description:
    #  Echo and validate a structure parameter
    #
    proc echoStruct {inputStruct} {
        array set i $inputStruct
        
        set r(varString)  $i(varString)
        set r(varInt)     $i(varInt)
        set r(varFloat)   $i(varFloat)

	return [rpcvar SOAPStruct r]
    }

    # Description:
    #  Validate and echo an array of integers
    #
    proc echoIntegerArray {inputIntegerArray} {
        foreach int $inputIntegerArray {
            if {! [string is integer -strict $int]} {
                return -code error "invalid array element: \"$int\" must be an integer"
            }
        }
	return [rpcvar int() $inputIntegerArray]
    }

    # Description:
    #  Echo an array of floats
    #
    proc echoFloatArray {inputFloatArray} {
        foreach flt $inputFloatArray {
            if {! [string is double -strict $flt] } {
                return -code error "invalid array element: \"$flt\" must be a float"
            }
        }
	return [rpcvar float() $inputFloatArray]
    }

    # Description:
    #  Echo an array of strings.
    #
    proc echoStringArray {inputStringArray} {
	return [rpcvar string() $inputStringArray]
    }
    
    # Description:
    #  Echo an array of SOAPStructs.
    #
    proc echoStructArray {inputStructArray} {
        set result {}
        foreach elt $inputStructArray {
            array set i $elt
            set r(varString)  $i(varString)
            set r(varInt)     $i(varInt)
            set r(varFloat)   $i(varFloat)
            lappend result [array get r]
        }
	return [rpcvar SOAPStruct() $result]
    }

    proc echoDecimal {inputDecimal} {
        return [rpcvar decimal $inputDecimal]
    }

    proc echoBoolean {inputBoolean} {
        if {! [string is boolean -strict $inputBoolean] } {
            return -code error "invalid parameter: \"$inputBoolean\" must be a boolean"
        }
        return [rpcvar boolean $inputBoolean]
    }

    # ---------------------------------------------------------------------
    # SOAP Interop Proposal B
    # ---------------------------------------------------------------------

    # Description:
    #  Method accepts a struct of type SOAPStruct (see above) and returns the
    #  structure members as a parameter list. This requires the use of the
    #  the -paramlist option to rpcvar to get the correct reply structure as
    #  we are returning multiple values here.
    #
    proc echoStructAsSimpleTypes {inputStruct} {
        array set info $inputStruct
        return [rpcvar -paramlist \
                    outputString  [rpcvar string $info(varString)] \
                    outputInteger [rpcvar int $info(varInt)] \
                    outputFloat   [rpcvar float $info(varFloat)]]
    }
    
    # Description:
    #  This method constructs a SOAPStruct return value from three input
    #  parameters. The SOAP framework will be responsible for making sure the
    #  right named parameters are passed into the correct argument.
    #
    proc echoSimpleTypesAsStruct {inputString inputInteger inputFloat} {
        return [rpcvar SOAPStruct [list \
                                       varInt    $inputInteger \
                                       varFloat  $inputFloat \
                                       varString $inputString ]]
    }

    # Description:
    #  This method accepts a 2 dimensional array of strings and echoes it to
    #  the client.
    # Notes:
    #  Currently TclSOAP does not support 2D arrays.
    #
    rpcvar::typedef -namespace http://soapinterop.org/xsd \
	    string(,) ArrayOfString2D

    proc echo2DStringArray {input2DStringArray} {
        return [rpcvar ArrayOfString2D $input2DStringArray]
        #return -code error "2D Arrays not implemented in this version of TclSOAP" {} Server
    }

    # Description:
    #  This method takes a single struct containing another struct and echoes
    #  it all back the the client. We rebuild the reply so that we validate the 
    #  input.
    # Notes:
    #  Note that the structure type definition provide the type info for the 
    #  members of the structure. We _must_ not feed rpcvar-based lists into the
    #  structure itself.
    #
    typedef -namespace http://soapinterop.org/xsd { \
	    varString string \
	    varInt    int \
	    varFloat  float \
            varStruct SOAPStruct \
    } SOAPStructStruct

    proc echoNestedStruct {inputStruct} {
        array set o $inputStruct
        array set i $o(varStruct)

        set ri(varString) $i(varString)
        set ri(varInt)    $i(varInt)
        set ri(varFloat)  $i(varFloat)

        set r(varString)  $o(varString)
        set r(varInt)     $o(varInt)
        set r(varFloat)   $o(varFloat)
        set r(varStruct)  [array get ri]

        return [rpcvar SOAPStructStruct r]
    }

    # Description:
    #  This method takes a single struct that contains an array of strings
    #  and echoes it all back the the client. We rebuild the reply so that
    #  we validate the input.
    #
    typedef -namespace http://soapinterop.org/xsd { \
	    varString string \
	    varInt    int \
	    varFloat  float \
            varArray  string() \
    } SOAPArrayStruct

    proc echoNestedArray {inputStruct} {
        array set i $inputStruct

        set a {}
        foreach item $i(varArray) {
            lappend a $item
        }

        set r(varString) $i(varString)
        set r(varInt) $i(varInt)
        set r(varFloat) $i(varFloat)
        set r(varArray) $a

        return [rpcvar SOAPArrayStruct r]
    }
}

#
# Local variables:
# mode: tcl
# End:
