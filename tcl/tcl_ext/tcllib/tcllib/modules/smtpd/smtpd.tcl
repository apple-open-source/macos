# smtpd.tcl - Copyright (C) 2001 Pat Thoyts <patthoyts@users.sourceforge.net>
#
# This provides a minimal implementation of the Simple Mail Tranfer Protocol
# as per RFC821 and RFC2821 (http://www.normos.org/ietf/rfc/rfc821.txt) and
# is designed for use during local testing of SMTP client software.
#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the file 'license.terms' for
# more details.
# -------------------------------------------------------------------------

package require Tcl 8.3;                # tcl minimum version
package require log;                    # tcllib
package require mime;                   # tcllib

namespace eval ::smtpd {
    variable rcsid {$Id: smtpd.tcl,v 1.12.2.1 2004/06/18 04:43:09 andreas_kupries Exp $}
    variable version 1.2.2
    variable stopped

    namespace export start stop

    variable commands {EHLO HELO MAIL RCPT DATA RSET NOOP QUIT}
    # non-minimal commands HELP VRFY EXPN VERB ETRN DSN 

    variable options
    if {! [info exists options]} {
        array set options {
            serveraddr         {}
            deliverMIME        {}
            deliver            {}
            validate_host      {}
            validate_sender    {}
            validate_recipient {}
        }
    }

    variable extensions
    if {! [info exists extensions]} {
        array set extensions {
            8BITMIME {}
            SIZE     0
        }
    }
}

# -------------------------------------------------------------------------
# Description:
#   Obtain configuration options for the server.
#
proc ::smtpd::cget {option} {
    variable options
    set optname [string trimleft $option -]
    if { [info exists options($optname)] } {
        return $options($optname)
    } else {
        return -code error "unknown option: must be one of \
                            \"[array names options]\""
    }
}

# -------------------------------------------------------------------------
# Description:
#   Configure server options. These include validation of hosts or users
#   and a procedure to handle delivery of incoming mail. The -deliver
#   procedure must handle mail because the server may release all session
#   resources once the deliver proc has completed.
#   An example might be to exec procmail to deliver the mail to users.
#
proc ::smtpd::configure {args} {
    variable options

    if {[llength $args] == 0} {
        foreach {opt value} [array get options] {
            lappend r -$opt $value

        }
        return $r
    }

    foreach {opt value} $args {
        switch -- $opt {
            -deliverMIME        {set options(deliverMIME) $value}
            -deliver            {set options(deliver) $value}
            -validate_host      {set options(validate_host) $value}
            -validate_sender    {set options(validate_sender) $value}
            -validate_recipient {set options(validate_recipient) $value}
            default {
                error "unknown option: \"$opt\": must be one of \
                       -deliverMIME, -deliver,\
                       -validate_host, -validate_recipient \
                       or -validate_sender"
            }
        }
    }
    return {}
}

# -------------------------------------------------------------------------
# Description:
#   Start the server on the given interface and port.
#
proc ::smtpd::start {{myaddr {}} {port 25}} {
    variable options
    variable stopped
    
    if {[info exists options(socket)]} {
        error "smtpd service already running on socket $options(socket)"
    }

    if {$myaddr != {}} {
        set options(serveraddr) $myaddr
        set myaddr "-myaddr $myaddr"
    } else {
        if {$options(serveraddr) == {}} {
            set options(serveraddr) [info hostname]
        }
    }

    set options(socket) [eval socket \
                             -server [namespace current]::accept $myaddr $port]
    set stopped 0
    log::log notice "smtpd service started on $options(socket)"
    return $options(socket)
}

# -------------------------------------------------------------------------
# Description:
#  Stop a running server. Do nothing if the server isn't running.
#
proc ::smtpd::stop {} {
    variable options
    variable stopped
    if {[info exists options(socket)]} {
        close $options(socket)
        set stopped 1
        log::log notice "smtpd service stopped"
        unset options(socket)
    }
}

# -------------------------------------------------------------------------
# Description:
#   Accept a new connection and setup a fileevent handler to process the new
#   session. Performs a host id validation step before allowing access.
#
proc ::smtpd::accept {channel client_addr client_port} {
    variable options
    variable version
    upvar [namespace current]::state_$channel State

    # init state array
    catch {unset State}
    initializeState $channel
    set State(access) allowed
    set State(client_addr) $client_addr
    set State(client_port) $client_port
    set accepted true

    # configure the data channel
    fconfigure $channel -buffering line -translation crlf -encoding ascii
    fileevent $channel readable [list [namespace current]::service $channel]

    # check host access permissions
    if {[cget -validate_host] != {}} {
        if {[catch {eval [cget -validate_host] $client_addr} msg] } {
            log::log notice "access denied for $client_addr:$client_port: $msg"
            Puts $channel "550 Access denied: $msg"
            set State(access) denied
            set accepted false
        }
    }
    
    if {$accepted} {
        # Accept the connection
        log::log notice "connect from $client_addr:$client_port on $channel"
        Puts $channel "220 $options(serveraddr) tcllib smtpd $version; [timestamp]"
    }
    
    return
}

# -------------------------------------------------------------------------
# Description:
#   Initialize the channel state array. Called by accept and RSET.
#
proc ::smtpd::initializeState {channel} {
    upvar [namespace current]::state_$channel State
    set State(indata) 0
    set State(to) {}
    set State(from) {}
    set State(data) {}
    set State(options) {}
}

# -------------------------------------------------------------------------
# Description:
#   Access the state of a connected session using the channel name as part
#   of the state array name. Called with no value, it returns the current
#   value of the item (or {} if not defined).
#
proc ::smtpd::state {channel args} {
    if {[llength $args] == 0} {
        return [array get [namespace current]::state_$channel]
    }

    set arrname [namespace current]::[subst state_$channel]

    if {[llength $args] == 1} {
        set r {}
        if {[info exists [subst $arrname]($args)]} {
            # FRINK: nocheck
            set r [set [subst $arrname]($args)]
        }
        return $r
    }

    foreach {name value} $args {
        # FRINK: nocheck
        set [namespace current]::[subst state_$channel]($name) $value
    }
    return {}
}

# -------------------------------------------------------------------------
# Description:
#   Safe puts.
#   If the client closes the channel, then puts will throw an error. Lets
#   terminate the session if this occurs.
proc ::smtpd::Puts {channel args} {
    if {[catch {uplevel puts $channel $args} msg]} {
        log::log error $msg
        catch {
            close $channel
            # FRINK: nocheck
            unset -- [namespace current]::state_$channel
        }
    }
    return $msg
}

# -------------------------------------------------------------------------
# Description:
#   Perform the chat with a connected client. This procedure accepts input on
#   the connected socket and executes commands according to the state of the
#   session.
#
proc ::smtpd::service {channel} {
    variable commands
    variable options
    upvar [namespace current]::state_$channel State

    if {[eof $channel]} {
        close $channel
        return
    }

    if {[catch {gets $channel cmdline} msg]} {
        close $channel
        log::log error $msg
        return
    }

    if { $cmdline == "" && [eof $channel] } {
        log::log warning "client has closed the channel"
        return
    }

    log::log debug "received: $cmdline"

    # If we are handling a DATA section, keep looking for the end of data.
    if {$State(indata)} {
        if {$cmdline == "."} {
            set State(indata) 0
            fconfigure $channel -translation crlf
            if {[catch {deliver $channel} err]} {
                # permit delivery handler to return SMTP errors in errorCode
                if {[regexp {\d{3}} $::errorCode]} {
                    Puts $channel "$::errorCode $err"
                } else {
                    Puts $channel "554 Transaction failed: $err"
                }
            } else {
                Puts $channel "250 [state $channel id]\
                        Message accepted for delivery"
            }
        } else {
            # RFC 2821 section 4.5.2: Transparency
            if {[string match {..*} $cmdline]} {
                set cmdline [string range $cmdline 1 end]
            }
            lappend State(data) $cmdline
        }
        return
    }

    # Process SMTP commands (case insensitive)
    set cmd [string toupper [lindex [split $cmdline] 0]]
    if {[lsearch $commands $cmd] != -1} {
        if {[info proc $cmd] == {}} {
            Puts $channel "500 $cmd not implemented"
        } else {
            # If access denied then client can only issue QUIT.
            if {$State(access) == "denied" && $cmd != "QUIT" } {
                Puts $channel "503 bad sequence of commands"
            } else {
                set r [eval $cmd $channel [list $cmdline]]
            }
        }
    } else {
        Puts $channel "500 Invalid command"
    }

    return
}

# -------------------------------------------------------------------------
# Description:
#  Generate a random ASCII character for use in mail identifiers.
#
proc ::smtpd::uidchar {} {
    set c .
    while {! [string is alnum $c]} {
        set n [expr {int(rand() * 74 + 48)}]
        set c [format %c $n]
    }
    return $c
}

# Description:
#  Generate a unique random identifier using only ASCII alphanumeric chars.
#
proc ::smtpd::uid {} {
    set r {}
    for {set cn 0} {$cn < 12} {incr cn} {
        append r [uidchar]
    }
    return $r
}

# -------------------------------------------------------------------------
# Description:
#   Calculate the local offset from GMT in hours for use in the timestamp
#
proc ::smtpd::gmtoffset {} {
    set now [clock seconds]
    set local [clock format $now -format "%j %H" -gmt false]
    set zulu  [clock format $now -format "%j %H" -gmt true]
    set lh [expr {([scan [lindex $local 0] %d] * 24) \
                      + [scan [lindex $local 1] %d]}]
    set zh [expr {([scan [lindex $zulu 0] %d] * 24) \
                      + [scan [lindex $zulu 1] %d]}]
    set off [expr {$lh - $zh}]
    set off [format "%+03d00" $off]
    return $off
}

# -------------------------------------------------------------------------
# Description:
#   Generate a standard SMTP compliant timestamp. That is a local time but with
#   the timezone represented as an offset.
#
proc ::smtpd::timestamp {} {
    set ts [clock format [clock seconds] \
                -format "%a, %d %b %Y %H:%M:%S" -gmt false]
    append ts " " [gmtoffset]
    return $ts
}

# -------------------------------------------------------------------------
# Description:
#   Get the servers ip address (from http://purl.org/mini/tcl/526.html)
#
proc ::smtpd::server_ip {} {
    set me [socket -server xxx -myaddr [info hostname] 0]
    set ip [lindex [fconfigure $me -sockname] 0]
    close $me
    return $ip
}

# -------------------------------------------------------------------------
# Description:
#   deliver is called once a mail transaction is completed and there is
#   no deliver procedure defined
#   The configured -deliverMIME procedure is called with a MIME token.
#   If no such callback is defined then try the -deliver option and use
#   the old API.
#
proc ::smtpd::deliver {channel} {
    set deliverMIME [cget deliverMIME]
    if { $deliverMIME != {} \
            && [state $channel from] != {} \
            && [state $channel to] != {} \
            && [state $channel data] != {} } {
        
        # create a MIME token from the mail message.        
        set tok [mime::initialize -string \
                [join [state $channel data] "\n"]]
#        mime::setheader $tok "From" [state $channel from]
#        foreach recipient [state $channel to] {
#            mime::setheader $tok "To" $recipient -mode append
#        }
        
        # catch and rethrow any errors.
        set err [catch {$deliverMIME $tok} msg]
        mime::finalize $tok -subordinates all
        if {$err} {
            log::log debug "error in deliver: $msg"
            return -code error -errorcode $::errorCode \
                    -errorinfo $::errorInfo $msg
        }        
        
    } else {
        # Try the old interface
        deliver_old $channel
    }
}

# -------------------------------------------------------------------------
# Description:
#   Deliver is called once a mail transaction is completed (defined as the
#   completion of a DATA command). The configured -deliver procedure is called
#   with the sender, list of recipients and the text of the mail.
#
proc ::smtpd::deliver_old {channel} {
    set deliver [cget deliver]
    if { $deliver != {} \
             && [state $channel from] != {} \
             && [state $channel to] != {} \
             && [state $channel data] != {} } {
        if {[catch {$deliver [state $channel from] \
                        [state $channel to] \
                        [state $channel data]} msg]} {
            log::log debug "error in deliver: $msg"
            return -code error -errorcode $::errorCode \
                    -errorinfo $::errorInfo $msg
        }
    }
}

# -------------------------------------------------------------------------
proc ::smtpd::split_address {address} {
    set start [string first < $address]
    set end [string last > $address]
    set addr [string range $address $start $end]
    incr end
    set opts [string trim [string range $address $end end]]
    return [list $addr $opts]
}

