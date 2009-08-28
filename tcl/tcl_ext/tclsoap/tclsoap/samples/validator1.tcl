# UserLand Validator methods
#  The TclSOAP package provides an implementation of the UserLand 
#  SOAP interoperability methods. This procedure provides the client side
#  for these methods. Either see validator.soapware.org or the notes in each
#  of the implementation files in the TclSOAP/cgi-bin/soap directory.
#

package require SOAP
package require XMLRPC
package require SOAP::http
package require base64

package require rpcvar
namespace import -force rpcvar::*

set help {
    Call 'validator_soap_clients url' or 'validator_xmlrpc_clients url' to
    define the methods for a given endpoint URL.
    
    Call 'validate_soap url' or 'validate_xmlrpc url' to run the test suite
    against an endpoint.
}

# The types for the nested type test...
typedef {
    larry int
    curly int
    moe int
} Stooges

typedef {
    day01 Stooges
    day02 Stooges
    day03 Stooges
} month

typedef {
    month03 month
    month04 month
} year

typedef {
    year2000 year
} myStruct

typedef {
    substruct0 Stooges
    substruct1 Stooges
    substruct2 Stooges
    substruct3 Stooges
    substruct4 Stooges
} StoogeSet

# The XMLRPC nestedStructTest method uses different member names. Bah.
typedef {01 Stooges 02 Stooges 03 Stooges} XMonth
typedef {03 Xmonth 04 Xmonth} XYear
typedef {2000 XYear} XMyStruct

