# xml.tcl --
#
#	This file provides generic XML services for all implementations.
#	This file supports Tcl 8.1 regular expressions.
#
#	See tclparser.tcl for the Tcl implementation of a XML parser.
#
# Copyright (c) 1998-2003 Zveno Pty Ltd
# http://www.zveno.com/
# 
# Zveno makes this software and all associated data and documentation
# ('Software') available free of charge for any purpose.
# Copies may be made of this Software but all of this notice must be included
# on any copy.
# 
# The Software was developed for research purposes and Zveno does not warrant
# that it is error free or fit for any purpose.  Zveno disclaims any
# liability for all claims, expenses, losses, damages and costs any user may
# incur as a result of using, copying or modifying the Software.
#
# Copyright (c) 1997 Australian National University (ANU).
# 
# ANU makes this software and all associated data and documentation
# ('Software') available free of charge for any purpose. You may make copies
# of the Software but you must include all of this notice on any copy.
# 
# The Software was developed for research purposes and ANU does not warrant
# that it is error free or fit for any purpose.  ANU disclaims any
# liability for all claims, expenses, losses, damages and costs any user may
# incur as a result of using, copying or modifying the Software.
#
# $Id: xml-8.1.tcl,v 1.13 2003/02/25 04:09:22 balls Exp $

package require Tcl 8.1

package provide xmldefs 2.6

package require sgml 1.8

namespace eval xml {

    namespace export qnamesplit

    # Convenience routine
    proc cl x {
	return "\[$x\]"
    }

    # Define various regular expressions

    # Characters
    variable Char $::sgml::Char

    # white space
    variable Wsp " \t\r\n"
    variable allWsp [cl $Wsp]*
    variable noWsp [cl ^$Wsp]

    # Various XML names and tokens

    variable NameChar $::sgml::NameChar
    variable Name $::sgml::Name
    variable Names $::sgml::Names
    variable Nmtoken $::sgml::Nmtoken
    variable Nmtokens $::sgml::Nmtokens

    # XML Namespaces names

    # NCName ::= Name - ':'
    variable NCName $::sgml::Name
    regsub -all : $NCName {} NCName
    variable QName (${NCName}:)?$NCName		;# (Prefix ':')? LocalPart

    # The definition of the Namespace URI for XML Namespaces themselves.
    # The prefix 'xml' is automatically bound to this URI.
    variable xmlnsNS http://www.w3.org/XML/1998/namespace

    # table of predefined entities

    variable EntityPredef
    array set EntityPredef {
	lt <   gt >   amp &   quot \"   apos '
    }

    # Expressions for pulling things apart
    variable tokExpr <(/?)([::xml::cl ^$::xml::Wsp>/]+)([::xml::cl $::xml::Wsp]*[::xml::cl ^>]*)>
    variable substExpr "\}\n{\\2} {\\1} {\\3} \{"

}

###
###	Exported procedures
###

# xml::qnamesplit --
#
#	Split a QName into its constituent parts:
#	the XML Namespace prefix and the Local-name
#
# Arguments:
#	qname	XML Qualified Name (see XML Namespaces [6])
#
# Results:
#	Returns prefix and local-name as a Tcl list.
#	Error condition returned if the prefix or local-name
#	are not valid NCNames (XML Name)

proc xml::qnamesplit qname {
    variable NCName
    variable Name

    set prefix {}
    set localname $qname
    if {[regexp : $qname]} {
	if {![regexp ^($NCName)?:($NCName)\$ $qname discard prefix localname]} {
	    return -code error "name \"$qname\" is not a valid QName"
	}
    } elseif {![regexp ^$Name\$ $qname]} {
	return -code error "name \"$qname\" is not a valid Name"
    }

    return [list $prefix $localname]
}

###
###	General utility procedures
###

# xml::noop --
#
# A do-nothing proc

proc xml::noop args {}

### Following procedures are based on html_library

# xml::zapWhite --
#
#	Convert multiple white space into a single space.
#
# Arguments:
#	data	plain text
#
# Results:
#	As above

proc xml::zapWhite data {
    regsub -all "\[ \t\r\n\]+" $data { } data
    return $data
}