# -------------------------------------------------------------------------
# The SMTP Commands
# -------------------------------------------------------------------------
# Description:
#   Initiate an SMTP session
# Reference:
#   RFC2821 4.1.1.1
#
proc ::smtpd::HELO {channel line} {
    variable options

    if {[state $channel domain] != {}} {
        Puts $channel "503 bad sequence of commands"
        log::log debug "HELO received out of sequence."
        return
    }

    set r [regexp -nocase {^HELO\s+([-\w\.]+)\s*$} $line -> domain]
    if {$r == 0} {
        Puts $channel "501 Syntax error in parameters or arguments"
        log::log debug "HELO received \"$line\""
        return
    }
    Puts $channel "250 $options(serveraddr) Hello $domain\
                     \[[state $channel client_addr]\], pleased to meet you"
    state $channel domain $domain
    log::log debug "HELO on $channel from $domain"
    return
}

# -------------------------------------------------------------------------
# Description:
#   Initiate an ESMTP session
# Reference:
#   RFC2821 4.1.1.1
proc ::smtpd::EHLO {channel line} {
    variable options
    variable extensions

    if {[state $channel domain] != {}} {
        Puts $channel "503 bad sequence of commands"
        log::log debug "EHLO received out of sequence."
        return
    }

    set r [regexp -nocase {^EHLO\s+([-\w\.]+)\s*$} $line -> domain]
    if {$r == 0} {
        Puts $channel "501 Syntax error in parameters or arguments"
        log::log debug "EHLO received \"$line\""
        return
    }
    Puts $channel "250-$options(serveraddr) Hello $domain\
                     \[[state $channel client_addr]\], pleased to meet you"
    foreach {extn opts} [array get extensions] {
        Puts $channel [string trimright "250-$extn $opts"]
    }
    Puts $channel "250 Ready for mail."
    state $channel domain $domain
    log::log debug "EHLO on $channel from $domain"
    return
}

# -------------------------------------------------------------------------
# Description:
# Reference:
#   RFC2821 4.1.1.2
#
proc ::smtpd::MAIL {channel line} {
    set r [regexp -nocase {^MAIL FROM:\s*(.*)} $line -> from]
    if {$r == 0} {
        Puts $channel "501 Syntax error in parameters or arguments"
        log::log debug "MAIL received \"$line\""
        return
    }
    if {[catch {
        set from [split_address $from]
        set opts [lindex $from 1]
        set from [lindex $from 0]
        eval array set addr [mime::parseaddress $from]
    } msg]} {
        set addr(error) $msg
    }
    if {$addr(error) != {} } {
        log::log debug "MAIL failed $addr(error)"
        Puts $channel "501 Syntax error in parameters or arguments"
        return
    }

    if {[cget -validate_sender] != {}} {
        if {[catch {eval [cget -validate_sender] $addr(address)}]} {
            # this user has been denied
            log::log info "MAIL denied user $addr(address)"
            Puts $channel "553 Requested action not taken:\
                            mailbox name not allowed"
            return
        }
    }

    log::log debug "MAIL FROM: $addr(address)"
    state $channel from $from
    state $channel options $opts
    Puts $channel "250 OK"
    return
}

# -------------------------------------------------------------------------
# Description:
#   Specify a recipient for this mail. This command may be executed multiple
#   times to contruct a list of recipients. If a -validate_recipient 
#   procedure is configured then this is used. An error from the validation
#   procedure indicates an invalid or unacceptable mailbox.
# Reference:
#   RFC2821 4.1.1.3
# Notes:
#   The postmaster mailbox MUST be supported. (RFC2821: 4.5.1)
#
proc ::smtpd::RCPT {channel line} {
    set r [regexp -nocase {^RCPT TO:\s*(.*)} $line -> to]
    if {$r == 0} {
        Puts $channel "501 Syntax error in parameters or arguments"
        log::log debug "RCPT received \"$line\""
        return
    }
    if {[catch {
        set to [split_address $to]
        set opts [lindex $to 1]
        set to [lindex $to 0]
        eval array set addr [mime::parseaddress $to]
    } msg]} {
        set addr(error) $msg
    }

    if {$addr(error) != {}} {
        log::log debug "RCPT failed $addr(error)"
        Puts $channel "501 Syntax error in parameters or arguments"
        return
    }

    if {[string match -nocase "postmaster" $addr(local)]} {
        # we MUST support this recipient somehow as mail.
        log::log notice "RCPT to postmaster"
    } else {
        if {[cget -validate_recipient] != {}} {
            if {[catch {eval [cget -validate_recipient] $addr(address)}]} {
                # this recipient has been denied
                log::log info "RCPT denied mailbox $addr(address)"
                Puts $channel "553 Requested action not taken:\
                            mailbox name not allowed"
                return
            }
        }
    }

    log::log debug "RCPT TO: $addr(address)"
    set recipients {}
    catch {set recipients [state $channel to]}
    lappend recipients $to
    state $channel to $recipients
    Puts $channel "250 OK"
    return
}

# -------------------------------------------------------------------------
# Description:
#   Begin accepting data for the mail payload. A line containing a single 
#   period marks the end of the data and the server will then deliver the
#   mail. RCPT and MAIL commands must have been executed before the DATA
#   command.
# Reference:
#   RFC2821 4.1.1.4
# Notes:
#   The DATA section is the only part of the protocol permitted to use non-
#   ASCII characters and non-CRLF line endings and some clients take
#   advantage of this. Therefore we change the translation option on the
#   channel and reset it once the DATA command is completed. See the
#   'service' procedure for the handling of DATA lines.
#   We also insert trace information as per RFC2821:4.4
#
proc ::smtpd::DATA {channel line} {
    variable version
    upvar [namespace current]::state_$channel State
    log::log debug "DATA"
    if { $State(from) == {}} {
        Puts $channel "503 bad sequence: no sender specified"
    } elseif { $State(to) == {}} {
        Puts $channel "503 bad sequence: no recipient specified"
    } else {
        Puts $channel "354 Enter mail, end with \".\" on a line by itself"
        set State(id) [uid]
        set State(indata) 1

        lappend trace "Return-Path: $State(from)"
        lappend trace "Received: from [state $channel domain]\
                   \[[state $channel client_addr]\]"
        lappend trace "\tby [info hostname] with tcllib smtpd ($version)\
                   id $State(id); [timestamp]"
        set State(data) $trace
        fconfigure $channel -translation auto ;# naughty: RFC2821:2.3.7
    }
    return
}

# -------------------------------------------------------------------------
# Description:
#   Reset the server state for this connection.
# Reference:
#   RFC2821 4.1.1.5
#
proc ::smtpd::RSET {channel line} {
    upvar [namespace current]::state_$channel State
    log::log debug "RSET on $channel"
    if {[catch {initializeState $channel} msg]} {
        log::log warning "RSET: $msg"
    }
    Puts $channel "250 OK"
    return
}

# -------------------------------------------------------------------------
# Description:
#   Verify the existence of a mailbox on the server
# Reference:
#   RFC2821 4.1.1.6
#
#proc ::smtpd::VRFY {channel line} {
#    # VRFY SP String CRLF
#}

# -------------------------------------------------------------------------
# Description:
#   Expand a mailing list.
# Reference:
#   RFC2821 4.1.1.7
#
#proc ::smtpd::EXPN {channel line} {
#    # EXPN SP String CRLF
#}

# -------------------------------------------------------------------------
# Description:
#   Return a help message.
# Reference:
#   RFC2821 4.1.1.8
#
#proc ::smtpd::HELP {channel line} {
#    # HELP SP String CRLF
#}

# -------------------------------------------------------------------------
# Description:
#   Perform no action.
# Reference:
#   RFC2821 4.1.1.9
#
proc ::smtpd::NOOP {channel line} {
    set str {}
    regexp -nocase {^NOOP (.*)$} -> str
    log::log debug "NOOP: $str"
    Puts $channel "250 OK"
    return
}

# -------------------------------------------------------------------------
# Description:
#   Terminate a session and close the transmission channel.
# Reference:
#   RFC2821 4.1.1.10
# Notes:
#   The server is only permitted to close the channel once it has received 
#   a QUIT message.
#
proc ::smtpd::QUIT {channel line} {
    variable options
    upvar [namespace current]::state_$channel State

    log::log debug "QUIT on $channel"
    Puts $channel "221 $options(serveraddr) Service closing transmission channel"
    close $channel
        
    # cleanup the session state array.
    unset State
    return
}

package provide smtpd $smtpd::version

# -------------------------------------------------------------------------
# Local variables:
#   mode: tcl
#   indent-tabs-mode: nil
# End:
