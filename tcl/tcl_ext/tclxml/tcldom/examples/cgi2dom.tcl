# cgi2dom.tcl --
#
#	Turns CGI parameters into a DOM document
#
# Copyright (c) 2000-2002 Zveno Pty Ltd
#
# $Id: cgi2dom.tcl,v 1.3 2002/12/10 05:17:41 balls Exp $

package require dom 2.5
package require xpath

package provide cgi2dom 1.1

namespace eval cgi2dom {
    namespace export createdocument
}

# cgi2dom::createdocument --
#
#	Construct a DOM document from XPath locations paths.
#
# Arguments:
#	specs	List of XPath location path specifications
#		given as location-path/cdata pairs
#
# Results:
#	Returns token for new DOM document

proc cgi2dom::createdocument specs {
    set doc [dom::DOMImplementation create]

    foreach {path value} $specs {
	if {![string match /* $path]} continue

	set node [dom::DOMImplementation createNode $doc $path]
	if {[string length $value]} {
	    switch [dom::node cget $node -nodeType] {
		element {
		    dom::document createTextNode $node $value
		}
		textNode {
		    dom::node configure $node -nodeValue $value
		}
		default {}
	    }
	}
    }

    return $doc
}



