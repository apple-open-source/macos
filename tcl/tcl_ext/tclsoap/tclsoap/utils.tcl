# utils.tcl - Copyright (C) 2001 Pat Thoyts <patthoyts@users.sourceforge.net>
#
# DOM data access utilities for use in the TclSOAP package.
#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the accompanying file `LICENSE'
# for more details.
# -------------------------------------------------------------------------

namespace eval ::SOAP {
    namespace eval Utils {
        variable version 1.0.1
        variable rcsid {$Id: utils.tcl,v 1.9 2003/09/06 17:08:46 patthoyts Exp $}
        namespace export getElements getElementsByName \
                getElementValue getElementName \
                getElementValues getElementNames \
                getElementNamedValues \
                getElementAttributes getElementAttribute \
                decomposeSoap decomposeXMLRPC selectNode \
                namespaceURI targetNamespaceURI \
                nodeName baseElementName
    }
}

# -------------------------------------------------------------------------

# Description:
#   Provide a version independent selectNode implementation. We either use
#   the version from the dom package or use the SOAP::xpath version if there
#   is no dom one.
# Parameters:
#   node  - reference to a dom tree
#   path  - XPath selection
# Result:
#   Returns the selected node or a list of matching nodes or an empty list
#   if no match.
#
proc ::SOAP::Utils::selectNode {node path} {
    package require SOAP::xpath
    if {[catch {SOAP::xpath::xpath -node $node $path} r]} {
        set r {}
    }
    return $r
}

# -------------------------------------------------------------------------

# for extracting the parameters from a SOAP packet.
# Arrays -> list
# Structs -> list of name/value pairs.
# a methods parameter list comes out looking like a struct where the member
# names == parameter names. This allows us to check the param name if we need
# to.

proc ::SOAP::Utils::is_array {domElement} {
    # Look for "xsi:type"="SOAP-ENC:Array"
    # FIX ME
    # This code should check the namespace using namespaceURI code (CGI)
    #
    set attr [dom::node cget $domElement -attributes]
    upvar #0 attr Attr
    if {[info exists Attr(SOAP-ENC:arrayType)]} {
        return 1
    }
    if {[info exists Attr(xsi:type)]} {
        set type $Attr(xsi:type)
        if {[string match -nocase {*:Array} $type]} {
            return 1
        }
    }

    # If all the child element names are the same, it's an array
    # but of there is only one element???
    set names [getElementNames $domElement]
    if {[llength $names] > 1 && [llength [lsort -unique $names]] == 1} {
        return 1
    }

    return 0
}

# -------------------------------------------------------------------------

# Break down a SOAP packet into a Tcl list of the data.
proc ::SOAP::Utils::decomposeSoap {domElement} {
    set result {}

    # get a list of the child elements of this base element.
    set child_elements [getElements $domElement]

    # if no child element - return the value.
    if {$child_elements == {}} {
	set result [getElementValue $domElement]
    } else {
	# decide if this is an array or struct
	if {[is_array $domElement] == 1} {
	    foreach child $child_elements {
		lappend result [decomposeSoap $child]
	    }
	} else {
	    foreach child $child_elements {
		lappend result [nodeName $child] [decomposeSoap $child]
	    }
	}
    }

    return $result
}

# -------------------------------------------------------------------------

# I expect domElement to be the params element.
proc ::SOAP::Utils::decomposeXMLRPC {domElement} {
    set result {}
    foreach param_elt [getElements $domElement] {
        lappend result [getXMLRPCValue [getElements $param_elt]]
    }
    return $result
}

# -------------------------------------------------------------------------

proc ::SOAP::Utils::getXMLRPCValue {value_elt} {
    set value {}
    if {$value_elt == {}} { return $value }

    # if there is not type element then the specs say it's a string type.
    set type_elt [getElements $value_elt]
    if {$type_elt == {}} {
        return [getElementValue $value_elt]
    }

    set type [getElementName $type_elt]
    if {[string match "struct" $type]} {
        foreach member_elt [getElements $type_elt] {
            foreach elt [getElements $member_elt] {
                set eltname [getElementName $elt]
                if {[string match "name" $eltname]} {
                    set m_name [getElementValue $elt]
                } elseif {[string match "value" $eltname]} {
                    set m_value [getXMLRPCValue $elt]
                }
            }
            lappend value $m_name $m_value
        }
    } elseif {[string match "array" $type]} {
        foreach elt [getElements [lindex [getElements $type_elt] 0]] {
            lappend value [getXMLRPCValue $elt]
        }
    } else {
        set value [getElementValue $type_elt]
    }
    return $value
}

# -------------------------------------------------------------------------

