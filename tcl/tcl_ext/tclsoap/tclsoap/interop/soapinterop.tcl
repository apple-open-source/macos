# soapinterop.tcl - Copyright (C) 2001 Pat Thoyts <Pat.Thoyts@bigfoot.com>
#
# Client implementation of the SOAP Interoperability lab Round 1 Test Suite
# and Round 2 Proposal A.
#
# If you live behind a firewall and have an authenticating proxy web server
# try executing SOAP::proxyconfig and filling in the fields. This sets
# up the SOAP package to send the correct headers for the proxy to 
# forward the packets (provided it is using the `Basic' encoding scheme).
#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the accompanying file `LICENSE'
# for more details.
# -------------------------------------------------------------------------
#
# @(#)$Id: soapinterop.tcl,v 1.6 2002/02/26 23:50:13 patthoyts Exp $

package provide soapinterop::base 1.0

package require SOAP
package require XMLRPC
package require rpcvar
package require SOAP::http

namespace eval soapinterop {
    variable uri    "http://soapinterop.org/"
    variable action "http://soapinterop.org/"

    namespace export rand_string rand_int rand_float

    rpcvar::typedef -namespace http://soapinterop.org/xsd { \
	    varString string \
	    varInt    int \
	    varFloat  float} SOAPStruct

}

# -------------------------------------------------------------------------
# round 1 and the round 2 base tests are the same apart from the SOAPAction
# -------------------------------------------------------------------------

proc soapinterop::round1 {{proxy {}}} {
    set soapinterop::action urn:soapinterop
    return [validate:base $proxy]
}

proc soapinterop::round2 {{proxy {}}} {
    set soapinterop::action http://soapinterop.org/
    return [validate:base $proxy]
}

# ----------------------------------------------------------------------

proc soapinterop::create:base {proxy args} {
    variable uri
    variable action
    
    SOAP::create echoString  -proxy $proxy -uri $uri -action $action \
        -params {inputString string}
    SOAP::create echoInteger -proxy $proxy -uri $uri -action $action \
        -params {inputInteger int}
    SOAP::create echoFloat -proxy $proxy -uri $uri -action $action \
        -params {inputFloat float}
    SOAP::create echoStruct -proxy $proxy -uri $uri -action $action \
        -params {inputStruct SOAPStruct}
    
    SOAP::create echoStringArray -proxy $proxy -uri $uri -action $action \
        -params {inputStringArray string()}
    SOAP::create echoIntegerArray -proxy $proxy -uri $uri -action $action \
        -params {inputIntegerArray int()}
    SOAP::create echoFloatArray -proxy $proxy -uri $uri -action $action \
        -params {inputFloatArray float()}
    SOAP::create echoStructArray -proxy $proxy -uri $uri -action $action \
        -params {inputStructArray SOAPStruct()}
    
    SOAP::create echoBase64 -proxy $proxy -uri $uri -action $action \
        -params {inputBase64 base64}
    SOAP::create echoDate -proxy $proxy -uri $uri -action $action \
        -params {inputDate timeInstant}
    SOAP::create echoVoid -proxy $proxy -uri $uri -action $action \
        -params {}

    if {$args != {}} {
        foreach method [list echoVoid echoDate echoBase64 \
                            echoString echoInteger echoFloat echoStruct \
                            echoStringArray echoIntegerArray echoFloatArray \
                            echoStructArray] {
         eval SOAP::configure $method $args
     }
    }
}

proc soapinterop::validate:base {proxy args} {
    set soapinterop::action urn:soapinterop

    if {$proxy != {}} {
	eval create:base [list $proxy] $args
    }

    catch {validate.echoVoid} msg        ; puts "$msg"
    catch {validate.echoDate} msg        ; puts "$msg"
    catch {validate.echoBase64} msg      ; puts "$msg"
    catch {validate.echoInteger} msg     ; puts "$msg"
    catch {validate.echoFloat} msg       ; puts "$msg"
    catch {validate.echoString} msg      ; puts "$msg"
    catch {validate.echoIntegerArray} msg; puts "$msg"
    catch {validate.echoFloatArray} msg  ; puts "$msg"
    catch {validate.echoStringArray} msg ; puts "$msg"
    catch {validate.echoStruct} msg      ; puts "$msg"
    catch {validate.echoStructArray} msg ; puts "$msg"
}

# -------------------------------------------------------------------------
# Helper methods
# -------------------------------------------------------------------------

proc soapinterop::rand_float {} {
    set r [expr rand() * 200 - 100]
    set p [string first . $r]
    incr p 4
    return [string range $r 0 $p]
}

proc soapinterop::rand_int {} {
    return [expr int(rand() * 200 - 100) ]
}

proc soapinterop::rand_string {} {
    set cmds [info commands]
    set ndx [expr int(rand() * [llength $cmds])]
    return [lindex $cmds $ndx]
}

proc soapinterop::float=? {lhs rhs {dp 4}} {
    set lhs [format %0.${dp}f $lhs]
    set rhs [format %0.${dp}f $rhs]
    return [expr {$lhs == $rhs}]
}

proc soapinterop::list=? {lhs rhs} {
    if {[llength $lhs] != [llength $rhs]} {
        return false
    }
    for {set n 0} {$n < $max} {incr n} {
	if {[lindex $q $n] != [lindex $r $n]} {
            return false
	}
    }
    return true
}

# -------------------------------------------------------------------------
# Round 1 and Round 2 Base Tests
# -------------------------------------------------------------------------

proc soapinterop::validate.echoVoid {} {
    set r [echoVoid]
    if {$r != {}} { error "echoVoid failed" }
    return "echoVoid"
}

proc soapinterop::validate.echoDate {} {
    set d [clock format [clock seconds] -format {%Y-%m-%dT%H:%M:%S}]
    set r [echoDate $d]
    if {! [string match "$d*" $r]} {
	error "echoDate failed: $d != $r"
    }
    return "echoDate"
}

proc soapinterop::validate.echoBase64 {} {
    package require base64
    set check [array get ::tcl_platform]
    set q [join [base64::encode $check] {}]
    set result [echoBase64 $q]
    set r [base64::decode $result]
    if {![string match $check $r]} {
	error "echoBase64 failed: strings do not match"
    }
    return "echoBase64"
}

proc soapinterop::validate.echoInteger {} {
    set i [rand_int]
    set r [echoInteger $i]
    if {$i != $r} { error "echoInteger failed: $i != $r" }
    return "echoInteger"
}

# Tend to loose some decimal places. Check to ?? dp ??
proc soapinterop::validate.echoFloat {} {
    set f [rand_float]
    set r [echoFloat $f]
    if {! [float=? $f $r]} {
	error "echoFloat failed: $f != $r" 
    }
    return "echoFloat"
}

proc soapinterop::validate.echoString {} {
    set s [array get ::tcl_platform]
    set r [echoString $s]
    if {! [string match $s $r]} {
	error "echoString failed simple string test: $s != $r"
    }
    return "echoString"
}

proc soapinterop::validate.echoIntegerArray {} {
    set max [expr int(rand() * 19 + 2)]
    for {set n 0} {$n < $max} {incr n} {
	lappend q [rand_int]
    }
    set r [echoIntegerArray $q]
    if {[llength $r] != [llength $q]} {
	error "echoIntegerArray failed: lists are different: $q != $r"
    }
    for {set n 0} {$n < $max} {incr n} {
	if {[lindex $q $n] != [lindex $r $n]} {
	    error "echoIntegerArray failed: element $n is different: $q != $r"
	}
    }
    return "echoIntegerArray"
}

proc soapinterop::validate.echoFloatArray {} {
    set max [expr int(rand() * 19 + 2)]
    for {set n 0} {$n < $max} {incr n} {
	lappend q [rand_float]
    }
    set r [echoFloatArray $q]
    if {[llength $r] != [llength $q]} {
	error "echoFloatArray failed: lists are different: $q != $r"
    }
    for {set n 0} {$n < $max} {incr n} {
	if {![float=? [lindex $q $n] [lindex $r $n]]} {
	    error "echoFloatArray failed: element $n is different: $q != $r"
	}
    }
    return "echoFloatArray"
}

proc soapinterop::validate.echoStringArray {} {
    set q [array get ::tcl_platform]
    set r [echoStringArray $q]
    if {[llength $r] != [llength $q]} {
	error "echoStringArray failed: lists are different: $q != $r"
    }
    set max [llength $q]
    for {set n 0} {$n < $max} {incr n} {
	if {! [string match [lindex $q $n] [lindex $r $n]]} {
	    error "echoStringArray failed: element $n is different: $q != $r"
	}
    }
    return "echoStringArray"
}

proc soapinterop::validateSOAPStruct {first second} {
    set r 0
    array set f $first
    array set s $second
    foreach key [array names f] {
	set type [rpcvar::rpctype $f($key)]
	switch -- $type {
	    double  { set r [float=? $f($key) $s($key)] }
	    float   { set r [float=? $f($key) $s($key)] }
	    int     { set r [expr $f($key) == $s($key)] }
	    default { 
                if {[string match "varStruct" $key]} {
                    set r [validateSOAPStruct $f($key) $s($key)]
                } else {
                    set r [string match $f($key) $s($key)] 
                }
            }
	}
	if {! $r} {
	    error "failed: mismatching \"$key\" element\
		    $f($key) != $s($key)"
	}
    }
    return $r
}

proc soapinterop::validate.echoStruct {} {
    set q [list \
               varInt    [rand_int] \
               varFloat  [rand_float] \
               varString [rand_string]]
    set r [echoStruct $q]
    if {[catch {validateSOAPStruct $q $r} err]} {
        error "echoStruct $err"
    }
    return "echoStruct"
}

proc soapinterop::validate.echoStructArray {} {

    set max [expr int(rand() * 19 + 2)]
    for {set n 0} {$n < $max} {incr n} {
	lappend q [list \
                       varInt    [rand_int] \
                       varFloat  [rand_float] \
                       varString [rand_string]]
    }
    
    set r [echoStructArray $q]
    for {set n 0} {$n < $max} {incr n} {
	if {[catch {validateSOAPStruct [lindex $q $n] [lindex $r $n]} err]} {
            error "echoStructArray $err"
        }
    }
    return "echoStructArray"
}

# -------------------------------------------------------------------------

#
# Local variables:
#   mode: tcl
#   indent-tabs-mode: nil
# End:
