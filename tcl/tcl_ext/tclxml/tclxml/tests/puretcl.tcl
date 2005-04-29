# puretcl.tcl --
#
# This scripts runs the tests using the Tcl XML parser.
#
# Copyright (c) 1999-2000 by Zveno Pty Ltd
# All rights reserved.
#
# $Id: puretcl.tcl,v 1.1 2001/02/06 07:29:51 doss Exp $

set auto_path [linsert $auto_path 0 [file dirname [file dirname [file join [pwd] [info script]]]]]
set auto_path [linsert $auto_path 0 [pwd]]

package require puretclparser

puts stdout "Selecting XML Parser Class: TCL (Script Only)"
source all.tcl

