# procRedefNub.tcl --
#
#	Test file for tbcload.  This file contains a renaming of the
#	"proc" command to the "DbgNub_procCmd" command, a special
#	case for tcldebugger.
#
# Copyright (c) 1998-2000 by Ajuba Solutions
# All rights reserved.
# 
# RCS: @(#) $Id: procRedefNub.tcl,v 1.3 2000/05/30 22:19:12 wart Exp $

if {[catch {

    if {[catch {package require tbcload}]} {
	if {$env(TBCLOAD_LOAD_STRING) != ""} {
	    eval $env(TBCLOAD_LOAD_STRING)
	}
    }

    rename proc DbgNub_procCmd
    DbgNub_procCmd proc { name argList body } {
	DbgNub_procCmd $name $argList {return 333}
    }
    source [file join [file dirname [file dirname [info script]]] \
	    tbc10 proc.tbc]
} err]} {
    puts "err in [info script]: $err"
}