proc validator_soap_clients {{proxy http://localhost/cgi-bin/rpc}} {
    SOAP::create countTheEntities \
	    -proxy $proxy -params {s string}
    SOAP::create easyStructTest \
	    -proxy $proxy -params {stooges struct}
    SOAP::create echoStructTest \
	    -proxy $proxy -params {myStruct StoogeSet}
    SOAP::create manyTypesTest \
	    -proxy $proxy \
            -params {num int bool boolean state int doub double \
	             dat timeInstant bin string}
    SOAP::create moderateSizeArrayCheck \
	    -proxy $proxy -params {myArray array}
    SOAP::create nestedStructTest \
	    -proxy $proxy -params {myStruct myStruct}
    SOAP::create simpleStructReturnTest \
	    -proxy $proxy -params {myNumber int}
    SOAP::create whichToolkit \
	    -uri http://www.soapware.org/ \
	    -proxy $proxy -params {}
}

proc validator_xmlrpc_clients {{proxy http://localhost/cgi-bin/rpc}} {
    XMLRPC::create countTheEntities \
	    -name validator1.countTheEntities \
	    -proxy $proxy -params {s string}
    XMLRPC::create easyStructTest \
	    -name validator1.easyStructTest \
	    -proxy $proxy -params {stooges struct}
    XMLRPC::create echoStructTest \
	    -name validator1.echoStructTest \
	    -proxy $proxy -params {myStruct StoogeSet}
    XMLRPC::create manyTypesTest \
	    -name validator1.manyTypesTest \
	    -proxy $proxy \
            -params {num int bool boolean state int doub double \
	             dat string bin string}
    XMLRPC::create moderateSizeArrayCheck \
	    -name validator1.moderateSizeArrayCheck\
	    -proxy $proxy -params {myArray array}
    XMLRPC::create arrayOfStructsTest \
	    -name validator1.arrayOfStructsTest \
	    -proxy $proxy -params {myArray Stooges()}
    XMLRPC::create nestedStructTest \
	    -name validator1.nestedStructTest \
	    -proxy $proxy -params {myStruct XMyStruct}
    XMLRPC::create simpleStructReturnTest \
	    -name validator1.simpleStructReturnTest \
	    -proxy $proxy -params {myNumber int}
    XMLRPC::create whichToolkit \
	    -name validator1.whichToolkit \
	    -proxy $proxy -params {}
}

# -------------------------------------------------------------------------

proc validate_soap {{proxy http://localhost/cgi-bin/rpc}} {
    validator_soap_clients $proxy
    validate_protocol
    catch {validate.nestedStructTest} msg       ; puts "$msg"
}

proc validate_xmlrpc {{proxy http://localhost/cgi-bin/rpc}} {
    validator_xmlrpc_clients $proxy
    validate_protocol
    catch {validate.arrayOfStructsTest} msg     ; puts "$msg"
    catch {validate.xnestedStructTest} msg      ; puts "$msg"
}

proc validate_protocol {} {
    catch {validate.countTheEntities} msg       ; puts "$msg"
    catch {validate.easyStructTest} msg         ; puts "$msg"
    catch {validate.moderateSizeArrayCheck} msg ; puts "$msg"
    catch {validate.simpleStructReturnTest} msg ; puts "$msg"
    catch {validate.echoStructTest} msg         ; puts "$msg"
    catch {validate.manyTypesTest} msg          ; puts "$msg"
}

# -------------------------------------------------------------------------
# Helper methods used in generating data for the requests.

# Get a random integer between -100 and +100
proc randVal {} {
    return [expr int(rand() * 200 - 100)]
}

# Create a struct list with three named elements and three random int values.
proc stoogeStruct {} {
    return [list larry [randVal] curly [randVal] moe [randVal]]
}

# -------------------------------------------------------------------------

proc validate.countTheEntities {} {
    array set ents [countTheEntities {<""><&><'><}]
    
    if {$ents(ctQuotes) != 2} {
	error "ctQuotes is incorrect" 
    }
    if {$ents(ctLeftAngleBrackets) != 4} {
	error "ctLeftAngleBrackets is incorrent" 
    }
    if {$ents(ctAmpersands) != 1} {
	error "ctAmpersands is incorrect" 
    }
    if {$ents(ctApostrophes) != 1} {
	error "ctApostrophes is incorrent"
    }
    if {$ents(ctRightAngleBrackets) != 3} {
	error "ctRightAngleBrackets is incorrent" 
    }
    return "countTheEntities"
}

proc validate.easyStructTest {} {
    array set stooges [stoogeStruct]
    set check [expr $stooges(larry) + $stooges(curly) + $stooges(moe)]

    set r [easyStructTest [array get stooges]]
    if {$r != $check} { 
	error "easyStructTest failed: $r != $check" 
    }
    return "easyStructTest"
}

proc validate.echoStructTest {} {
    set q [list \
	    substruct0 [stoogeStruct] \
	    substruct1 [stoogeStruct] \
	    substruct2 [stoogeStruct] \
	    substruct3 [stoogeStruct] \
	    substruct4 [stoogeStruct] ]
    set r [echoStructTest $q]
    if {[llength $q] != [llength $r]} {
	error "echoStructTest failed: list lengths differ"
    }
    
    array set aq $q
    array set ar $r

    foreach substruct [array names aq] {
        if {[llength $ar($substruct)] != [llength $aq($substruct)]} {
            error "echoStructTest failed: $substruct lengths differ"
        }
        array set asq $aq($substruct)
        array set asr $ar($substruct)
        foreach stooge [array names asq] {
            if {$asq($stooge) != $asr($stooge)} {
                error "echoStructTest failed: $substruct.$stooge\
                    $asq($stooge) != $asr($stooge)"
            }
	}
    }
    return "echoStructTest"
}

proc validate.manyTypesTest {} {
    set i [randVal]
    set b 1 ;#true
    set st [expr int(rand() * 48) + 1]
    set dbl [expr rand() * 200]
    set secs [clock seconds]
    set date [clock format $secs -format {%Y-%m-%dT%H:%M:%S}]
    set bin [base64::encode [string repeat "HelloWorld!" 24]]
    set r [manyTypesTest $i $b $st $dbl $date $bin]
    
    if {[lindex $r 0] != $i} {error "manyTypesTest failed int"}
    if {[lindex $r 1] && ! $b} {error "failed bool: [lindex $r 1] != $b"}
    if {[lindex $r 2] != $st} {error "manyTypesTest failed state"}
    if {! [expr [lindex $r 3] == $dbl]} {error "manyTypesTest failed double"}
    set rsecs [clock scan [string range \
                               [string map {- {}} [lindex $r 4]] \
                               0 16]]
    if {$rsecs != $secs} {
        error "manyTypesTest failed date: [lindex $r 4] != $date"
    }
    if {[lindex $r 5] != $bin} {error "manyTypesTest failed bin"}
    return "manyTypesTest"
}

proc validate.moderateSizeArrayCheck {} {
    for {set n 12} {$n != 0} {incr n -1} {
	lappend q [randVal]
    }
    set check "[lindex $q 0][lindex $q end]"
    set r [moderateSizeArrayCheck $q]
    if {! [string match $check $r]} {
	error "moderateSizeArrayCheck failed: $check != $r"}
    return "moderateSizeArrayCheck"
}

proc validate.simpleStructReturnTest {} {
    set q [randVal]
    array set r [simpleStructReturnTest $q]
    if {$r(times10) != [expr $q * 10]} {
	error "simpleStructReturnTest: $q * 10 is not $r(times10)"
    }
    if {$r(times100) != [expr $q * 100]} {
	error "simpleStructReturnTest: $q * 100 is not $r(times100)"
    }
    if {$r(times1000) != [expr $q * 1000]} {
	error "simpleStructReturnTest: $q * 1000 is not $r(times1000)"
    }
    return "simpleStructReturnTest"
}

proc validate.arrayOfStructsTest {} {
    set max [expr int(rand() * 10) + 2]
    set check 0
    for {set n 0} {$n < $max} {incr n} {
	array set s [stoogeStruct]
	lappend arr [array get s]
	incr check $s(curly)
    }
    set r [arrayOfStructsTest $arr]
    if {$r != $check} {
	error "arrayOfStructsTest failed: $r != $check"
    }
    return "arrayOfStructTest"
}

proc validate.nestedStructTest {} {

    array set s [stoogeStruct]
    set check [expr $s(larry) + $s(moe) + $s(curly)]
    set q    [list \
	         year2000 [list \
		    month03 [list \
		       day01 [stoogeStruct] \
		       day02 [stoogeStruct] \
		       day03 [stoogeStruct]] \
	            month04 [list \
		       day01 [array get s] \
		       day02 [stoogeStruct] \
		       day03 [stoogeStruct]]]]
    
    set r [nestedStructTest $q]
    if {$r != $check} { error "nestedStructTest failed" }
    return "nestedStructTest"
}

# Pestilentially the XMLRPC validator doesn't use the same member names
# as the SOAP validator for this method.
proc validate.xnestedStructTest {} {

    array set s [stoogeStruct]
    set check [expr $s(larry) + $s(moe) + $s(curly)]
    set q    [list \
	         2000 [list \
	            04 [list \
		       01 [array get s] \
                    ] \
	         ] \
	      ]
    
    set r [nestedStructTest $q]
    if {$r != $check} { error "nestedStructTest failed" }
    return "nestedStructTest"
}

# -------------------------------------------------------------------------
    
if {$tcl_interactive} {
    puts $help
}

