# Async.tcl - Copyright (C) 2001 Pat Thoyts <Pat.Thoyts@bigfoot.com>
#
# Example of a method using asynchronous HTTP tranfers for SOAP/XMLRPC
#
# Usage:
#   1:  source this file
#   2:  optionally configure for your http proxy
#   3:  call Meerkat::getItems {search {/[Tt]cl/}
#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the accompanying file `LICENSE'
# for more details.
# -------------------------------------------------------------------------
#
# $Id: Async.tcl,v 1.2 2001/12/08 01:19:02 patthoyts Exp $

package require XMLRPC
package require Tkhtml
package require SOAP::http

# -------------------------------------------------------------------------
# Meerkat service
# -------------------------------------------------------------------------
namespace eval Meerkat {
    variable proxy {http://www.oreillynet.com/meerkat/xml-rpc/server.php}

    # ---------------------------------------------------------------------
    # Construct a user interface.
    # ---------------------------------------------------------------------
    set dlg [toplevel .t]
    wm title $dlg {Asynchronous Test}
    set mainFrame [frame ${dlg}.f1]
    set viewer [html ${mainFrame}.h -bg white \
	    -yscrollcommand "${mainFrame}.s set"]
    set scroll [scrollbar ${mainFrame}.s -command "$viewer yview"]
    pack $scroll -side right -fill y
    pack $viewer -side left -fill both -expand 1
    pack $mainFrame -side top -fill both -expand 1

    # ---------------------------------------------------------------------
    # The Asynchronous handlers
    # ---------------------------------------------------------------------
    proc gotCategories {w data} {
	set html "<ul>\n"
	foreach struct $data {
	    array set item $struct
	    append html "<li>$item(id) $item(title)</li>\n"
	}
	append html "</ul>\n"
	$w clear
	$w parse $html
    }
    
    proc gotItems {w data} {
	set html {}
	foreach entry $data {
	    array set item $entry
	    append html "<h2><a href=\"$item(link)\">$item(title)</a></h2>\n"
	    append html "<p>$item(description)\n"
	    append html "<br><a href=\"$item(link)\">$item(link)</a>\n"
	    append html "<br><font size=\"-1\">[array names item]</font></p>\n"
	}
	$w clear
	$w parse $html
    }

    # ---------------------------------------------------------------------
    # Configure RPC commands
    # ---------------------------------------------------------------------

    # returns an array of category structs: {int id; string title;}
    XMLRPC::create getCategories \
	-name "meerkat.getCategories" \
	-proxy $proxy \
	-command "[namespace current]::gotCategories $viewer" \
	-params {}
    
    XMLRPC::create getItems \
	-name "meerkat.getItems" \
	-proxy $proxy \
	-command "[namespace current]::gotItems $viewer" \
	-params { item struct }
    
}

# -------------------------------------------------------------------------

# Configure for our proxy
#XMLRPC::proxyconfig

# Set this running
#Meerkat::getItems {search /[Tt]cl/}

#
# Local variables:
# mode: tcl
# End:
