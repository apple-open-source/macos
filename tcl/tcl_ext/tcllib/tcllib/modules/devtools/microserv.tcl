# -*- tcl -*-
# MicroServer (also MicroServant)
# aka muserv (mu = greek micron)
#
# Copyright (c) 2003 by Andreas Kupries <andreas_kupries@users.sourceforge.net>

# ####################################################################

# Code for a simple server listening on one part for a connection and
# then performing a fixed sequence of responses, independent of the
# queries sent to it. This should make the testing of servers and
# clients for a particular protocol easier. Especially as this
# micro-server is better suited to push data tailored to generating
# boundary conditions on the other side of the connection than a true
# client/server for the protoco.

# ####################################################################

package require log

namespace eval ::muserv {
    variable port      ; # Port to listen on for protocol connections.
    variable responses ; # Script to run for a protocol connection.
    variable sock      ; # Channel of the protocol connection.
    variable lastline  ; # Last line received on the protocol connection.
    variable ctrlsock  ; # Channel of the control connection.
    variable trace     ; # Recorded trace of activity.
}

# ####################################################################
# Public functionality

# ::muserv::listen --
#
#	Setup the server to listen for a connection

proc ::muserv::listen {theport theresponses} {
    variable port      $theport
    variable responses $theresponses
    set lsock [socket -server ::muserv::New $theport]
    set port  [lindex [fconfigure $lsock -sockname] end]
    log::log debug "muserv | Listening on :: $port"
    return $port
}

proc ::muserv::control {control} {
    variable ctrlsock $control
    return
}

proc ::muserv::control {control} {
    variable ctrlsock $control
    return
}

proc ::muserv::gettrace {} {
    variable ctrlsock
    variable trace

    puts  $ctrlsock [join $trace \n]
    puts  $ctrlsock __EOTrace__
    flush $ctrlsock
    return
}

# ####################################################################
# Private functionality

# ::muserv::New --
#
#	Store the connection information and setup the dialog

proc ::muserv::New {thesock addr port} {
    variable sock $thesock
    log::log debug "muserv | Connected    :: $addr $port :: $sock"
    after 0 ::muserv::Dialog
    return
}

# ::muserv::Dialog --
#
#	Run the pre-programmed responses on the connection

proc ::muserv::Dialog {} {
    variable responses
    variable sock

    log::log debug "muserv | Dialog       :: ..."
    catch {eval  $responses}
    log::log debug "muserv | Dialog       :: ... done"
    catch {close $sock}
    set sock ""
    log::log debug "muserv | Connection   :: Closed"
    return
}

# ####################################################################
# Low-level interaction and configuration commands

proc ::muserv::__Trace       {line} {
    variable trace
    log::log debug "muserv | Logging ____ :: == $line"
    lappend trace $line
    return
}
proc ::muserv::__Send        {line} {
    log::log debug "muserv | Sending ____ :: >> $line"
    variable sock ; puts $sock $line ; flush $sock
    return
}
proc ::muserv::__Wait        {}     {
    variable lastline
    variable sock ; gets $sock line ; set lastline $line
    log::log debug "muserv | Received ___ :: << $line"
    return
}
proc ::muserv::__Reconfigure {args} {
    log::log debug "muserv | Reconfigure  :: [join $args]"
    variable sock ; eval fconfigure $sock $args
    return
}
proc ::muserv::__Got {} {variable lastline ; __Trace $lastline}

# ####################################################################
# Semi-public functionality: Commands available to program the dialog.

proc ::muserv::CrLf        {}     {__Reconfigure -translation crlf   ; return}
proc ::muserv::Binary      {}     {__Reconfigure -translation binary ; return}
proc ::muserv::Send        {line} {         __Send $line ; return}
proc ::muserv::Respond     {line} {__Wait ; __Send $line ; return}
proc ::muserv::Wait        {}     {__Wait ;                return}
proc ::muserv::RespondLog  {line} {__Wait ; __Got ; __Send $line ; return}
proc ::muserv::WaitLog     {}     {__Wait ; __Got ;                return}

# ####################################################################
