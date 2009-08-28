# soapinteropC.tcl - Copyright (C) 2001 Pat Thoyts <Pat.Thoyts@bigfoot.com>
#
# SOAP Interoperability Lab "Round 2" Proposal C Client Tests
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
# @(#)$Id: soapinteropC.tcl,v 1.2 2001/10/07 22:28:08 patthoyts Exp $

package require -exact soapinterop::base 1.0
package provide soapinterop::C 1.0

namespace eval soapinterop {
}

proc soapinterop::round2:proposalC {proxy} {
    validate.echoMeStringRequest $proxy
    validate.echoMeStructRequest $proxy
}

proc soapinterop::validate.echoMeStringRequest {proxy} {
    SOAP::create echoVoid -proxy $proxy \
	-uri http://soapinterop.org/ \
	-action http://soapinterop.org/

    catch {validate.emsr:A} msg  ; puts "$msg"
    catch {validate.emsr:B} msg  ; puts "$msg"
    catch {validate.emsr:C} msg  ; puts "$msg"
    catch {validate.emsr:D} msg  ; puts "$msg"
    catch {validate.emsr:E} msg  ; puts "$msg"
    catch {validate.emsr:F} msg  ; puts "$msg"
}

proc soapinterop::validate.echoMeStructRequest {proxy} {
    
    SOAP::create echoVoid -proxy $proxy \
	-uri http://soapinterop.org/ \
	-action http://soapinterop.org/

    catch {validate.emtr:A} msg  ; puts "$msg"
    catch {validate.emtr:B} msg  ; puts "$msg"
    catch {validate.emtr:C} msg  ; puts "$msg"
    catch {validate.emtr:D} msg  ; puts "$msg"
    catch {validate.emtr:E} msg  ; puts "$msg"
    catch {validate.emtr:F} msg  ; puts "$msg"
}

# -------------------------------------------------------------------------
# Header contructors
# -------------------------------------------------------------------------

proc soapinterop::emsr:header {actor mustUnderstand request {siop http://soapinterop.org/echoheader/} } {
    return [list siop:echoMeStringRequest \
		[rpcvar::rpcvar -attr [list xmlns:siop $siop \
					   SOAP-ENV:mustUnderstand $mustUnderstand \
					   SOAP-ENV:actor $actor]\
		     string $request]]
}

proc soapinterop::emtr:header {actor mustUnderstand request {siop http://soapinterop.org/echoheader/}} {
    return [list siop:echoMeStructRequest \
		[rpcvar::rpcvar -attr [list xmlns:siop $siop  \
					   SOAP-ENV:mustUnderstand $mustUnderstand \
					   SOAP-ENV:actor $actor]\
		     SOAPStruct $request]]
}

# -------------------------------------------------------------------------

proc soapinterop::validate.emsr:A {} {

    set actor  http://schemas.xmlsoap.org/soap/actor/next
    set request "TestA"
    set header [emsr:header $actor 0 $request]

    set ::SOAP::_soapinterop_echoVoid(headers) {} ;# HACK
    echoVoid -header $header
    array set h [SOAP::cget echoVoid headers]

    if {! [info exists h(echoMeStringResponse)]} {
	error "failed: no suitable header returned."
    }
    if {![string match $request $h(echoMeStringResponse)]} {
	error "failed: reply mismatch \"$request\" != \"$h(echoMeStringResponse)\""
    }
    return "echoMeStringRequest(actor=next, mustUnderstand=0)"
}

proc soapinterop::validate.emsr:B {} {

    set actor  http://schemas.xmlsoap.org/soap/actor/next
    set request "TestB"
    set header [emsr:header $actor 1 $request]

    set ::SOAP::_soapinterop_echoVoid(headers) {} ;# HACK
    echoVoid -header $header
    array set h [SOAP::cget echoVoid headers]

    if {! [info exists h(echoMeStringResponse)]} {
	error "failed: no suitable header returned."
    }
    if {![string match $request $h(echoMeStringResponse)]} {
	error "failed: reply mismatch \"$request\" != \"$h(echoMeStringResponse)\""
    }
    return "echoMeStringRequest(actor=next, mustUnderstand=1)"
}

proc soapinterop::validate.emsr:C {} {

    set actor  "http://tempuri.org/not/me"
    set request "TestC"
    set header [emsr:header $actor 0 $request]

    set ::SOAP::_soapinterop_echoVoid(headers) {} ;# HACK
    echoVoid -header $header
    array set h [SOAP::cget echoVoid headers]

    if {[info exists h(echoMeStringResponse)]} {
	error "failed: recipient should not have replied: $h(echoMeStringResponse)"
    }
    return "echoMeStringRequest(actor=other, mustUnderstand=0)"
}

proc soapinterop::validate.emsr:D {} {

    set actor  "http://tempuri.org/not/me"
    set request "TestD"
    set header [emsr:header $actor 1 $request]

    set ::SOAP::_soapinterop_echoVoid(headers) {} ;# HACK
    echoVoid -header $header
    array set h [SOAP::cget echoVoid headers]

    if {[info exists h(echoMeStringResponse)]} {
	error "failed: recipient should not have replied: $h(echoMeStringRequest)"
    }
    return "echoMeStringRequest(actor=other, mustUnderstand=1)"
}

# actor=next, mustUnderstand=1, wrong namespace => error.
proc soapinterop::validate.emsr:E {} {

    set actor  http://schemas.xmlsoap.org/soap/actor/next
    set request "TestE"
    set header [emsr:header $actor 1 $request http://tempuri.org/not/siop]

    set ::SOAP::_soapinterop_echoVoid(headers) {} ;# HACK
    if {[catch {echoVoid -header $header} msg]} {
	if {![regexp {^([^ ]*:)?MustUnderstand} $msg]} {
	    error "failed: expected MustUnderstand fault. Received \"$msg\""
	}
    } else {
	error "failed: should return error for incomprehensible required header"
    }

    return "echoMeStringRequest(actor=next, mustUnderstand=1, namespace=rubbish)"
}

# actor=next, mustUnderstand=1, wrong namespace => ignored.
proc soapinterop::validate.emsr:F {} {

    set actor  http://schemas.xmlsoap.org/soap/actor/next
    set request "TestF"
    set header [emsr:header $actor 0 $request http://tempuri.org/not/siop]

    set ::SOAP::_soapinterop_echoVoid(headers) {} ;# HACK
    if {[catch {echoVoid -header $header} msg]} {
	if {[regexp {^([^ ]*:)?MustUnderstand} $msg]} {
	    error "failed: received unexpected MustUnderstand fault."
	} else {
	    error "failed: received error \"$msg\""
	}
    }

    array set h [SOAP::cget echoVoid headers]
    if {[info exists h(echoMeStringResponse)]} {
	error "failed: recipient should not have replied: $h(echoMeStringRequest)"
    }

    return "echoMeStringRequest(actor=next, mustUnderstand=0, namespace=rubbish)"
}
# -------------------------------------------------------------------------

proc soapinterop::validate.emtr:A {} {

    set request [list varString "TestA" varInt [rand_int] varFloat [rand_float]]
    set actor  http://schemas.xmlsoap.org/soap/actor/next
    set header [emtr:header $actor 0 $request]

    set ::SOAP::_soapinterop_echoVoid(headers) {} ;# HACK
    echoVoid -header $header
    array set h [SOAP::cget echoVoid headers]

    if {! [info exists h(echoMeStructResponse)]} {
	error "failed: no suitable header returned."
    }
    if {[catch {validateSOAPStruct $request $h(echoMeStructResponse)} msg]} {
	error "failed: $msg"
    }
    return "echoMeStructRequest(actor=next, mustUnderstand=0)"
}

proc soapinterop::validate.emtr:B {} {

    set request [list varString "TestB" varInt [rand_int] varFloat [rand_float]]
    set actor  http://schemas.xmlsoap.org/soap/actor/next
    set header [emtr:header $actor 1 $request]

    set ::SOAP::_soapinterop_echoVoid(headers) {} ;# HACK
    echoVoid -header $header
    array set h [SOAP::cget echoVoid headers]

    if {! [info exists h(echoMeStructResponse)]} {
	error "failed: no suitable header returned."
    }
    if {[catch {validateSOAPStruct $request $h(echoMeStructResponse)} msg]} {
	error "failed: $msg"
    }
    return "echoMeStructRequest(actor=next, mustUnderstand=1)"
}

proc soapinterop::validate.emtr:C {} {

    set request [list varString "TestC" varInt [rand_int] varFloat [rand_float]]
    set actor  "http://tempuri.org/not/me"
    set header [emtr:header $actor 0 $request]

    set ::SOAP::_soapinterop_echoVoid(headers) {} ;# HACK
    echoVoid -header $header
    array set h [SOAP::cget echoVoid headers]

    if {[info exists h(echoMeStructResponse)]} {
	error "failed: recipient should not have replied: $h(echoMeStructResponse)"
    }
    return "echoMeStructRequest(actor=other, mustUnderstand=0)"
}

proc soapinterop::validate.emtr:D {} {

    set request [list varString "TestD" varInt [rand_int] varFloat [rand_float]]
    set actor  "http://tempuri.org/not/me"
    set header [emtr:header $actor 1 $request]

    set ::SOAP::_soapinterop_echoVoid(headers) {} ;# HACK
    echoVoid -header $header
    array set h [SOAP::cget echoVoid headers]

    if {[info exists h(echoMeStructResponse)]} {
	error "failed: recipient should not have replied: $h(echoMeStructResponse)"
    }
    return "echoMeStructRequest(actor=other, mustUnderstand=1)"
}

proc soapinterop::validate.emtr:E {} {

    set request [list varString "TestE" varInt [rand_int] varFloat [rand_float]]
    set actor  "http://schemas.xmlsoap.org/soap/actor/next"
    set header [emtr:header $actor 1 $request http://tempuri.org/not/siop]

    set ::SOAP::_soapinterop_echoVoid(headers) {} ;# HACK
    if {[catch {echoVoid -header $header} msg]} {
	if {![regexp {^([^ ]*:)?MustUnderstand} $msg]} {
	    error "failed: expected MustUnderstand fault. Received \"$msg\""
	}
    } else {
	error "failed: should return error for incomprehensible required header"
    }
    return "echoMeStructRequest(actor=next, mustUnderstand=1, namespace=rubbish)"
}

proc soapinterop::validate.emtr:F {} {

    set request [list varString "TestF" varInt [rand_int] varFloat [rand_float]]
    set actor  "http://schemas.xmlsoap.org/soap/actor/next"
    set header [emtr:header $actor 0 $request http://tempuri.org/not/siop]

    set ::SOAP::_soapinterop_echoVoid(headers) {} ;# HACK
    if {[catch {echoVoid -header $header} msg]} {
	if {[regexp {^([^ ]*:)?MustUnderstand} $msg]} {
	    error "failed: received unexpected MustUnderstand fault."
	} else {
	    error "failed: received error \"$msg\""
	}
    }

    array set h [SOAP::cget echoVoid headers]
    if {[info exists h(echoMeStructResponse)]} {
	error "failed: recipient should not have replied: $h(echoMeStructResponse)"
    }

    return "echoMeStructRequest(actor=next, mustUnderstand=0, namespace=rubbish)"
}

# -------------------------------------------------------------------------

#
# Local variables:
#   mode: tcl
#   indent-tabs-mode: nil
# End:
