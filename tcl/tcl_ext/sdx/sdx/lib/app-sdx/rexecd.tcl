#
# Sensus Consulting Ltd (c) 1997-1999
# Matt Newman <matt@sensus.org>
#
# $Header$
#
# Simple REXEC Server
#
namespace eval rexec {
    variable debug 0
    variable state

    proc now {} {
	return [clock format [clock seconds] -format {%Y%m%d %T}]
    }
    proc log {msg {level 0}} {
	variable debug
	if {$level > $debug} return
	puts stderr "[now] REXEC: $msg"
	flush stderr
	catch {update idletasks}
    }
    proc getns {chan} {
	set data ""
	while 1 {
	    set c [read $chan 1]
	    if [string compare $c "\0"]==0 {
		return $data
	    }
	    append data $c
	}
    }
    proc putns {chan str} {
	puts -nonewline $chan "${str}\0"
	flush $chan
    }
    proc nack {chan msg} {
	puts $chan "\001${msg}"
	flush $chan
    }
    proc ack {chan} {
	puts -nonewline $chan "\0"
	flush $chan
    }
    proc accept {callback chan host port} {
	log "Accept: $chan $host $port"
    
	fconfigure $chan -buffering none -translation binary

	set rport [getns $chan]
	if {$rport != "" && $rport != 0} {
	    log "Calling back to $host.$rport"
	    set rchan [socket $host $rport]
	} else {
	    set rchan ""
	}
	set user [getns $chan]
	set pass [getns $chan]
	set args [getns $chan]
	log "user=$user pass=$pass args=>$args<"

	if {[string compare $user abort]==0} {
	    nack $chan "Bog off!"
	    close $chan
	    catch {close $rchan}
	    return
	}
	if {[catch {eval $callback [list $user $pass] $args} ret]} {
	    nack $chan $ret
	} else {
	    ack $chan
	    puts $chan $ret
	}
	close $chan
	catch {close $rchan}
    }
    proc clnt_accept {chan rchan host port} {
	variable state
	log "clnt_accept: $rchan $host $port"
	set state($chan) $port
    }
    proc connect {host user pass cmd {port 512}} {
	variable state
	set chan [socket $host $port]
	fconfigure $chan -translation binary -buffering none
	#
	# Setup control socket - sever will contact me.
	#
	#set rchan [socket -server "rexec::clnt_accept $chan" 0]
	#set rport [lindex [fconfigure $rchan -sockname] 2]
	#
	# Tell server which port I am listening on.
	#
	#putns $chan ${rport}
	# Disable callback channel
	putns $chan ""
	    
	#
	# Wait for server to connect back.
	#
	#log "waiting on $chan (rchan=$rchan)"
	#vwait [namespace which -variable state]($chan)
	#log "got reply, logging in..."
	#
	# Logon
	#
	putns $chan ${user}
	putns $chan ${pass}
	putns $chan ${cmd}
	#
	# If all is well the server will send "\0", otherwise it
	# will send an error.
	#
	log "checking status..."
	set c [read $chan 1]
	if {[string compare $c "\0"]!=0} {
	    set err [gets $chan]
	    close $chan
	    catch {close $rchan}
	    error $err
	}
	set msg [gets $chan]
	# Auto-close
	close $chan
	return $msg
	#
	#fconfigure $chan -translation auto -buffering line
	#return $chan
    }
}
proc bgerror {args} {
    tclLog ${::errorInfo}
}
proc rexec::callback {user pass args} {
    # Default callback - no security!
    eval $args
}
array set opts {
    -port 	512
    -ipaddr	0.0.0.0
}

while {[llength $argv] > 0} {
    set option [lindex $argv 0]
    if {![info exists opts($option)] || [llength $argv] == 1} {
	puts stderr "usage: rexecd ?options?"
	puts stderr "\nwhere options are any of the following:\n"
	foreach opt [lsort [array names opts -*]] {
	    puts stderr [format "\t%-15s default: %s" $opt $opts($opt)]
	}
	exit 1
    }
    set opts($option) [lindex $argv 1]
    set argv [lrange $argv 2 end]
}

set svcfd [socket -server [list rexec::accept rexec::callback] \
	-myaddr $opts(-ipaddr) $opts(-port)]

tclLog "Accepting connections on rexec://$opts(-ipaddr):$opts(-port)/"

if {![info exists tcl_service]} {
    vwait forever		;# start the Tcl event loop
}
