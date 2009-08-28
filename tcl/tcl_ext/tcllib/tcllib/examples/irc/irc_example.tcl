#! /bin/sh
# -*- tcl -*- \
exec tclsh "$0" ${1+"$@"}

# irc example script, by David N. Welton <davidw@dedasys.com>
# $Id: irc_example.tcl,v 1.9 2004/03/25 07:23:59 andreas_kupries Exp $

# I include these so that it can find both the irc package and the
# logger package that irc needs.

set auto_path "[file join [file dirname [info script]] .. .. modules irc] $auto_path"
set auto_path "[file join [file dirname [info script]] .. .. modules log] $auto_path"
package require irc 0.4

namespace eval ircclient {
    variable channel \#tcl

    # Pick up a nick from the command line, or default to TclIrc.
    if { [lindex $::argv 0] != "" } {
	set nick [lindex $::argv 0]
    } else {
	set nick TclIrc
    }

    set cn [::irc::connection]
    # Connect to the server.
    $cn connect irc.freenode.net 6667
    $cn user $nick localhost domain "www.tcl.tk"
    $cn nick $nick
    while { 1 } {
	source mainloop.tcl
	vwait ::ircclient::RELOAD
    }
}

