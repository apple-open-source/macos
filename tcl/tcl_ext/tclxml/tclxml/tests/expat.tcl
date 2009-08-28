# expat.tcl --
#
# This scripts runs the tests using the expat parser.
#
# Copyright (c) 1999-2000 by Zveno Pty Ltd
# All rights reserved.
#
# $Id: expat.tcl,v 1.5 2001/02/06 07:29:51 doss Exp $

set auto_path [linsert $auto_path 0 [file dirname [file dirname [file join [pwd] [info script]]]]]

package require expat
puts stdout "selecting XML Parser Class: Expat"
source all.tcl

