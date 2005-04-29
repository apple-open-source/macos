# validator.tcl - Copyright (C) 2001 Pat Thoyts <Pat.Thoyts@bigfoot.com>
#
# Implement the http://validator.soapware.org/ interoperability suite and
# the http://validator1.xmlrpc.org/ XML-RPC interop suite.
#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the accompanying file `LICENSE'
# for more details.
# -------------------------------------------------------------------------
#
# @(#)$Id: validator.tcl,v 1.3 2001/08/07 12:07:06 patthoyts Exp $

package require SOAP
package require XMLRPC
package require rpcvar
namespace import -force rpcvar::*

# -------------------------------------------------------------------------

# Export the SOAP webservices

SOAP::export whichToolkit countTheEntities easyStructTest echoStructTest \
	manyTypesTest moderateSizeArrayCheck simpleStructReturnTest \
	nestedStructTest

# Export the XML-RPC webservices.

XMLRPC::export validator1.whichToolkit validator1.countTheEntities \
	validator1.easyStructTest validator1.echoStructTest \
	validator1.manyTypesTest validator1.moderateSizeArrayCheck \
	validator1.simpleStructReturnTest validator1.nestedStructTest \
	validator1.arrayOfStructsTest

# -------------------------------------------------------------------------

# Optional feature used by the validator at http://validator.soapware.org/
# Helps them to work out what SOAP toolkit is providing the service.
#
proc validator1.whichToolkit {} {
    if {[catch {package require SOAP} soapVersion]} {
	set soapVersion {unknown}
    }
    set r(toolkitDocsUrl)         "http://tclsoap.sourceforge.net/"
    set r(toolkitName)            "TclSOAP"
    set r(toolkitVersion)         $soapVersion
    set r(toolkitOperatingSystem) "System Independent"
    return [rpcvar struct r]
}

# -------------------------------------------------------------------------

