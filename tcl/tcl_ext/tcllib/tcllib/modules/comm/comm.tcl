# comm.tcl --
#
#	socket-based 'send'ing of commands between interpreters.
#
# %%_OSF_FREE_COPYRIGHT_%%
# Copyright (C) 1995-1998 The Open Group.   All Rights Reserved.
# (Please see the file "comm.LICENSE" that accompanied this source,
#  or http://www.opengroup.org/www/dist_client/caubweb/COPYRIGHT.free.html)
# Copyright (c) 2003 ActiveState Corporation
#
# This is the 'comm' package written by Jon Robert LoVerso, placed
# into its own namespace during integration into tcllib.
#
# Note that the actual code was changed in several places (Reordered,
# eval speedup)
# 
#	comm works just like Tk's send, except that it uses sockets.
#	These commands work just like "send" and "winfo interps":
#
#		comm send ?-async? <id> <cmd> ?<arg> ...?
#		comm interps
#
#	See the manual page comm.n for further details on this package.
#
# RCS: @(#) $Id: comm.tcl,v 1.12 2004/01/15 06:36:12 andreas_kupries Exp $

package require Tcl 8.2

namespace eval ::comm {
    namespace export comm comm_send

    variable  comm
    array set comm {}

    if {![info exists comm(chans)]} {
	array set comm {
	    debug 0 chans {} localhost 127.0.0.1
	    connecting,hook	1
	    connected,hook	1
	    incoming,hook	1
	    eval,hook		1
	    callback,hook	1
	    reply,hook		1
	    lost,hook		1
	    offerVers		{3 2}
	    acceptVers		{3 2}
	    defVers		2
	    defaultEncoding	"utf-8"
	}
	set comm(lastport) [expr {[pid] % 32768 + 9999}]
	# fast check for acceptable versions
	foreach comm(_x) $comm(acceptVers) {
	    set comm($comm(_x),vers) 1
	}
	catch {unset comm(_x)}
    }

    # Class variables:
    #	lastport		saves last default listening port allocated
    #	debug			enable debug output
    #	chans			list of allocated channels
    #
    # Channel instance variables:
    # comm()
    #	$ch,port		listening port (our id)
    #	$ch,socket		listening socket
    #	$ch,local		boolean to indicate if port is local
    #	$ch,serial		next serial number for commands
    #
    #	$ch,hook,$hook		script for hook $hook
    #
    #	$ch,peers,$id		open connections to peers; ch,id=>fid
    #	$ch,fids,$fid		reverse mapping for peers; ch,fid=>id
    #	$ch,vers,$id		negotiated protocol version for id
    #	$ch,pending,$id		list of outstanding send serial numbers for id
    #
    #	$ch,buf,$fid		buffer to collect incoming data
    #	$ch,result,$serial	result value set here to wake up sender
    #	$ch,return,$serial	return codes to go along with result

    if {0} {
	# Propogate result, code, and errorCode.  Can't just eval
	# otherwise TCL_BREAK gets turrned into TCL_ERROR.
	global errorInfo errorCode
	set code [catch [concat commSend $args] res]
	return -code $code -errorinfo $errorInfo -errorcode $errorCode $res
    }
}

# ::comm::comm_send --
#
#	Convenience command. Replaces Tk 'send' and 'winfo' with
#	versions using the 'comm' variants. Multiple calls are
#	allowed, only the first one will have an effect.
#
# Arguments:
#	None.
#
# Results:
#	None.

proc ::comm::comm_send {} {
    proc send {args} {
	# Use pure lists to speed this up.
	uplevel 1 [linsert $args 0 ::comm::comm send]
    }
    rename winfo tk_winfo
    proc winfo {cmd args} {
	if {![string match in* $cmd]} {
	    # Use pure lists to speed this up ...
	    return [uplevel 1 [linsert $args 0 tk_winfo $cmd]]
	}
	return [::comm::comm interps]
    }
    proc ::comm::comm_send {} {}
}

# ::comm::comm --
#
#	See documentation for public methods of "comm".
#	This procedure is followed by the definition of
#	the public methods themselves.
#
# Arguments:
#	cmd	Invoked method
#	args	Arguments to method.
#
# Results:
#	As of the invoked method.

proc ::comm::comm {cmd args} {
    set method [info commands ::comm::comm_cmd_$cmd*]

    if {[llength $method] == 1} {
	set chan ::comm::comm; # passed to methods
	return [uplevel 1 [linsert $args 0 $method $chan]]
    } else {
	foreach c [info commands ::comm::comm_cmd_*] {
	    # remove ::comm::comm_cmd_
	    lappend cmds [string range $c 17 end]
	}
        return -code error "unknown subcommand \"$cmd\":\
		must be one of [join [lsort $cmds] {, }]"
    }
}

proc ::comm::comm_cmd_connect {chan args} {
    uplevel 1 [linsert $args 0 [namespace current]::commConnect $chan]
}
proc ::comm::comm_cmd_self {chan args} {
    variable comm
    return $comm($chan,port)
}
proc ::comm::comm_cmd_channels {chan args} {
    variable comm
    return $comm(chans)
}
proc ::comm::comm_cmd_configure {chan args} {
    uplevel 1 [linsert $args 0 [namespace current]::commConfigure $chan 0]
}
proc ::comm::comm_cmd_ids {chan args} {
    variable comm
    set res $comm($chan,port)
    foreach {i id} [array get comm $chan,fids,*] {lappend res $id}
    return $res
}
interp alias {} ::comm::comm_cmd_interps {} ::comm::comm_cmd_ids
proc ::comm::comm_cmd_remoteid {chan args} {
    variable comm
    if {[info exists comm($chan,remoteid)]} {
	set comm($chan,remoteid)
    } else {
	return -code error "No remote commands processed yet"
    }
}
proc ::comm::comm_cmd_debug {chan bool} {
    variable comm
    return [set comm(debug) [string is true -strict $bool]]
}

# hook --
#
#	Internal command. Implements 'comm hook'.
#
# Arguments:
#	hook	hook to modify
#	script	Script to add/remove to/from the hook
#
# Results:
#	None.
#
proc ::comm::comm_cmd_hook {chan hook {script +}} {
    variable comm
    if {![info exists comm($hook,hook)]} {
	return -code error "Unknown hook invoked"
    }
    if {!$comm($hook,hook)} {
	return -code error "Unimplemented hook invoked"
    }
    if {[string equal + $script]} {
	if {[catch {set comm($chan,hook,$hook)} ret]} {
	    return
	}
	return $ret
    }
    if {[string match +* $script]} {
	append comm($chan,hook,$hook) \n [string range $script 1 end]
    } else {
	set comm($chan,hook,$hook) $script
    }
    return
}

# abort --
#
#	Close down all peer connections.
#	Implements the 'comm abort' method.
#
# Arguments:
#	None.
#
# Results:
#	None.

proc ::comm::comm_cmd_abort {chan} {
    variable comm

    foreach pid [array names comm $chan,peers,*] {
	commLostConn $chan $comm($pid) "Connection aborted by request"
    }
}

# destroy --
#
#	Destroy the channel invoking it.
#	Implements the 'comm destroy' method.
#
# Arguments:
#	None.
#
# Results:
#	None.
#
proc ::comm::comm_cmd_destroy {chan} {
    variable comm
    catch {close $comm($chan,socket)}
    comm_cmd_abort $chan
    catch {unset comm($chan,port)}
    catch {unset comm($chan,local)}
    catch {unset comm($chan,socket)}
    unset comm($chan,serial)
    set pos [lsearch -exact $comm(chans) $chan]
    set comm(chans) [lreplace $comm(chans) $pos $pos]
    if {![string equal ::comm::comm $chan]} {
	rename $chan {}
    }
}

# shutdown --
#
#	Close down a peer connection.
#	Implements the 'comm shutdown' method.
#
# Arguments:
#	id	Reference to the remote interp
#
# Results:
#	None.
#
proc ::comm::comm_cmd_shutdown {chan id} {
    variable comm

    if {[info exists comm($chan,peers,$id)]} {
	commLostConn $chan $comm($chan,peers,$id) \
	    "Connection shutdown by request"
    }
}

# new --
#
#	Create a new comm channel/instance.
#	Implements the 'comm new' method.
#
# Arguments:
#	ch	Name of the new channel
#	args	Configuration, in the form of -option value pairs.
#
# Results:
#	None.
#
proc ::comm::comm_cmd_new {chan ch args} {
    variable comm

    if {[lsearch -exact $comm(chans) $ch] >= 0} {
	return -code error "Already existing channel: $ch"
    }
    if {([llength $args] % 2) != 0} {
	return -code error "Must have an even number of config arguments"
    }
    # ensure that the new channel name is fully qualified
    set ch ::[string trimleft $ch :]
    if {[string equal ::comm::comm $ch]} {
	# allow comm to be recreated after destroy
    } elseif {[string equal $ch [info commands $ch]]} {
	return -code error "Already existing command: $ch"
    } else {
	# Create the new channel with fully qualified proc name
	proc $ch {cmd args} {
	    set method [info commands ::comm::comm_cmd_$cmd*]

	    if {[llength $method] == 1} {
		# this should work right even if aliased
		# it is passed to methods to identify itself
		set chan [namespace origin [lindex [info level 0] 0]]
		return [uplevel 1 [linsert $args 0 $method $chan]]
	    } else {
		foreach c [info commands ::comm::comm_cmd_*] {
		    # remove ::comm::comm_cmd_
		    lappend cmds [string range $c 17 end]
		}
		return -code error "unknown subcommand \"$cmd\":\
			must be one of [join [lsort $cmds] {, }]"
	    }
	}
    }
    lappend comm(chans) $ch
    set chan $ch
    set comm($chan,serial) 0
    set comm($chan,chan)   $chan
    set comm($chan,port)   0
    set comm($chan,listen) 0
    set comm($chan,socket) ""
    set comm($chan,local)  1
    set comm($chan,encoding) $comm(defaultEncoding)

    if {[llength $args] > 0} {
	if {[catch [linsert $args 0 commConfigure $chan 1] err]} {
	    comm_cmd_destroy $chan
	    return -code error $err
	}
    }
    return $chan
}

# send --
#
#	Send command to a specified channel.
#	Implements the 'comm send' method.
#
# Arguments:
#	args	see inside
#
# Results:
#	varies.
#
proc ::comm::comm_cmd_send {chan args} {
    variable comm

    set cmd send

    # args = ?-async | -command command? id cmd ?arg arg ...?
    set i 0
    set opt [lindex $args $i]
    if {[string equal -async $opt]} {
	set cmd async
	incr i
    } elseif {[string equal -command $opt]} {
	set cmd command
	set callback [lindex $args [incr i]]
	incr i
    }
    # args = id cmd ?arg arg ...?

    set id [lindex $args $i]
    incr i
    set args [lrange $args $i end]

    if {![info complete $args]} {
	return -code error "Incomplete command"
    }
    if {![llength $args]} {
	return -code error \
		"wrong # args: should be \"send ?-async? id arg ?arg ...?\""
    }
    if {[catch {commConnect $chan $id} fid]} {
	return -code error "Connect to remote failed: $fid"
    }

    set ser [incr comm($chan,serial)]
    # This is unneeded - wraps from 2147483647 to -2147483648
    ### if {$comm($chan,serial) == 0x7fffffff} {set comm($chan,serial) 0}

    commDebug {puts stderr "send <[list [list $cmd $ser $args]]>"}

    # The double list assures that the command is a single list when read.
    puts  $fid [list [list $cmd $ser $args]]
    flush $fid

    # wait for reply if so requested

    if {[string equal command $cmd]} {
	# In this case, don't wait on the command result.  Set the callback
	# in the return and that will be invoked by the result.
	lappend comm($chan,pending,$id) [list $ser callback]
	set comm($chan,return,$ser) $callback
	return $ser
    } elseif {[string equal send $cmd]} {
	upvar 0 comm($chan,pending,$id) pending	;# shorter variable name

	lappend pending $ser
	set comm($chan,return,$ser) ""		;# we're waiting

	commDebug {puts stderr "--<<waiting $chan $ser>>--"}
	vwait ::comm::comm($chan,result,$ser)

	# if connection was lost, pending is gone
	if {[info exists pending]} {
	    set pos [lsearch -exact $pending $ser]
	    set pending [lreplace $pending $pos $pos]
	}

	commDebug {
	    puts stderr "result\
		    <$comm($chan,return,$ser);$comm($chan,result,$ser)>"
	}
	after idle unset ::comm::comm($chan,result,$ser)

	array set return $comm($chan,return,$ser)
	unset comm($chan,return,$ser)
	switch -- $return(-code) {
	    "" - 0 {return $comm($chan,result,$ser)}
	    1 {
		return  -code $return(-code) \
			-errorinfo $return(-errorinfo) \
			-errorcode $return(-errorcode) \
			$comm($chan,result,$ser)
	    }
	    default {return -code $return(-code) $comm($chan,result,$ser)}
	}
    }
}

###############################################################################

# ::comm::commDebug --
#
#	Internal command. Conditionally executes debugging
#	statements. Currently this are only puts commands logging the
#	various interactions. These could be replaced with calls into
#	the 'log' module.
#
# Arguments:
#	arg	Tcl script to execute.
#
# Results:
#	None.

proc ::comm::commDebug {cmd} {
    variable comm
    if {$comm(debug)} {
	uplevel 1 $cmd
    }
}

# ::comm::commConfVars --
#
#	Internal command. Used to declare configuration options.
#
# Arguments:
#	v	Name of configuration option.
#	t	Default value.
#
# Results:
#	None.

proc ::comm::commConfVars {v t} {
    variable comm
    set comm($v,var) $t
    set comm(vars) {}
    foreach c [array names comm *,var] {
	lappend comm(vars) [lindex [split $c ,] 0]
    }
}
::comm::commConfVars port p
::comm::commConfVars local b
::comm::commConfVars listen b
::comm::commConfVars socket ro
::comm::commConfVars chan ro
::comm::commConfVars serial ro
::comm::commConfVars encoding enc

# ::comm::commConfigure --
#
#	Internal command. Implements 'comm configure'.
#
# Arguments:
#	force	Boolean flag. If set the socket is reinitialized.
#	args	New configuration, as -option value pairs.
#
# Results:
#	None.

proc ::comm::commConfigure {chan {force 0} args} {
    variable comm

    # query
    if {[llength $args] == 0} {
	foreach v $comm(vars) {lappend res -$v $comm($chan,$v)}
	return $res
    } elseif {[llength $args] == 1} {
	set arg [lindex $args 0]
	set var [string range $arg 1 end]
	if {![string match -* $arg] || ![info exists comm($var,var)]} {
	    return -code error "Unknown configuration option: $arg"
	}
	return $comm($chan,$var)
    }

    # set
    set opt 0
    foreach arg $args {
	incr opt
	if {[info exists skip]} {unset skip; continue}
	set var [string range $arg 1 end]
	if {![string match -* $arg] || ![info exists comm($var,var)]} {
	    return -code error "Unknown configuration option: $arg"
	}
	set optval [lindex $args $opt]
	switch $comm($var,var) {
	    b {
		# FRINK: nocheck
		set $var [string is true -strict $optval]
		set skip 1
	    }
	    v {
		# FRINK: nocheck
		set $var $optval
		set skip 1
	    }
	    p {
		if {
		    ![string equal $optval ""] &&
		    ![string is integer $optval]
		} {
		    return -code error \
			"Non-port to configuration option: -$var"
		}
		# FRINK: nocheck
		set $var $optval
		set skip 1
	    }
	    i {
		if {![string is integer $optval]} {
		    return -code error \
			"Non-integer to configuration option: -$var"
		}
		# FRINK: nocheck
		set $var $optval
		set skip 1
	    }
	    enc {
		# to configure encodings, we will need to extend the
		# protocol to allow for handshaked encoding changes
		return -code error "encoding not configurable"
		if {[lsearch -exact [encoding names] $optval] == -1} {
		    return -code error \
			"Unknown encoding to configuration option: -$var"
		}
		set $var $optval
		set skip 1
	    }
	    ro { return -code error "Readonly configuration option: -$var" }
	}
    }
    if {[info exists skip]} {
	return -code error "Missing value for option: $arg"
    }

    foreach var {port listen local} {
	# FRINK: nocheck
	if {[info exists $var] && [set $var] != $comm($chan,$var)} {
	    incr force
	    # FRINK: nocheck
	    set comm($chan,$var) [set $var]
	}
    }

    if {[info exists encoding] &&
	![string equal $encoding $comm($chan,encoding)]} {
	# This should not be entered yet
	set comm($chan,encoding) $encoding
	fconfigure $comm($chan,socket) -encoding $encoding
	foreach {i sock} [array get comm $chan,peers,*] {
	    fconfigure $sock -encoding $encoding
	}
    }

    # do not re-init socket
    if {!$force} {return ""}

    # User is recycling object, possibly to change from local to !local
    if {[info exists comm($chan,socket)]} {
	comm_cmd_abort $chan
	catch {close $comm($chan,socket)}
	unset comm($chan,socket)
    }

    set comm($chan,socket) ""
    if {!$comm($chan,listen)} {
	set comm($chan,port) 0
	return ""
    }

    if {[info exists port] && [string equal "" $comm($chan,port)]} {
	set nport [incr comm(lastport)]
    } else {
	set userport 1
	set nport $comm($chan,port)
    }
    while {1} {
	set cmd [list socket -server [list ::comm::commIncoming $chan]]
	if {$comm($chan,local)} {
	    lappend cmd -myaddr $comm(localhost)
	}
	lappend cmd $nport
	if {![catch $cmd ret]} {
	    break
	}
	if {[info exists userport] || ![string match "*already in use" $ret]} {
	    # don't erradicate the class
	    if {![string equal ::comm::comm $chan]} {
		rename $chan {}
	    }
	    return -code error $ret
	}
	set nport [incr comm(lastport)]
    }
    set comm($chan,socket) $ret
    fconfigure $ret -translation lf -encoding $comm($chan,encoding)

    # If port was 0, system allocated it for us
    set comm($chan,port) [lindex [fconfigure $ret -sockname] 2]
    return ""
}

