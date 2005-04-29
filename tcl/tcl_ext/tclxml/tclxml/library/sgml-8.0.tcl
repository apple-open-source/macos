# sgml-8.0.tcl --
#
#	This file provides generic parsing services for SGML-based
#	languages, namely HTML and XML.
#	This file supports Tcl 8.0 characters and regular expressions.
#
#	NB.  It is a misnomer.  There is no support for parsing
#	arbitrary SGML as such.
#
# Copyright (c) 1998,1999 Zveno Pty Ltd
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
# Copyright (c) 1997 ANU and CSIRO on behalf of the
# participants in the CRC for Advanced Computational Systems ('ACSys').
# 
# ACSys makes this software and all associated data and documentation 
# ('Software') available free of charge for any purpose.  You may make copies 
# of the Software but you must include all of this notice on any copy.
# 
# The Software was developed for research purposes and ACSys does not warrant
# that it is error free or fit for any purpose.  ACSys disclaims any
# liability for all claims, expenses, losses, damages and costs any user may
# incur as a result of using, copying or modifying the Software.
#
# $Id: sgml-8.0.tcl,v 1.3 2002/08/30 07:52:16 balls Exp $

package require -exact Tcl 8.0

package provide sgml 1.9

namespace eval sgml {

    # Convenience routine
    proc cl x {
	return "\[$x\]"
    }

    # Define various regular expressions

    # Character classes
    variable Char \t\n\r\ -\xFF
    variable BaseChar A-Za-z
    variable Letter $BaseChar
    variable Digit 0-9
    variable CombiningChar {}
    variable Extender {}
    variable Ideographic {}

    # white space
    variable Wsp " \t\r\n"
    variable noWsp [cl ^$Wsp]

    # Various XML names
    variable NameChar \[-$Letter$Digit._:$CombiningChar$Extender\]
    variable Name \[_:$BaseChar$Ideographic\]$NameChar*
    variable Names ${Name}(?:$Wsp$Name)*
    variable Nmtoken $NameChar+
    variable Nmtokens ${Nmtoken}(?:$Wsp$Nmtoken)*

    # table of predefined entities for XML

    variable EntityPredef
    array set EntityPredef {
	lt <   gt >   amp &   quot \"   apos '
    }

}

# These regular expressions are defined here once for better performance

namespace eval sgml {
    variable Wsp

    # Watch out for case-sensitivity

    set attlist_exp [cl $Wsp]*([cl ^$Wsp]+)[cl $Wsp]*([cl ^$Wsp]+)[cl $Wsp]*(#REQUIRED|#IMPLIED)
    set attlist_enum_exp [cl $Wsp]*([cl ^$Wsp]+)[cl $Wsp]*\\(([cl ^)]*)\\)[cl $Wsp]*("([cl ^")])")? ;# "
    set attlist_fixed_exp [cl $Wsp]*([cl ^$Wsp]+)[cl $Wsp]*([cl ^$Wsp]+)[cl $Wsp]*(#FIXED)[cl $Wsp]*([cl ^$Wsp]+)

    set param_entity_exp [cl $Wsp]*([cl ^$Wsp]+)[cl $Wsp]*([cl ^"$Wsp]*)[cl $Wsp]*"([cl ^"]*)"

    set notation_exp [cl $Wsp]*([cl ^$Wsp]+)[cl $Wsp]*(.*)

}

### Utility procedures

# sgml::noop --
#
#	A do-nothing proc
#
# Arguments:
#	args	arguments
#
# Results:
#	Nothing.

proc sgml::noop args {
    return 0
}

# sgml::identity --
#
#	Identity function.
#
# Arguments:
#	a	arbitrary argument
#
# Results:
#	$a

proc sgml::identity a {
    return $a
}

# sgml::Error --
#
#	Throw an error
#
# Arguments:
#	args	arguments
#
# Results:
#	Error return condition.

proc sgml::Error args {
    uplevel return -code error [list $args]
}

### Following procedures are based on html_library

# sgml::zapWhite --
#
#	Convert multiple white space into a single space.
#
# Arguments:
#	data	plain text
#
# Results:
#	As above

proc sgml::zapWhite data {
    regsub -all "\[ \t\r\n\]+" $data { } data
    return $data
}

proc sgml::Boolean value {
    regsub {1|true|yes|on} $value 1 value
    regsub {0|false|no|off} $value 0 value
    return $value
}

