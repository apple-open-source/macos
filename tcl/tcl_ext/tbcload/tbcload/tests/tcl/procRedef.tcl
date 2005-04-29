# procRedef.tcl --
#
#	Test file for tbcload.  This file contains a redefinition of the
#	"proc" command.
#
# Copyright (c) 1998-2000 by Ajuba Solutions
# All rights reserved.
# 
# RCS: @(#) $Id: procRedef.tcl,v 1.3 2000/05/30 22:19:12 wart Exp $

if {[catch {
    if {[catch {package require tbcload}]} {
	if {$env(TBCLOAD_LOAD_STRING) != ""} {
	    eval $env(TBCLOAD_LOAD_STRING)
	}
    }

    rename proc __test_originalProcCmd
    __test_originalProcCmd proc { name argList body } {
	__test_originalProcCmd $name $argList {return 333}
    }

    source [file join [file dirname [file dirname [info script]]] \
	    tbc10 proc.tbc]
} err]} {
    puts "err in [info script]: $err"
}