# ::comm::commConnect --
#
#	Internal command. Called to connect to a remote interp
#
# Arguments:
#	id	Specification of the location of the remote interp.
#		A list containing either one or two elements.
#		One element = port, host is localhost.
#		Two elements = port and host, in this order.
#
# Results:
#	fid	channel handle of the socket the connection goes through.

proc ::comm::commConnect {chan id} {
    variable comm

    commDebug {puts stderr "commConnect $id"}

    # process connecting hook now
    if {[info exists comm($chan,hook,connecting)]} {
    	eval $comm($chan,hook,connecting)
    }

    if {[info exists comm($chan,peers,$id)]} {
	return $comm($chan,peers,$id)
    }
    if {[lindex $id 0] == 0} {
	return -code error "Remote comm is anonymous; cannot connect"
    }

    if {[llength $id] > 1} {
	set host [lindex $id 1]
    } else {
	set host $comm(localhost)
    }
    set port [lindex $id 0]
    set fid [socket $host $port]

    # process connected hook now
    if {[info exists comm($chan,hook,connected)]} {
    	if {[catch $comm($chan,hook,connected) err]} {
	    global errorInfo
	    set ei $errorInfo
	    close $fid
	    error $err $ei
	}
    }

    # commit new connection
    commNewConn $chan $id $fid

    # send offered protocols versions and id to identify ourselves to remote
    puts $fid [list $comm(offerVers) $comm($chan,port)]
    set comm($chan,vers,$id) $comm(defVers)		;# default proto vers
    flush  $fid
    return $fid
}

# ::comm::commIncoming --
#
#	Internal command. Called for an incoming new connection.
#	Handles connection setup and initialization.
#
# Arguments:
#	chan	logical channel handling the connection.
#	fid	channel handle of the socket running the connection.
#	addr	ip address of the socket channel 'fid'
#	remport	remote port for the socket channel 'fid'
#
# Results:
#	None.

proc ::comm::commIncoming {chan fid addr remport} {
    variable comm

    commDebug {puts stderr "commIncoming $chan $fid $addr $remport"}

    # process incoming hook now
    if {[info exists comm($chan,hook,incoming)]} {
    	if {[catch $comm($chan,hook,incoming) err]} {
	    global errorInfo
	    set ei $errorInfo
	    close $fid
	    error $err $ei
	}
    }

    # a list of offered proto versions is the first word of first line
    # remote id is the second word of first line
    # rest of first line is ignored
    set protoline [gets $fid]
    set offeredvers [lindex $protoline 0]
    set remid [lindex $protoline 1]

    # use the first supported version in the offered list
    foreach v $offeredvers {
	if {[info exists comm($v,vers)]} {
	    set vers $v
	    break
	}
    }
    if {![info exists vers]} {
	close $fid
	error "Unknown offered protocols \"$protoline\" from $addr/$remport"
    }

    # If the remote host addr isn't our local host addr,
    # then add it to the remote id.
    if {[string equal [lindex [fconfigure $fid -sockname] 0] $addr]} {
	set id $remid
    } else {
	set id [list $remid $addr]
    }

    # Detect race condition of two comms connecting to each other
    # simultaneously.  It is OK when we are talking to ourselves.

    if {[info exists comm($chan,peers,$id)] && $id != $comm($chan,port)} {

	puts stderr "commIncoming race condition: $id"
	puts stderr "peers=$comm($chan,peers,$id) port=$comm($chan,port)"

	# To avoid the race, we really want to terminate one connection.
	# However, both sides are commited to using it.
	# commConnect needs to be sychronous and detect the close.
	# close $fid
	# return $comm($chan,peers,$id)
    }

    # Make a protocol response.  Avoid any temptation to use {$vers > 2}
    # - this forces forwards compatibility issues on protocol versions
    # that haven't been invented yet.  DON'T DO IT!  Instead, test for
    # each supported version explicitly.  I.e., {$vers >2 && $vers < 5} is OK.

    switch $vers {
	3 {
	    # Respond with the selected version number
	    puts  $fid [list [list vers $vers]]
	    flush $fid
	}
    }

    # commit new connection
    commNewConn $chan $id $fid
    set comm($chan,vers,$id) $vers
}

# ::comm::commNewConn --
#
#	Internal command. Common new connection processing
#
# Arguments:
#	id	Reference to the remote interp
#	fid	channel handle of the socket running the connection.
#
# Results:
#	None.

proc ::comm::commNewConn {chan id fid} {
    variable comm

    commDebug {puts stderr "commNewConn $id $fid"}

    # There can be a race condition two where comms connect to each other
    # simultaneously.  This code favors our outgoing connection.

    if {[info exists comm($chan,peers,$id)]} {
	# abort this connection, use the existing one
	# close $fid
	# return -code return $comm($chan,peers,$id)
    } else {
	set comm($chan,pending,$id) {}
    	set comm($chan,peers,$id) $fid
    }
    set comm($chan,fids,$fid) $id
    fconfigure $fid -translation lf -encoding $comm($chan,encoding) -blocking 0
    fileevent $fid readable [list ::comm::commCollect $chan $fid]
}

# ::comm::commLostConn --
#
#	Internal command. Called to tidy up a lost connection,
#	including aborting ongoing sends. Each send should clean
#	themselves up in pending/result.
#
# Arguments:
#	fid	Channel handle of the socket which got lost.
#	reason	Message describing the reason of the loss.
#
# Results:
#	reason

proc ::comm::commLostConn {chan fid reason} {
    variable comm

    commDebug {puts stderr "commLostConn $fid $reason"}

    catch {close $fid}

    set id $comm($chan,fids,$fid)

    foreach s $comm($chan,pending,$id) {
	if {[string equal "callback" [lindex $s end]]} {
	    set ser [lindex $s 0]
	    if {[info exists comm($chan,return,$ser)]} {
		set args [list -id       $id \
			      -serial    $ser \
			      -chan      $chan \
			      -code      -1 \
			      -errorcode NONE \
			      -errorinfo "" \
			      -result    $reason \
			     ]
		if {[catch {uplevel #0 $comm($chan,return,$ser) $args} err]} {
		    commBgerror $err
		}
	    }
	} else {
	    set comm($chan,return,$s) {-code error}
	    set comm($chan,result,$s) $reason
	}
    }
    unset comm($chan,pending,$id)
    unset comm($chan,fids,$fid)
    catch {unset comm($chan,peers,$id)}		;# race condition
    catch {unset comm($chan,buf,$fid)}

    # process lost hook now
    catch {catch $comm($chan,hook,lost)}

    return $reason
}

proc ::comm::commBgerror {err} {
    # SF Tcllib Patch #526499
    # (See http://sourceforge.net/tracker/?func=detail&aid=526499&group_id=12883&atid=312883
    #  for initial request and comments)
    #
    # Error in async call. Look for [bgerror] to report it. Same
    # logic as in Tcl itself. Errors thrown by bgerror itself get
    # reported to stderr.
    if {[catch {bgerror $err} msg]} {
	puts stderr "bgerror failed to handle background error."
	puts stderr "    Original error: $err"
	puts stderr "    Error in bgerror: $msg"
	flush stderr
    }
}

###############################################################################

# ::comm::commCollect --
#
#	Internal command. Called from the fileevent to read from fid
#	and append to the buffer. This continues until we get a whole
#	command, which we then invoke.
#
# Arguments:
#	chan	logical channel collecting the data
#	fid	channel handle of the socket we collect.
#
# Results:
#	None.

proc ::comm::commCollect {chan fid} {
    variable comm
    upvar #0 comm($chan,buf,$fid) data

    # Tcl8 may return an error on read after a close
    if {[catch {read $fid} nbuf] || [eof $fid]} {
	fileevent $fid readable {}		;# be safe
	commLostConn $chan $fid "target application died or connection lost"
	return
    }
    append data $nbuf

    commDebug {puts stderr "collect <$data>"}

    # If data contains at least one complete command, we will
    # be able to take off the first element, which is a list holding
    # the command.  This is true even if data isn't a well-formed
    # list overall, with unmatched open braces.  This works because
    # each command in the protocol ends with a newline, thus allowing
    # lindex and lreplace to work.
    #
    # This isn't true with Tcl8.0, which will return an error until
    # the whole buffer is a valid list.  This is probably OK, although
    # it could potentially cause a deadlock.

    while {![catch {set cmd [lindex $data 0]}]} {
	commDebug {puts stderr "cmd <$data>"}
	if {[string equal "" $cmd]} break
	if {[info complete $cmd]} {
	    set data [lreplace $data 0 0]
	    after idle \
		    [list ::comm::commExec $chan $fid $comm($chan,fids,$fid) $cmd]
	}
    }
}

# ::comm::commExec --
#
#	Internal command. Receives and executes a remote command,
#	returning the result and/or error. Unknown protocol commands
#	are silently discarded
#
# Arguments:
#	chan		logical channel collecting the data
#	fid		channel handle of the socket we collect.
#	remoteid	id of the other side.
#	buf		buffer containing the command to execute.
#
# Results:
#	None.

proc ::comm::commExec {chan fid remoteid buf} {
    variable comm

    # buffer should contain:
    #	send # {cmd}		execute cmd and send reply with serial #
    #	async # {cmd}		execute cmd but send no reply
    #	reply # {cmd}		execute cmd as reply to serial #

    # these variables are documented in the hook interface
    set cmd [lindex $buf 0]
    set ser [lindex $buf 1]
    set buf [lrange $buf 2 end]
    set buffer [lindex $buf 0]

    # Save remoteid for "comm remoteid".  This will only be valid
    # if retrieved before any additional events occur # on this channel.
    # N.B. we could have already lost the connection to remote, making
    # this id be purely informational!
    set comm($chan,remoteid) [set id $remoteid]

    commDebug {puts stderr "exec <$cmd,$ser,$buf>"}

    switch -- $cmd {
	send - async - command {}
	callback {
	    if {![info exists comm($chan,return,$ser)]} {
	        commDebug {puts stderr "No one waiting for serial \"$ser\""}
		return
	    }

	    # Decompose reply command to assure it only uses "return"
	    # with no side effects.

	    array set return {-code "" -errorinfo "" -errorcode ""}
	    set ret [lindex $buffer end]
	    set len [llength $buffer]
	    incr len -2
	    foreach {sw val} [lrange $buffer 1 $len] {
		if {![info exists return($sw)]} continue
		set return($sw) $val
	    }

	    if {[info exists comm($chan,hook,callback)]} {
		catch $comm($chan,hook,callback)
	    }

	    # this wakes up the sender
	    commDebug {puts stderr "--<<wakeup $chan $ser>>--"}

	    # the return holds the callback command
	    # string map the optional %-subs
	    set args [list -id       $id \
			  -serial    $ser \
			  -chan      $chan \
			  -code      $return(-code) \
			  -errorcode $return(-errorcode) \
			  -errorinfo $return(-errorinfo) \
			  -result    $ret \
			 ]
	    set code [catch {uplevel #0 $comm($chan,return,$ser) $args} err]
	    catch {unset comm($chan,return,$ser)}

	    # remove pending serial
	    upvar 0 comm($chan,pending,$id) pending
	    if {[info exists pending]} {
		set pos [lsearch -exact $pending [list $ser callback]]
		if {$pos != -1} {
		    set pending [lreplace $pending $pos $pos]
		}
	    }
	    if {$code} {
		commBgerror $err
	    }
	    return
	}
	reply {
	    if {![info exists comm($chan,return,$ser)]} {
	        commDebug {puts stderr "No one waiting for serial \"$ser\""}
		return
	    }

	    # Decompose reply command to assure it only uses "return"
	    # with no side effects.

	    array set return {-code "" -errorinfo "" -errorcode ""}
	    set ret [lindex $buffer end]
	    set len [llength $buffer]
	    incr len -2
	    foreach {sw val} [lrange $buffer 1 $len] {
		if {![info exists return($sw)]} continue
		set return($sw) $val
	    }

	    if {[info exists comm($chan,hook,reply)]} {
		catch $comm($chan,hook,reply)
	    }

	    # this wakes up the sender
	    commDebug {puts stderr "--<<wakeup $chan $ser>>--"}
	    set comm($chan,result,$ser) $ret
	    set comm($chan,return,$ser) [array get return]
	    return
	}
	vers {
	    set ::comm::comm($chan,vers,$id) $ser
	    return
	}
	default {
	    commDebug {puts stderr "unknown command; discard \"$cmd\""}
	    return
	}
    }

    # process eval hook now
    if {[info exists comm($chan,hook,eval)]} {
    	set err [catch $comm($chan,hook,eval) ret]
	commDebug {puts stderr "eval hook res <$err,$ret>"}
	switch $err {
	    1 {				;# error
		set done 1
	    }
	    2 - 3 {			;# return / break
		set err 0
		set done 1
	    }
	}
    }

    # exec command
    if {![info exists done]} {
	# Sadly, the uplevel needs to be in the catch to access the local
	# variables buffer and ret.  These cannot simply be global because
	# commExec is reentrant (i.e., they could be linked to an allocated
	# serial number).
	set err [catch [concat [list uplevel #0] $buffer] ret]
    }

    commDebug {puts stderr "res <$err,$ret>"}

    # The double list assures that the command is a single list when read.
    if {[string equal send $cmd] || [string equal command $cmd]} {
	# The catch here is just in case we lose the target.  Consider:
	#	comm send $other comm send [comm self] exit
	catch {
	    set return [list return -code $err]
	    # send error or result
	    if {$err == 1} {
		global errorInfo errorCode
		lappend return -errorinfo $errorInfo -errorcode $errorCode
	    }
	    lappend return $ret
	    if {[string equal send $cmd]} {
		set reply reply
	    } else {
		set reply callback
	    }
	    puts $fid [list [list $reply $ser $return]]
	    flush $fid
	}
    }

    if {$err == 1} {
	commBgerror $ret
    }
    return
}

###############################################################################
#
# Finish creating "comm" using the default port for this interp.
#

if {![info exists ::comm::comm(comm,port)]} {
    if {[string equal macintosh $tcl_platform(platform)]} {
	::comm::comm new ::comm::comm -port 0 -local 0 -listen 1
	set ::comm::comm(localhost) \
	    [lindex [fconfigure $::comm::comm(::comm::comm,socket) -sockname] 0]
	::comm::comm config -local 1
    } else {
	::comm::comm new ::comm::comm -port 0 -local 1 -listen 1
    }
}

#eof
package provide comm 4.2
