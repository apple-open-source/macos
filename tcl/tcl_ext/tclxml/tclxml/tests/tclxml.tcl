# tclxml.tcl --
#
# This scripts runs the tests using the Tcl XML parser.
#
# Copyright (c) 1999-2000 by Zveno Pty Ltd
# All rights reserved.
#
# $Id: tclxml.tcl,v 1.5 2001/02/06 07:29:51 doss Exp $

set auto_path [linsert $auto_path 0 [file dirname [file dirname [file join [pwd] [info script]]]]]

package require tclparser
puts stdout "Selecting XML Parser Class: TCL"
source all.tcl
