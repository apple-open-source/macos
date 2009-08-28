# loadtnc.tcl --
#
# This file is [source]d by all.tcl and all test files, to ensure, that
# the tcltest package and the lastest tnc build is present.

if {[lsearch [namespace children] ::tcltest] == -1} {
    if {$tcl_version < 8.2} {
        puts stderr "sourcing def.tcl"
        source [file join [file dir [info script]] defs.tcl]
        set auto_path [pwd]
    } else {
        package require tcltest
        namespace import ::tcltest::*
    }
}

if {[catch {package present tdom}]} {
    package require tdom 0.7.5
}

if {[catch {package require tnc 0.3}]} {
    load [file join [file dir [info script]] .. libtnc0.3.0.so]
}