# validator1.countTheEntities (s) returns struct
#
# This handler takes a single parameter named s, a string, that
# contains any number of predefined entities, namely <, >, &, ' and ".
#
# Your handler must return a struct that contains five fields, all
# numbers: ctLeftAngleBrackets, ctRightAngleBrackets, ctAmpersands,
# ctApostrophes, ctQuotes.
#
# To validate, the numbers must be correct.
#
proc validator1.countTheEntities {s} {
    array set a {< 0 > 0 & 0 ' 0 \" 0}
    foreach c [split $s {}] {
	if {[catch {incr a($c)}]} {
	    set a($c) 1
	}
    }
    set r(ctLeftAngleBrackets) $a(<)
    set r(ctRightAngleBrackets) $a(>)
    set r(ctAmpersands) $a(&)
    set r(ctApostrophes) $a(\')
    set r(ctQuotes) $a(\")
    return [rpcvar struct r]
}

# -------------------------------------------------------------------------

# validator1.easyStructTest (stooges) returns number
#
# This handler takes a single parameter named stooges, a struct,
# containing at least three elements named moe, larry and curly, all
# ints. Your handler must add the three numbers and return the result.    
#
proc validator1.easyStructTest {stooges} {
    array set stooge $stooges
    set r [expr $stooge(larry) + $stooge(curly) + $stooge(moe)]
    return $r
}

# -------------------------------------------------------------------------

# validator1.echoStructTest (myStruct) returns struct
#
# This handler takes a single parameter named myStruct, a struct. Your
# handler must return the struct.
#
# This is a struct of structs (actually an array but with different names
# for each item).
#
proc validator1.echoStructTest {myStruct} {
    set r {}
    foreach {name value} $myStruct {
	lappend r $name [rpcvar struct $value]
    }
	
    return [rpcvar struct $r]
}

# -------------------------------------------------------------------------

# validator1.manyTypesTest (num, bool, state, doub, dat, bin) returns array
#
# This handler takes six parameters and returns an array containing
# all the parameters.
#
proc validator1.manyTypesTest {num bool state doub dat bin} {
    set r {}
    if {$bool} {set bool true} else {set bool false}
    set dat [rpcvar "dateTime.iso8601" $dat]
    set bin [rpcvar "base64" $bin]
    lappend r $num $bool $state $doub $dat $bin
    return [rpcvar array $r]
}

# I need to do better handling of the type mismatching between SOAP and
# XML-RPC, until then...
proc soapvalidator.manyTypesTest {num bool state doub dat bin} {
    set r {}
    if {$bool} {set bool true} else {set bool false}
    set dat [rpcvar "timeInstant" $dat]
    lappend r $num $bool $state $doub $dat $bin
    return [rpcvar array $r]
}

# -------------------------------------------------------------------------

# validator1.moderateSizeArrayCheck (myArray) returns string
#
# This handler takes a single parameter named myArray, which is an
# array containing between 100 and 200 elements. Each of the items is
# a string, your handler must return a string containing the
# concatenated text of the first and last elements.

proc validator1.moderateSizeArrayCheck {myArray} {
    return "[lindex $myArray 0][lindex $myArray end]"
}

# -------------------------------------------------------------------------

# validator1.simpleStructReturnTest (myNumber) returns struct
#
# This handler takes one parameter a number named myNumber, and returns
# a struct containing three elements, times10, times100 and times1000,
# the result of multiplying the number by 10, 100 and 1000
#
proc validator1.simpleStructReturnTest {myNumber} {
    set r(times10) [expr $myNumber * 10]
    set r(times100) [expr $myNumber * 100]
    set r(times1000) [expr $myNumber * 1000]
    return [rpcvar struct r]
}

# -------------------------------------------------------------------------

# validator1.arrayOfStructsTest (array) returns number

# This handler takes a single parameter, an array of structs, each of
# which contains at least three elemenets names noe, larry and curly,
# all <i4>'s. Your handler must add all the struct elements named curly
# and return the result.
#
proc validator1.arrayOfStructsTest {myArray} {
    set r 0
    foreach itemdata $myArray {
	array set item $itemdata
	incr r $item(curly)
    }
    return $r
}

# -------------------------------------------------------------------------

# validator1.nestedStructTest (myStruct) returns number
#
# This handler takes a single parameter named myStruct, a struct, that
# models a daily calendar. At the top level, there is one struct for
# each year. Each year is broken down into months, and months into
# days. Most of the days are empty in the struct you receive, but the
# entry for April 1, 2000 contains a least three elements named moe,
# larry and curly, all <i4>s. Your handler must add the three numbers
# and return the result.
# NB: month and day are two-digits with leading 0s, and January is 01
#
# First, the XML-RPC implementation.
proc validator1.nestedStructTest {myStruct} {
    array set ms $myStruct
    array set y2k $ms(2000)
    array set m4 $y2k(04)
    array set d1 $m4(01)
    return [expr $d1(larry) + $d1(moe) + $d1(curly)]
}
# The SOAP implementation receives different member names.
proc soapvalidator.nestedStructTest {myStruct} {
    array set ms $myStruct
    array set y2k $ms(year2000)
    array set m4 $y2k(month04)
    array set d1 $m4(day01)
    return [expr $d1(larry) + $d1(moe) + $d1(curly)]
}

# -------------------------------------------------------------------------

# Description:
#   Given the nested structure provided for the nestedStructTest, 
#   echo the struct back to the caller.
# Notes:
#   This is not one of the required tests, but writing this exposed some
#   issues in handling nested structures within the TclSOAP framework. It
#   works now. However, this implementation will not ensure that the structure
#   members are returned in the same order that they were provided.
#
proc validator1.echoNestedStructTest {myStruct} {
    global years
    array set years {}
    foreach {name value} $myStruct {
	set years($name) [year $value]
    }
    return [rpcvar struct years]
}

proc year {yearValue} {
    array set months {}
    foreach {name value} $yearValue {
	set months($name) [month $value]
    }
    return [rpcvar struct months]
}

proc month {monthValue} {
    array set days {}
    foreach {name value} $monthValue {
	set days($name) [day $value]
    }
    return [rpcvar struct days]
}

proc day {dayValue} {
    array set stooges {}
    foreach {name value} $dayValue {
	set stooges($name) $value
    }
    return [rpcvar struct stooges]
}

# -------------------------------------------------------------------------

# Link the XMLRPC names to global names suitable for use with SOAP.
#
# XMLRPC expects to see the method names as defined in this file, but SOAP
# expects methods to be in an XML namespace. For the validator test suite 
# here, that namespace is global, thus:
#
foreach procname [info proc validator1.*] {
    set soapname [lindex [split $procname .] end]
    if {[string match "nestedStructTest" $soapname]} {
	set procname soapvalidator.nestedStructTest ;# redirect for SOAP
    }
    if {[string match "manyTypesTest" $soapname]} {
	set procname soapvalidator.manyTypesTest ;# redirect for SOAP
    }
    interp alias {} $soapname {} $procname
}

# The whichToolkit method is called from http://www.soapware.org/ namespace
# So expose it.
namespace eval http://www.soapware.org/ {
    SOAP::export whichToolkit
    interp alias {} whichToolkit {} ::validator1.whichToolkit
}

# -------------------------------------------------------------------------

#
# Local variables:
# mode: tcl
# End:
