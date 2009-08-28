# tclsoap-Test.tcl - Copyright (C) 2001 Pat Thoyts <Pat.Thoyts@bigfoot.com>
#
# Sample SOAP methods for testing out the TclSOAP package.
#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the accompanying file `LICENSE'
# for more details.
# -------------------------------------------------------------------------
#
# @(#)$Id: tclsoap-Test.tcl,v 1.2 2003/01/26 01:57:33 patthoyts Exp $

package require SOAP
package require XMLRPC
package require rpcvar
namespace import -force rpcvar::*

namespace eval urn:tclsoap:Test {

    SOAP::export time square sum platform printenv printenv_names mistake

    # ---------------------------------------------------------------------
    # Sample SOAP method returning a single string value that is the servers
    # current time in iso8601 point in time format.
    proc time {} {
	set r [rpcvar timeInstant \
		[clock format [clock seconds] -format {%Y%m%dT%H%M%S} \
		-gmt true]]
	return $r
    }

    # ---------------------------------------------------------------------
    # Sample SOAP method taking a single numeric parameter and returning
    # the square of the value.
    proc square {num} {
	if {[catch {expr $num + 0.0} num]} {
	    error "invalid arguments: \"num\" must be a number" {} CLIENT
	}
	return [expr $num * $num]
    }

    # ---------------------------------------------------------------------
    # Sample SOAP method taking a single numeric parameter and returning
    # the sum of two values.
    proc sum {lhs rhs} {
	if {[catch {expr $lhs + $rhs} r]} {
	    error "invalid arguments: both parameters must be numeric" \
		    {} CLIENT
	}
	return $r
    }

    # ---------------------------------------------------------------------
    # Method returning a struct type.
    proc platform {} {
	return [rpcvar struct ::tcl_platform]
    }

    # ---------------------------------------------------------------------
    # Sample SOAP method returning an array of structs. The structs are
    #  struct {
    #      string name;
    #      any    value;
    #  }
    proc printenv {} {
	set r {}
	foreach {name value} [array get ::env] {
	    lappend r [rpcvar "struct" [list "name" $name "value" $value]]
	}
	set result [rpcvar "array" $r]
	return $result
    }
    
    # ---------------------------------------------------------------------
    # just return an array of strings.
    proc printenv_names {} {
	set result [array names ::env]
	set result [rpcvar "array(string)" $result]
	return $result
    }

    # ---------------------------------------------------------------------
    # Sample SOAP method returning an error
    proc mistake {} {
	error "It's a mistake!" {} SERVER
    }

}

# -------------------------------------------------------------------------

# Setup XML-RPC versions of these methods by linking a suitable XML-RPC
# name to the SOAP namespace and exporting the new name to XML-RPC.
#
foreach name {time square sum platform printenv printenv_names mistake} {
    set newname tclsoap.$name
    interp alias {} $newname {} urn:tclsoap:Test::$name
    XMLRPC::export $newname
}

# -------------------------------------------------------------------------

#
# Local variables:
# mode: tcl
# End:
