# xerces.tcl --
#
# This scripts runs the tests using the xerces parser.
#
# Copyright (c) 2000 by Zveno Pty Ltd
# All rights reserved.
#
# $Id: xerces.tcl,v 1.3 2001/02/06 07:29:51 doss Exp $

set auto_path [linsert $auto_path 0 [file dirname [file dirname [file join [pwd] [info script]]]]]

package require xerces
puts stdout "Selecting XML Parser Class: Xerces"
source all.tcl
