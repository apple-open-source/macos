# math.tcl --
#
#	Main 'package provide' script for the package 'math'.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
# Copyright (c) 2002 by Kevin B. Kenny.  All rights reserved.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: math.tcl,v 1.18 2004/01/15 06:36:13 andreas_kupries Exp $

package require Tcl 8.2		;# uses [lindex $l end-$integer]

namespace eval ::math {

    variable version 1.2.2

    # misc.tcl

    namespace export	cov		fibonacci	integrate
    namespace export	max		mean		min
    namespace export	product		random		sigma
    namespace export	stats		sum
    namespace export	expectDouble

    # combinatorics.tcl

    namespace export	ln_Gamma	factorial	choose
    namespace export	Beta

    # Set up for auto-loading

    variable home [file join [pwd] [file dirname [info script]]]
    if {[lsearch -exact $::auto_path $home] == -1} {
	lappend ::auto_path $home
    }

    package provide [namespace tail [namespace current]] $version
}