# Description:
#   Return a list of all the immediate children of domNode that are element
#   nodes.
# Parameters:
#   domNode  - a reference to a node in a dom tree
#
proc ::SOAP::Utils::getElements {domNode} {
    set elements {}
    if {$domNode != {}} {
        foreach node [dom::node children $domNode] {
            if {[dom::node cget $node -nodeType] == "element"} {
                lappend elements $node
            }
        }
    }
    return $elements
}

# -------------------------------------------------------------------------

# Description:
#   If there are child elements then recursively call this procedure on each
#   child element. If this is a leaf element, then get the element value data.
# Parameters:
#   domElement - a reference to a dom element node
# Result:
#   Returns a value or a list of values.
#
proc ::SOAP::Utils::getElementValues {domElement} {
    set result {}
    if {$domElement != {}} {
        set nodes [getElements $domElement]
        if {$nodes =={}} {
            set result [getElementValue $domElement]
        } else {
            foreach node $nodes {
                lappend result [getElementValues $node]
            }
        }
    }
    return $result
}

# -------------------------------------------------------------------------

proc ::SOAP::Utils::getElementValuesList {domElement} {
    set result {}
    if {$domElement != {}} {
        set nodes [getElements $domElement]
        if {$nodes =={}} {
            set result [getElementValue $domElement]
        } else {
            foreach node $nodes {
                lappend result [getElementValues $node]
            }
        }
    }
    return $result
}

# -------------------------------------------------------------------------

proc ::SOAP::Utils::getElementNames {domElement} {
    set result {}
    if {$domElement != {}} {
        set nodes [getElements $domElement]
        if {$nodes == {}} {
            set result [getElementName $domElement]
        } else {
            foreach node $nodes {
                lappend result [getElementName $node]
            }
        }
    }
    return $result
}

# -------------------------------------------------------------------------

proc ::SOAP::Utils::getElementNamedValues {domElement} {
    set name [getElementName $domElement]
    set value {}
    set nodes [getElements $domElement]
    if {$nodes == {}} {
	set value [getElementValue $domElement]
    } else {
	foreach node $nodes {
	    lappend value [getElementNamedValues $node]
	}
    }
    return [list $name $value]
}

# -------------------------------------------------------------------------

# Description:
#   Merge together all the child node values under a given dom element
#   This procedure will also cope with elements whose data is elsewhere
#   using the href attribute. We currently expect the data to be a local
#   reference.
# Params:
#   domElement  - a reference to an element node in a dom tree
# Result:
#   A string containing the elements value
#
proc ::SOAP::Utils::getElementValue {domElement} {
    set r {}
    set dataNodes [dom::node children $domElement]
    if {[set href [href $domElement]] != {}} {
        if {[string match "\#*" $href]} {
            set href [string trimleft $href "\#"]
        } else {
            return -code error "cannot follow non-local href"
        }
        set r [[uplevel proc:name] [getNodeById \
                [getDocumentElement $domElement] $href]]
    }
    foreach dataNode $dataNodes {
        append r [dom::node cget $dataNode -nodeValue]
    }
    return $r
}

# -------------------------------------------------------------------------

# Description:
#   Get the name of the current proc
#   - from http://purl.org/thecliff/tcl/wiki/526.html
proc ::SOAP::Utils::proc:name {} {
    lindex [info level -1] 0
} 

# -------------------------------------------------------------------------

proc ::SOAP::Utils::href {node} {
    set a [dom::node cget $node -attributes]
    upvar #0 $a A
    if {[info exists A(href)]} {
        return $A(href)
    }
    return {}
}

# -------------------------------------------------------------------------

proc ::SOAP::Utils::id {node} {
    set a [dom::node cget $node -attributes]
    upvar #0 $a A
    if {[info exists A(id)]} {
        return $A(id)
    }
    return {}
}
# -------------------------------------------------------------------------

proc ::SOAP::Utils::getElementName {domElement} {
    return [dom::node cget $domElement -nodeName]
}

# -------------------------------------------------------------------------

proc ::SOAP::Utils::getElementAttributes {domElement} {
    set attr [dom::node cget $domElement -attributes]
    set attrlist [array get $attr]
    return $attrlist
}

# -------------------------------------------------------------------------

# Find a node by id (sort of the xpath id() function)
proc ::SOAP::Utils::getNodeById {base id} {
    if {[string match $id [id $base]]} {
        return $base
    }
    set r {}
    set children [dom::node children $base]
    foreach child $children {
        set r [getNodeById $child $id]
        if {$r != {}} { return $r }
    }
    return {}
}

# -------------------------------------------------------------------------

# Walk up the DOM until you get to the top.
proc ::SOAP::Utils::getDocumentElement {node} {
    set parent [dom::node parent $node]
    if {$parent == {}} {
        return $node
    } else {
        return [getDocumentElement $parent]
    }
}

# -------------------------------------------------------------------------

# Return the value of the specified atribute. First check for an exact match,
# if that fails look for an attribute name without any namespace specification.
# Result:
#  Returns the value of the attribute.
#
proc ::SOAP::Utils::getElementAttribute {node attrname} {
    set r {}
    set attrs [array get [dom::node cget $node -attributes]]
    if {[set ndx [lsearch -exact $attrs $attrname]] == -1} {
        set ndx [lsearch -regexp $attrs ":${attrname}\$"]
    }

    if {$ndx != -1} {
        incr ndx
        set r [lindex $attrs $ndx]
    }
    return $r
}

# -------------------------------------------------------------------------

# Description:
#  Get the namespace of the given node. This code will examine the nodes 
#  attributes and if necessary the parent nodes attributes until it finds
#  a relevant namespace declaration.
# Parameters:
#  node - the node for which to return a namespace
# Result:
#  returns either the namespace uri or an empty string.
# Notes:
#  The TclDOM 2.0 package provides a -namespaceURI option. The C code module
#  does not, so we have the second chunk of code.
#  The hasFeature method doesn't seem to provide information about this
#  but the versions that support 'query' seem to have the namespaceURI
#  method so we'll use this test for now.
#
proc ::SOAP::Utils::namespaceURI {node} {
    #if {[dom::DOMImplementation hasFeature query 1.0]} {
    #    return [dom::node cget $node -namespaceURI]
    #} 
    if {[catch {dom::node cget $node -namespaceURI} result]} {
        set nodeName [dom::node cget $node -nodeName]
        set ndx [string last : $nodeName]
        set nodeNS [string range $nodeName 0 $ndx]
        set nodeNS [string trimright $nodeNS :]
        
        set result [find_namespaceURI $node $nodeNS]
    }
    return $result
}

# Description:
#  As for namespaceURI except that we are interested in the targetNamespace
#  URI. This is commonly used in XML schemas to specify the default namespace
#  for the defined items.
#
proc ::SOAP::Utils::targetNamespaceURI {node value} {
    set ndx [string last : $value]
    set ns [string trimright [string range $value 0 $ndx] :]
    #set base [string trimleft [string range $value $ndx end] :]
    return [find_namespaceURI $node $ns 1]
}

# -------------------------------------------------------------------------

# Description:
#   Obtain the unqualified part of a node name.
# Parameters:
#   node - a DOM node
# Result:
#   the node name without any namespace prefix.
#
proc ::SOAP::Utils::nodeName {node} {
    set nodeName [dom::node cget $node -nodeName]
    set nodeName [string range $nodeName [string last : $nodeName] end]
    return [string trimleft $nodeName :]
}

proc ::SOAP::Utils::baseElementName {nodeName} {
    set nodeName [string range $nodeName [string last : $nodeName] end]
    return [string trimleft $nodeName :]
}
# -------------------------------------------------------------------------

# Description:
#   Obtain the uri for the nsname namespace name working up the DOM tree
#   from the given node.
# Parameters:
#   node - the starting point in the tree.
#   nsname - the namespace name. May be an null string.
# Result:
#   Returns the namespace uri or an empty string.
#
proc ::SOAP::Utils::find_namespaceURI {node nsname {find_targetNamespace 0}} {
    if {$node == {}} { return {} }
    set atts [dom::node cget $node -attributes]
    upvar #0 atts Atts

    # check for the default namespace or targetNamespace
    if {$nsname == {}} {
        if {$find_targetNamespace} {
            if {[info exists Atts(targetNamespace)]} {
                return $Atts(targetNamespace)
            }
        } else {
            if {[info exists Atts(xmlns)]} {
                return $Atts(xmlns)
            }
        }
    } else {
    
        # check the defined namespace names.
        foreach {attname attvalue} [array get $atts] {
            if {[string match "xmlns:$nsname" $attname]} {
                return $attvalue
            }
        }

    }
    
    # recurse through the parents.
    return [find_namespaceURI [dom::node parent $node] $nsname $find_targetNamespace]
}

# -------------------------------------------------------------------------

# Description:
#   Return a list of all the immediate children of domNode that are element
#   nodes.
# Parameters:
#   domNode  - a reference to a node in a dom tree
#
proc ::SOAP::Utils::getElementsByName {domNode name} {
    set elements {}
    if {$domNode != {}} {
        foreach node [dom::node children $domNode] {
            if {[dom::node cget $node -nodeType] == "element"
                && [string match $name [dom::node cget $node -nodeName]]} {
                lappend elements $node
            }
        }
    }
    return $elements
}

# -------------------------------------------------------------------------       
package provide SOAP::Utils $::SOAP::Utils::version

# -------------------------------------------------------------------------
# Local variables:
#    indent-tabs-mode: nil
# End:
