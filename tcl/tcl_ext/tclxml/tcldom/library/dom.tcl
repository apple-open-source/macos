# dom.tcl --
#
#	This file sets up the generic API for TclDOM.
#	It is used when the Tcl-only version of TclDOM
#	is loaded.
#
#	The actual pure-Tcl DOM implementation has moved
#	to domimpl.tcl
#
# Copyright (c) 2002-2003 Zveno Pty Ltd
# http://www.zveno.com/
#
# Zveno makes this software available free of charge for any purpose.
# Copies may be made of this software but all of this notice must be included
# on any copy.
#
# The software was developed for research purposes only and Zveno does not
# warrant that it is error free or fit for any purpose.  Zveno disclaims any
# liability for all claims, expenses, losses, damages and costs any user may
# incur as a result of using, copying or modifying this software.
#
# $Id: dom.tcl,v 1.19 2003/03/09 11:12:49 balls Exp $

package provide dom::tclgeneric 2.6

namespace eval dom {
    namespace export DOMImplementation
    namespace export document documentFragment node
    namespace export element textNode attribute
    namespace export processingInstruction
    namespace export event

    variable maxSpecials
    if {![info exists maxSpecials]} {
	set maxSpecials 10
    }

    variable strictDOM 0

    # Default -indentspec value
    #	spaces-per-indent-level {collapse-re collapse-value}
    variable indentspec [list 2 [list {        } \t]]

    # The Namespace URI for XML Namespace declarations
    variable xmlnsURI http://www.w3.org/2000/xmlns/

}

package require dom::tcl 2.6

foreach p {DOMImplementation hasFeature createDocument create createDocumentType createNode destroy isNode parse selectNode serialize trim document documentFragment node element textNode attribute processingInstruction event} {

    proc dom::$p args "return \[eval tcl::$p \$args\]"

}

