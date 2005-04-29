#! /bin/sh
# -*- tcl -*- \
exec tclsh "$0" ${1+"$@"}

# Generic framework for a microserv.tcl based server/
#
# Copyright (c) 2003 by Andreas Kupries <andreas_kupries@users.sourceforge.net>

# A server using this framework can be controlled through a socket
# connection they open to a listening socket which was opened by the
# creator of the server. The port to connect to is provided in the
# variable 'ctrlport'.
#
# The port the server will be listening on is specified through the
# contents of the variable 'port'. The actual port chosen is written
# to the control connection, as the first information. This also
# serves as a signal that the server is ready.
#
# The server will exit when the control connection is closed by the
# spawning process.
#
#           | data to be set by the creator of a full server.
#           |
# logfile   |
# port      |
# responses |
# ctrlport  |

# ##########################################################
# Setup logging
# Prevent log messages for now, or log into server log.

set         log [open $logfile w]
fconfigure $log -buffering none
proc log {txt} {global log ; puts $log $txt}
proc log__ {l t} {log "$l $t"}

log__ debug "musub  | framework entered"

package require log ; # tcllib | logging
::log::lvCmdForall log__
#::log::lvSuppress info
#::log::lvSuppress notice
#::log::lvSuppress debug
#::log::lvSuppress warning

log__ debug "musub  | logging activated"

# ##########################################################
# Handle activity on the control connection
# - closing => exit server
# - read single line, evaluate command in that line ! trusted

proc done {chan} {
    if {[eof $chan]} {
	log__ debug "musub  | shutdown through caller, control connection was closed"
	exit
    }

    set n [gets $chan line]
    log__ debug "musub  | gets = $n ($line)"

    if {$n < 0} {return}
    set line [string trim $line]
    if {$line == {}} {return}

    log__ debug "musub  | eval ($line)"
    uplevel #0 $line
    return
}

# ##########################################################
# Setup the control connection.

set         control [socket localhost $ctrlport]
fileevent  $control readable [list done $control]
fconfigure $control -blocking 0

muserv::control $control

log__ debug "musub  | control connection up"

# ##########################################################
# Start server ...
# If the incoming port number is 0 the return value
# will contain the actual port the server is listening on.

set             port [muserv::listen $port $responses]
puts  $control $port
flush $control

log__ debug "musub  | server ready and waiting ..."

vwait __forever__
log__ debug "musub  | reached infinity, closing :)"
exit

