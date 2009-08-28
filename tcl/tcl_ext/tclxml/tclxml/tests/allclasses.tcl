# allclasses.tcl --
#
# This script runs all the tests on each parser.
# This file contains a top-level script to run all of the Tcl
# tests on each pasrer class in turn.  
# Execute it by invoking "source allclasses.test" when running tcltest
# in this directory.
#
#
# Copyright (c) 2000 by Zveno Pty Ltd
# All rights reserved.
#
# $Id: allclasses.tcl,v 1.2 2001/02/06 07:51:41 doss Exp $

set auto_path [linsert $auto_path 0 [file dirname [file dirname [file join [pwd] [info script]]]]]

set lhstext "\nSelecting XML Parser Class"

package require puretclparser
puts stdout "$lhstext : TCL (Script Only)"
source all.tcl

package require tclparser
puts stdout "$lhstext : TCL"
source all.tcl

package require expat
puts stdout "$lhstext : Expat"
source all.tcl

#package require xerces
#puts stdout "$lhstext : Xerces"
#source all.tcl
