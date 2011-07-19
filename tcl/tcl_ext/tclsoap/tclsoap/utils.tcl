# utils.tcl - Copyright (C) 2001 Pat Thoyts <patthoyts@users.sourceforge.net>
#             Copyright (C) 2008 Andreas Kupries <andreask@activestate.com>
#
# DOM data access utilities for use in the TclSOAP package.
# This is the only place which has to be modified to switch
# between different dom implementations, like TclDOM and tDOM.
#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the accompanying file `LICENSE'
# for more details.
# -------------------------------------------------------------------------

package require tdom

namespace eval ::SOAP {
    namespace eval Utils {
        variable version 1.1
        variable rcsid {$Id: utils.tcl,v 1.10 2008/07/09 16:14:23 andreas_kupries Exp $}
        namespace export getElements getElementsByName \
                getElementValue getElementName \
                getElementValues getElementNames \
                getElementNamedValues \
                getElementAttributes getElementAttribute \
                decomposeSoap decomposeXMLRPC selectNode \
                namespaceURI targetNamespaceURI \
                nodeName baseElementName \
                newDocument deleteDocument \
                parseXML generateXML \
                addNode addTextNode setElementAttribute \
                documentElement getSimpleElementValue
    }
}

# -------------------------------------------------------------------------

proc ::SOAP::Utils::createDocument {name} {
    return [addNode [newDocument] $name]
}

proc ::SOAP::Utils::newDocument {} {
    return [NamespaceSetup [dom createDocumentNode]]
}

proc ::SOAP::Utils::deleteDocument {doc} {
    $doc delete
    return
}

# -------------------------------------------------------------------------

proc ::SOAP::Utils::parseXML {xml} {
    return [NamespaceSetup [dom parse -keepEmpties $xml]]
}

proc ::SOAP::Utils::generateXML {doc} {
    set    xml "<?xml version=\"1.0\"?>\n"
    append xml [$doc asXML -indent 0]
    return $xml
}

# -------------------------------------------------------------------------

proc ::SOAP::Utils::NamespaceSetup {doc} {
    $doc selectNodesNamespaces [list \
         SENC  "http://schemas.xmlsoap.org/soap/encoding/" \
         SENV  "http://schemas.xmlsoap.org/soap/envelope/" \
         xsd   "http://www.w3.org/1999/XMLSchema"          \
         xsi   "http://www.w3.org/1999/XMLSchema-instance" \
    ]
    return $doc
}

# -------------------------------------------------------------------------

proc ::SOAP::Utils::addNode {parent tag} {
   return [$parent appendChild [[$parent ownerDocument] createElement $tag]]
}

proc ::SOAP::Utils::addTextNode {parent value} {
   return [$parent appendChild [[$parent ownerDocument] createTextNode $value]]
}

proc ::SOAP::Utils::setElementAttribute {node name value} {
    $node setAttribute $name $value
    return
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
    return [$node selectNodes $path]
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
    array set Attr [getElementAttributes $domElement]
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
        foreach node [Children $domNode] {
            if {[IsElement $node]} {
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
    set dataNodes [Children $domElement]
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
        append r [NodeValue $dataNode]
    }
    return $r
}

proc ::SOAP::Utils::getSimpleElementValue {domElement} {
    set r {}
    set dataNodes [Children $domElement]
    foreach dataNode $dataNodes {
        append r [NodeValue $dataNode]
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
    array set A [getElementAttributes $node]
    if {[info exists A(href)]} {
        return $A(href)
    }
    return {}
}

# -------------------------------------------------------------------------

proc ::SOAP::Utils::id {node} {
    array set A [getElementAttributes $node]
    if {[info exists A(id)]} {
        return $A(id)
    }
    return {}
}
# -------------------------------------------------------------------------

proc ::SOAP::Utils::getElementName {domElement} {
    return [$domElement nodeName]
}

# -------------------------------------------------------------------------

proc ::SOAP::Utils::getElementAttributes {domElement} {
    set res {}
    foreach item [$domElement attributes] {
        foreach {name prefix ns} $item break
        if {[catch {$domElement getAttributeNS $ns $name} r]} continue
        lappend res $name $r
    }
    return $res
}

# -------------------------------------------------------------------------

# Find a node by id (sort of the xpath id() function)
proc ::SOAP::Utils::getNodeById {base id} {
    if {[string match $id [id $base]]} {
        return $base
    }
    set r {}
    set children [Children $base]
    foreach child $children {
        set r [getNodeById $child $id]
        if {$r != {}} { return $r }
    }
    return {}
}

# -------------------------------------------------------------------------

# Walk up the DOM until you get to the top.
proc ::SOAP::Utils::getDocumentElement {node} {
    while {1} {
        set parent [Parent $node]
        if {$parent == {}} {
            return $node
        }
        set node $parent
    }
}

proc ::SOAP::Utils::documentElement {domNode} {
    return [$domNode documentElement]
}

# -------------------------------------------------------------------------

# Return the value of the specified atribute. First check for an exact match,
# if that fails look for an attribute name without any namespace specification.
# Result:
#  Returns the value of the attribute.
#
proc ::SOAP::Utils::getElementAttribute {node attrname} {
    set r {}
    set attrs [getElementAttributes $node]
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
    if {[catch {
        $node namespaceURI
    } result]} {
        set nodeName [getElementName $node]
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
    set nodeName [$node nodeName]
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
    array set Atts [getElementAttributes $node]

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
    return [find_namespaceURI [Parent $node] $nsname $find_targetNamespace]
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
        foreach node [Children $domNode] {
            if {[IsElement $node]
                && [string match $name [getElementName $node]]} {
                lappend elements $node
            }
        }
    }
    return $elements
}

# -------------------------------------------------------------------------       

proc ::SOAP::Utils::IsElement {domNode} {
    return [string equal [$domNode nodeType] ELEMENT_NODE]
}

proc ::SOAP::Utils::Children {domNode} {
    return [$domNode childNodes]
}

proc ::SOAP::Utils::NodeValue {domNode} {
    return [$domNode nodeValue]
}

proc ::SOAP::Utils::Parent {domNode} {
    return [$domNode nodeParent]
}

# -------------------------------------------------------------------------       
package provide SOAP::Utils $::SOAP::Utils::version

# -------------------------------------------------------------------------
# Local variables:
#    indent-tabs-mode: nil
# End:
