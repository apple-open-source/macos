# soapinteropB.tcl - Copyright (C) 2001 Pat Thoyts <Pat.Thoyts@bigfoot.com>
#
# SOAP Interoperability Lab "Round 2" Proposal B Client Tests
# 
# See http://www.whitemesa.com/interop.htm for details.
#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the accompanying file `LICENSE'
# for more details.
# -------------------------------------------------------------------------
#
# @(#)$Id: soapinteropB.tcl,v 1.4 2002/08/20 00:38:00 patthoyts Exp $

package require -exact soapinterop::base 1.0
package provide soapinterop::B 1.0

namespace eval soapinterop {

    rpcvar::typedef -namespace http://soapinterop.org/xsd { \
	    varString string \
	    varInt    int \
	    varFloat  float \
	    varStruct SOAPStruct } SOAPStructStruct

    rpcvar::typedef -namespace http://soapinterop.org/xsd \
	    string() Arrayofstring

    # FIX ME
    rpcvar::typedef -namespace http://soapinterop.org/xsd \
	    string()() ArrayOfString2D
    
    rpcvar::typedef -namespace http://soapinterop.org/xsd { \
	    varString string \
	    varInt    int \
	    varFloat  float \
	    varArray  Arrayofstring } SOAPArrayStruct
}

# -------------------------------------------------------------------------

# Proposal B Methods
proc soapinterop::create:proposalB {proxy args} {
    variable action
    variable uri

    set action http://soapinterop.org/

    SOAP::create echoStructAsSimpleTypes -proxy $proxy -uri $uri \
	-action $action -params {inputStruct SOAPStruct}
    SOAP::create echoSimpleTypesAsStruct -proxy $proxy -uri $uri \
	-action $action \
	-params {inputString string inputInteger int inputFloat float}
    SOAP::create echo2DStringArray -proxy $proxy -uri $uri \
	-action $action -params {input2DStringArray ArrayOfString2D}
    SOAP::create echoNestedStruct -proxy $proxy -uri $uri -action $action \
	-params {inputStruct SOAPStructStruct}
    SOAP::create echoNestedArray -proxy $proxy -uri $uri -action $action \
	-params {inputStruct SOAPArrayStruct}

    if {$args != {}} {
        foreach method [list echoStructAsSimpleTypes \
                            echoSimpleTypesAsStruct\
                            echo2DStringArray \
                            echoNestedStruct \
                            echoNestedArray] {
           eval SOAP::configure $method $args
       }
    }
}

# -------------------------------------------------------------------------

proc soapinterop::round2:proposalB {proxy args} {
    if {$proxy != {}} {
        eval create:proposalB [list $proxy] $args
    }

    catch {validate.echoStructAsSimpleTypes} msg ; puts "$msg"
    catch {validate.echoSimpleTypesAsStruct} msg ; puts "$msg"
    catch {validate.echoNestedArray} msg         ; puts "$msg"
    catch {validate.echoNestedStruct} msg        ; puts "$msg"
    catch {validate.echo2DStringArray} msg       ; puts "$msg"
}

# -------------------------------------------------------------------------

# Description:
#  Returns the struct parts individually.
#  We check that each member value was returned (we cannot assume a
#  particular order.
#
proc soapinterop::validate.echoStructAsSimpleTypes {} {
    array set q [list varString [rand_string] \
                     varInt    [rand_int] \
                     varFloat  [rand_float]]
    set r [echoStructAsSimpleTypes [array get q]]

    foreach {n e} [array get q] {
        if {[lsearch -exact $r $e] == -1} {
            error "failed: member $n not found in \"$r\""
        }
    }
    return "echoStructAsSimpleTypes"
}    

# -------------------------------------------------------------------------

proc soapinterop::validate.echoSimpleTypesAsStruct {} {
    set s [rand_string]
    set i [rand_int]
    set f [rand_float]
    array set r [echoSimpleTypesAsStruct $s $i $f]
    if {[catch {validateSOAPStruct [array get q] [array get r]} err]} {
        error "echoSimpleTypesAsStruct $err"
    }
    return "echoSimpleTypesAsStruct"
}

# -------------------------------------------------------------------------

proc soapinterop::validate.echoNestedStruct {} {
    array set q [list \
                     varString [rand_string] \
                     varFloat [rand_float] \
                     varInt [rand_int] \
                     varStruct [list \
                                    varString [rand_string]\
                                    varInt [rand_int]\
                                    varFloat [rand_float]]]
    array set r [echoNestedStruct [array get q]]
    if {[catch {validateSOAPStruct [array get q] [array get r]} err]} {
        error "echoNestedStruct $err"
    }
    array set aq $q(varStruct)
    array set ar $r(varStruct)
    if {[catch {validateSOAPStruct [array get aq] [array get ar]} err]} {
        error "echoNestedStruct substruct $err"
    }
    return "echoNestedStruct"
}

# -------------------------------------------------------------------------

proc soapinterop::validate.echoNestedArray {} {
    array set q [list \
                     varString [rand_string] \
                     varFloat [rand_float] \
                     varInt [rand_int] \
                     varArray [list red green blue]]
    array set r [echoNestedArray [array get q]]
    if {[catch {validateSOAPStruct [array get q] [array get r]} err]} {
        error "echoNestedArray $err"
    }
    if {[llength $r(varArray)] != [llength $q(varArray)]} {
        error "echoNestedArray failed: lists are different:\
                $q(varArray) != $r(varArray)"
    }
    set max [llength $q(varArray)]
    for {set n 0} {$n < $max} {incr n} {
	if {! [string match [lindex $q(varArray) $n] [lindex $r(varArray) $n]]} {
	    error "echoNestedArray failed: element $n is different:\
                   $q(varArray) != $r(varArray)"
	}
    }
    return "echoNestedArray"
}

# -------------------------------------------------------------------------

proc soapinterop::validate.echo2DStringArray {} {
    set q [list r0c0 r0c1 r0c2 r1c0 r1c1 r1c2]
    return -code error "echo2DStringArray not implemented"
}

# -------------------------------------------------------------------------
#
# Local variables:
#   mode: tcl
#   indent-tabs-mode: nil
# End:
