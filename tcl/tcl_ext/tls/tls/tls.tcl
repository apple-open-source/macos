#
# Copyright (C) 1997-2000 Matt Newman <matt@novadigm.com> 
#
# $Header: /cvsroot/tls/tls/tls.tcl,v 1.6 2004/02/11 22:36:31 razzell Exp $
#
namespace eval tls {
    variable logcmd tclLog
    variable debug 0
 
    # Default flags passed to tls::import
    variable defaults {}

    # Maps UID to Server Socket
    variable srvmap
    variable srvuid 0
}
#
# Backwards compatibility, also used to set the default
# context options
#
proc tls::init {args} {
    variable defaults

    set defaults $args
}
#
# Helper function - behaves exactly as the native socket command.
#
proc tls::socket {args} {
    set idx [lsearch $args -server]
    if {$idx != -1} {
	set server 1
	set callback [lindex $args [expr {$idx+1}]]
	set args [lreplace $args $idx [expr {$idx+1}]]

	set usage "wrong # args: should be \"tls::socket -server command ?options? port\""
	set options "-cadir, -cafile, -certfile, -cipher, -command, -keyfile, -myaddr, -password, -request, -require, -ssl2, -ssl3, or -tls1"
    } else {
	set server 0

	set usage "wrong # args: should be \"tls::socket ?options? host port\""
	set options "-async, -cadir, -cafile, -certfile, -cipher, -command, -keyfile, -myaddr, -myport, -password, -request, -require, -ssl2, -ssl3, or -tls1"
    }
    set argc [llength $args]
    set sopts {}
    set iopts [concat [list -server $server] ${tls::defaults}]	;# Import options

    for {set idx 0} {$idx < $argc} {incr idx} {
	set arg [lindex $args $idx]
	switch -glob -- $server,$arg {
	    0,-async	{lappend sopts $arg}
	    0,-myport	-
	    *,-myaddr	{lappend sopts $arg [lindex $args [incr idx]]}
	    *,-cadir	-
	    *,-cafile	-
	    *,-certfile	-
	    *,-cipher	-
	    *,-command	-
	    *,-keyfile	-
	    *,-password	-
	    *,-request	-
	    *,-require	-
	    *,-ssl2	-
	    *,-ssl3	-
	    *,-tls1	{lappend iopts $arg [lindex $args [incr idx]]}
	    -*		{return -code error "bad option \"$arg\": must be one of $options"}
	    default	{break}
	}
    }
    if {$server} {
	if {($idx + 1) != $argc} {
	    return -code error $usage
	}
	set uid [incr ::tls::srvuid]

	set port [lindex $args [expr {$argc-1}]]
	lappend sopts $port
	#set sopts [linsert $sopts 0 -server $callback]
	set sopts [linsert $sopts 0 -server [list tls::_accept $iopts $callback]]
	#set sopts [linsert $sopts 0 -server [list tls::_accept $uid $callback]]
    } else {
	if {($idx + 2) != $argc} {
	    return -code error $usage
	}
	set host [lindex $args [expr {$argc-2}]]
	set port [lindex $args [expr {$argc-1}]]
	lappend sopts $host $port
    }
    #
    # Create TCP/IP socket
    #
    set chan [eval ::socket $sopts]
    if {!$server && [catch {
	#
	# Push SSL layer onto socket
	#
	eval [list tls::import] $chan $iopts
    } err]} {
	set info ${::errorInfo}
	catch {close $chan}
	return -code error -errorinfo $info $err
    }
    return $chan
}

# tls::_accept --
#
#   This is the actual accept that TLS sockets use, which then calls
#   the callback registered by tls::socket.
#
# Arguments:
#   iopts	tls::import opts
#   callback	server callback to invoke
#   chan	socket channel to accept/deny
#   ipaddr	calling IP address
#   port	calling port
#
# Results:
#   Returns an error if the callback throws one.
#
proc tls::_accept { iopts callback chan ipaddr port } {
    log 2 [list tls::_accept $iopts $callback $chan $ipaddr $port]

    set chan [eval [list tls::import $chan] $iopts]

    lappend callback $chan $ipaddr $port
    if {[catch {
	uplevel #0 $callback
    } err]} {
	log 1 "tls::_accept error: ${::errorInfo}"
	close $chan
	error $err $::errorInfo $::errorCode
    } else {
	log 2 "tls::_accept - called \"$callback\" succeeded"
    }
}
#
# Sample callback for hooking: -
#
# error
# verify
# info
#
proc tls::callback {option args} {
    variable debug

    #log 2 [concat $option $args]

    switch -- $option {
	"error"	{
	    foreach {chan msg} $args break

	    log 0 "TLS/$chan: error: $msg"
	}
	"verify"	{
	    # poor man's lassign
	    foreach {chan depth cert rc err} $args break

	    array set c $cert

	    if {$rc != "1"} {
		log 1 "TLS/$chan: verify/$depth: Bad Cert: $err (rc = $rc)"
	    } else {
		log 2 "TLS/$chan: verify/$depth: $c(subject)"
	    }
	    if {$debug > 0} {
		return 1;	# FORCE OK
	    } else {
		return $rc
	    }
	}
	"info"	{
	    # poor man's lassign
	    foreach {chan major minor state msg} $args break

	    if {$msg != ""} {
		append state ": $msg"
	    }
	    # For tracing
	    upvar #0 tls::$chan cb
	    set cb($major) $minor

	    log 2 "TLS/$chan: $major/$minor: $state"
	}
	default	{
	    return -code error "bad option \"$option\":\
		    must be one of error, info, or verify"
	}
    }
}

proc tls::xhandshake {chan} {
    upvar #0 tls::$chan cb

    if {[info exists cb(handshake)] && \
	$cb(handshake) == "done"} {
	return 1
    }
    while {1} {
	vwait tls::${chan}(handshake)
	if {![info exists cb(handshake)]} {
	    return 0
	}
	if {$cb(handshake) == "done"} {
	    return 1
	}
    }
}

proc tls::password {} {
    log 0 "TLS/Password: did you forget to set your passwd!"
    # Return the worlds best kept secret password.
    return "secret"
}

proc tls::log {level msg} {
    variable debug
    variable logcmd

    if {$level > $debug || $logcmd == ""} {
	return
    }
    set cmd $logcmd
    lappend cmd $msg
    uplevel #0 $cmd
}
