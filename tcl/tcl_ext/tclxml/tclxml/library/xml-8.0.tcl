# xml-8.0.tcl --
#
#	This file provides generic XML services for all implementations.
#	This file supports Tcl 8.0 regular expressions.
#
#	See xmlparse.tcl for the Tcl implementation of a XML parser.
#
# Copyright (c) 1998-2002 Zveno Pty Ltd
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
# $Id: xml-8.0.tcl,v 1.4 2002/08/30 07:52:16 balls Exp $

package require -exact Tcl 8.0

package require sgml 1.8

package provide xmldefs 1.10

namespace eval xml {

    # Convenience routine
    proc cl x {
	return "\[$x\]"
    }

    # Define various regular expressions

    # Characters
    variable Char $::sgml::Char

    # white space
    variable Wsp " \t\r\n"
    variable noWsp [cl ^$Wsp]

    # Various XML names and tokens

    variable NameChar $::sgml::NameChar
    variable Name $::sgml::Name
    variable Names $::sgml::Names
    variable Nmtoken $::sgml::Nmtoken
    variable Nmtokens $::sgml::Nmtokens

    # The definition of the Namespace URI for XML Namespaces themselves.
    # The prefix 'xml' is automatically bound to this URI.
    variable xmlnsNS http://www.w3.org/XML/1998/namespace

    # Tokenising expressions

    variable tokExpr <(/?)([cl ^$Wsp>/]+)([cl $Wsp]*[cl ^>]*)>
    variable substExpr "\}\n{\\2} {\\1} {\\3} \{"

    # table of predefined entities

    variable EntityPredef
    array set EntityPredef {
	lt <   gt >   amp &   quot \"   apos '
    }

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

