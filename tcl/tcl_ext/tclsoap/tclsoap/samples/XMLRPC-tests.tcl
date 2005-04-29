# XMLRPC-tests.tcl - Copyright (C) 2001 Pat Thoyts <Pat.Thoyts@bigfoot.com>
#
# Create some remote XML-RPC access methods to demo servers.
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
# @(#)$Id: XMLRPC-tests.tcl,v 1.4 2001/12/08 01:19:02 patthoyts Exp $

package require XMLRPC
package require SOAP::http

set methods {}

# -------------------------------------------------------------------------

# Some of UserLands XML RPC examples.

lappend methods [ \
    XMLRPC::create getStateName \
        -name "examples.getStateName" \
        -proxy "http://betty.userland.com/RPC2" \
        -params { state i4 } ]

lappend methods [ \
    XMLRPC::create getStateList \
	-name "examples.getStateList" \
	-proxy "http://betty.userland.com/RPC2" \
	-params { states array(int) } ]

# Meerkat
namespace eval Meerkat {
    set proxy {http://www.oreillynet.com/meerkat/xml-rpc/server.php}

    # returns array of meerkat method names
    lappend methods [ \
         XMLRPC::create listMethods \
            -name "system.listMethods" \
            -proxy $proxy \
            -params {} ]

    # returns an array of category structs: {int id; string title;}
    lappend methods [ \
         XMLRPC::create getCategories \
            -name "meerkat.getCategories" \
            -proxy $proxy \
            -params {} ]

    # returns an array of channel structs: { int id; string title; }
    lappend methods [ \
         XMLRPC::create getChannels \
            -name "meerkat.getChannels" \
            -proxy $proxy \
            -params {} ]

    # returns array of channel structs: {int id; string title;} given
    # a category id.
    lappend methods [ \
         XMLRPC::create getChannelsByCategory \
            -name "meerkat.getChannelsByCategory" \
            -proxy $proxy \
            -params {id int} ]

    # requires a recipe struct and returns an array of structs:
    # {string title; string link; string description; string dc_creator;
    #  string dc_subject; string dc_publisher; string dc_date;
    #  string dc_format; string dc_language; string dc_rights 
    # }
    # try getItems {search {/[Tt]cl/} num_items 5 descriptions 0}
    #
    # try: foreach item [Meerkat::getItems 2081] {
    #          array set d $item
    #          puts "$d(title)\n$d(link)\n$d(description)\n"
    #      }
    
    lappend methods [ \
         XMLRPC::create getItems \
            -name "meerkat.getItems" \
            -proxy $proxy \
            -params { item struct } ]

}

# Userland RSS server
lappend methods [ \
        XMLRPC::create getServiceInfo \
           -name "aggregator.getServiceInfo" \
           -proxy "http://aggregator.userland.com/RPC2" \
           -params { id int } ]

# -------------------------------------------------------------------------

set methods

# -------------------------------------------------------------------------

# Local variables:
#    indent-tabs-mode: nil
# End:
