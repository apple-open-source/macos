# -------------------------------------------------------------------------
# Examples of XML-RPC methods for use with XMLRPC::Domain under the tclhttpd
# web sever.
# -------------------------------------------------------------------------
#

# Load the XMLRPC URL domain handler into the web server and register it under
# the /rpc URL. All methods need to be defined in the zsplat::RPC
# namespace and begin with /. Thus my /base64 procedure will be called 
# via the URL http://localhost:8015/soap/base64
#

package require base64
package require XMLRPC::Domain
package require rpcvar
namespace import -force rpcvar::*

if {[catch {
    XMLRPC::Domain::register -prefix /rpc -namespace tclsoapTest1
} msg]} {
    if { $msg != "URL prefix \"/rpc\" already registered"} {
	puts "Warning: $msg"
    }
}

namespace eval tclsoapTest1 {


    # --------------------------------------------------------------------
    # base64 - convert the input string parameter to a base64 encoded string
    #
    proc /base64 {text} {
	return [rpcvar base64 [base64::encode $text]]
    }
    
    # --------------------------------------------------------------------
    # time - return the servers idea of the time in iso8601 format
    #
    proc /time {} {
	set result [clock format [clock seconds] -format {%Y%m%dT%H:%M:%S}]
	set result [rpcvar dateTime.iso8601 $result]
	return $result
    }

    # --------------------------------------------------------------------
    # rcsid - return the RCS version string for this package
    #
    proc /rcsid {} {
	return "${::XMLRPC::Domain::rcs_id}"
    }

    # --------------------------------------------------------------------
    # square - test validation of numerical methods.
    #
    proc /square {num} {
	if { [catch {expr $num + 0}] } {
	    error "parameter num must be a number"
	}
	return [expr $num * $num]
    }

    # --------------------------------------------------------------------
    # sort - sort a list
    #
    proc /sort {aList} {
	return [rpcvar array [lsort $aList]]
    }

    # --------------------------------------------------------------------
    # struct - generate a XML-RPC struct type
    #
    proc /platform {} {
	return [rpcvar struct ::tcl_platform]
    }

    # --------------------------------------------------------------------
    # Test out a COM calling extension.
    #
    proc /WiRECameras/get_Count {} {
	package require Renicam
	return [renicam count]
    }

    # --------------------------------------------------------------------

    proc /WiRECameras/Add {} {
	package require Renicam
	return [renicam add]
    }

    # ---------------------------------------------------------------------
}

# -------------------------------------------------------------------------
#
# Local variables:
# mode: tcl
# End:
