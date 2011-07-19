# ftpd.tcl -- Worlds Smallest FTPD?
#
# Copyright (c) 1999 Matt Newman, Jean-Claude Wippler and Equi4 Software.

package require Tcl 8.0	;# Works with all 8.x

# RFC0765, RFC0959
namespace eval ftpd {
    variable debug	1
    variable email	webmaster@[info hostname]
    variable port	8021
    variable root	/ftproot
    variable timeout	600
    variable version	0.4
    variable ident	"TclFTPD $version Server"
}
package provide ftpd ${::ftpd::version}

proc bgerror msg {
    tclLog ${::errorInfo}
}
proc ftpd::absolute {file} {
    upvar 1 cb cb
    # I wish [file normalize] (in VFS) was standard!
    set sp [file split $file]
    if {[file pathtype [lindex $sp 0]] == "relative"} {
	set nfile [eval [list file join $cb(cwd)] $sp]
	set sp [file split $nfile]
    }
    set splen [llength $sp]

    set np {}
    foreach ele $sp {
	if {$ele != ".."} {
	    if {$ele != "."} {
		lappend np $ele
	    }
	    continue
	}
	if {[llength $np]> 1} {
	    set np [lrange $np 0 [expr {[llength $np] - 2}]]
	}
    }
    # Strip ABS leader
    set np [lrange $np 1 end]
    if {[llength $np] > 0} {
	set ret [eval [list file join ${ftpd::root}] $np]
    } else {
	set ret ${ftpd::root}
    }
    #tclLog "abs: $file => $ret"
    return $ret
}
proc ftpd::relative {file} {
    set sp [file split $file]
    set rp [file split ${ftpd::root}]
    set sp [lrange $sp [llength $rp] end]
    return [eval file join / $sp]
}
proc ftpd::ls {path {short 0}} {
    if {[file isdirectory $path]} {
	set ret {}
	set list [glob -nocomplain [file join $path *] [file join $path .*]] 
	foreach file [lsort -dictionary $list] {
	    set tail [file tail $file]
	    if {$tail == "." || $tail == ".."} {continue}
	    append ret [ls1 $file $short]\n
	}
	return $ret
    } else {
	return [ls1 $path $short]
    }
}
proc ftpd::ls1 {path {short 0}} {
    if {$short} {
	return [file tail $path]
    }
    file stat $path sb

    #drwxr-xr-x    3 888      999           21 May 13 19:46 vjscdk
    return [format {%s %4d %-8s %-8s %7d %s %s} \
	[fmode sb] $sb(nlink) $sb(uid) $sb(gid) $sb(size) \
	[clock format $sb(mtime) -format {%b %d %H:%M} -gmt 1] \
	[file tail $path]]
}

proc ftpd::fmode arr { # From Richard Suchenwirth
    upvar 1 $arr sb

    if {$sb(type) == "directory"} { set pfx "d" } else { set pfx "-" }

    set s [format %o [expr $sb(mode)%512]]
    foreach i {  0   1   2   3   4   5   6   7} \
	    j {--- --x -w- -wx r-- r-x rw- rwx} {
	regsub -all $i $s $j s
    }
    return $pfx$s
}
proc ftpd::type {chan} {
    upvar #0 ftpd::$chan cb

    if {$cb(type) == "I"} { return ASCII } else { return BINARY }
}
proc ftpd::log {msg} {
    upvar 1 cb cb

    if {[info exists cb(debug)] && $cb(debug)} {
	tclLog "FTPD: $cb(rhost):$cb(rport): $msg"
    }
}
proc ftpd::reply {chan code data {cont ""}} {
    upvar #0 ftpd::$chan cb

    if {$cont == ""} {set sep " "} {set sep -}

    log "reply: $code$sep$data"

    puts $chan "$code$sep$data"
    flush $chan

    after cancel $cb(timer)
    set cb(timer) [after [expr {$cb(timeout) * 1000}] [list ftpd::timeout $chan]]
}
proc ftpd::timeout {chan} {
    upvar #0 ftpd::$chan cb
    reply $chan 421 "No Transfer Timeout ($cb(timeout)) closing control channel"
    finish $chan Timeout
}
proc ftpd::CopyDone {chan fd bytes {error ""}} {
    upvar #0 ftpd::$chan cb

tclLog "CLOSE file $fd"
    #log "Copied $bytes bytes"
    close $fd
    close-data $chan

    reply $chan 226 "Transfer complete."
}
proc ftpd::finish {chan {msg EOF}} {
    upvar #0 ftpd::$chan cb

    log "closing connection ($msg)"
    catch {after cancel $cb(timer)}
    close-data $chan

tclLog "CLOSE ctrl $chan"
    catch {close $chan}
    catch {unset cb}
}
proc ftpd::close-data {chan} {
    upvar #0 ftpd::$chan cb
    catch {flush $cb(data)}
    catch {close $cb(data)}
tclLog "CLOSE data $cb(data)"
    catch {close $cb(pasv)}
tclLog "CLOSE pasv $cb(pasv)"
    set cb(pasv) ""
    set cb(data) ""
}
proc ftpd::accept {chan ip port} {
    upvar #0 ftpd::$chan cb
    # Copy in settings - this will allow us to expand in the
    # future to tune settings based upon incomming IP or user name etc.
    set cb(debug) ${ftpd::debug}
    set cb(root) ${ftpd::root}
    set cb(email) ${ftpd::email}
    set cb(timeout) ${ftpd::timeout}

    set cb(cwd) /
    set cb(offset) 0
    set cb(type) binary
    set cb(last) ""
    set cb(pasv) ""
    set cb(data) ""
    set cb(rhost) $ip
    set cb(rport) $port
    set cb(chan) $chan
    set cb(timer) ""

    log "accept control"

    fconfigure $chan -buffering line
    fileevent $chan readable [list ftpd::handler $chan]

    reply $chan 220 "${ftpd::ident} ([info hostname])"
}
proc ftpd::accept/data {chan data ip port} {
    upvar #0 ftpd::$chan cb

    log "accept data from $ip $port"

    set cb(data) $data
    fconfigure $cb(data) -translation $cb(type)
}
proc ftpd::handler {chan} {
    upvar #0 ftpd::$chan cb

    set line [gets $chan]
    if {[eof $chan]} {
	finish $chan EOF
	return
    }
    log "request: $line"

    set op [string toupper [lindex [split $line] 0]]
    set arg [string trim [string range $line 4 end]]
    
    switch -- $op {
    SYST	{
		reply $chan 215 "UNIX Type: L8"
	    }
    NOOP	{
		reply $chan 250 "$op command successful."
	    }
    USER	{
		set cb(user) $arg
		reply $chan 331 "Password required for $cb(user)."
	    }
    PASS	{#reply $chan 530 "Login incorrect."
		reply $chan 230 "User $cb(user) logged in."
	    }
    TYPE	{
		if {$arg == "A"} {
		    set cb(type) {auto crlf}
		} else {
		    set cb(type) binary
		}
		if {$cb(data) != ""} {
		    fconfigure $cb(data) -translation $cb(type)
		}
		reply $chan 200 "Type set to $cb(type)."
	    }
    PORT	{
		# PORT IP1,IP2,IP3,IP4,PORT-HI,PORT-LO
		if {[catch {
		    regexp {([0-9]+),([0-9]+),([0-9]+),([0-9]+),([0-9]+),([0-9]+)} \
			$arg - i1 i2 i3 i4 pHi pLo
		    set ip $i1.$i2.$i3.$i4 
		    set port [expr {(256 * $pHi) + $pLo}]

		    set cb(data) [socket -async $ip $port]
tclLog "OPEN data $cb(data)"
		    fconfigure $cb(data) -translation $cb(type)
		} err]} {
		    reply $chan 550 $err
		} else {
		    reply $chan 200 "$op command successful."
		}
	    }
    PASV	{# Switch to passive mode (we listen)
		if {$cb(pasv) != ""} {
		    # This shouldn't happen
		    close-data $chan
		}
		set cb(pasv) [socket -server [list ftpd::accept/data $chan] \
				-myaddr [info hostname] 0]
tclLog "OPEN pasv $cb(pasv)"
		# XXX - This causes a NS lookup - which sucks
		set c [fconfigure $cb(pasv) -sockname]
		set ip [lindex $c 0]
		set port [lindex $c 2]
		regexp {([0-9]+).([0-9]+).([0-9]+).([0-9]+)} \
			$ip - i1 i2 i3 i4
		set pHi [expr {$port / 256}]
		set pLo [expr {$port % 256}]
		reply $chan 227 "Passive mode entered ($i1,$i2,$i3,$i4,$pHi,$pLo)"
	    }
    REST	{
		set cb(offset) $arg
		reply $chan 350 "Restarting at $cb(offset). Send STORE or RETRIEVE to initiate transfer."
	    }
    XCUP	-
    CDUP	-
    XCWD	-
    CWD		{
		if {$op == "CDUP" || $op == "XCUP"} {
		    set arg ..
		}
		if {[catch {
		    cd [absolute [file join $cb(cwd) $arg]]
		} err]} {
		    reply $chan 550 $err
		} else {
		    set cb(cwd) [relative [pwd]]
		    reply $chan 250 "$op command successful."
		}
	    }
    DELE	{
		if {[catch {
		    file delete [absolute $arg]
		} err]} {
		    reply $chan 550 $err
		} else {
		    reply $chan 257 "\"$arg\" - file successfully removed"
		}
	    }
    MDTM	{
		if {[catch {
		    file stat [absolute $arg] sb
		} err]} {
		    reply $chan 550 $err
		} elseif {$sb(type) != "file"} {
		    reply $chan 550 "$arg: not a plain file."
		} else {
		    set ts [clock format $sb(mtime) -format "%Y%m%d%H%M%S" -gmt 1]
		    reply $chan 213 $ts
		}
	    }
    SIZE	{
		if {[catch {
		    file stat [absolute $arg] sb
		} err]} {
		    reply $chan 550 $err
		} elseif {$sb(type) != "file"} {
		    reply $chan 550 "$arg: no a regular file."
		} else {
		    reply $chan 213 $sb(size)
		}
	    }
    XMKD	-
    MKD		{
		if {[catch {
		    file mkdir [absolute $arg]
		} err]} {
		    reply $chan 550 $err
		} else {
		    reply $chan 257 "\"$arg\" - directory successfully created"
		}
	    }
    XRMD	-
    RMD		{
		if {[catch {
		    file delete [absolute $arg]
		} err]} {
		    reply $chan 550 $err
		} else {
		    reply $chan 250 "$op command successful."
		}
	    }
    RNFR	{
		if {[catch {
		    file stat [absolute $arg] sb
		} err]} {
		    reply $chan 550 $err
		} else {
		    set cb(from) $arg
		    reply $chan 350 "File or directory exists, ready for destination name."
		}
	    }
    RNTO	{
		if {$cb(last) != "RNFR"} {
		    reply $chan 550 "RNTO must follow RNFR"
		} elseif {[catch {
		    file rename [absolute $cb(from)] [absolute $arg]
		} err]} {
		    reply $chan 550 $err
		} else {
		    reply $chan 200 "$op command successful."
		}
	    }
    NLST	-
    LIST	{if {$arg == ""} {set arg $cb(cwd)}
		reply $chan 150 "Opening [type $chan] mode data connection for file list."

		if {$op == "NLST"} {
		    # 550 No files found
		    catch {ls [absolute $arg] 1} ret
		} else {
		    catch {ls [absolute $arg]} ret
		}
		if {[catch {
		    puts $cb(data) $ret
		} err]} {
		    reply $chan 550 "Transfer Aborted: $err"
		} else {
		    reply $chan 226 "Transfer complete."
		}
		close-data $chan
	    }
    STAT	{# List LIST but using the control channel
		catch {ls [absolute $arg]} ret
		reply $chan 213 "status of $arg:" cont
		puts $chan $ret
		reply $chan 213 "End of Status"
	    }
    RETR	{
		if {[catch {
		    file stat [absolute $arg] sb
		    set fd [open [absolute $arg]]
tclLog "OPEN file $fd"
		    fconfigure $fd -translation binary
		    if {$cb(offset) > 0} {
			seek $fd $cb(offset)
		    }
		} err]} {
		    reply $chan 550 $err
		    close-data $chan
		} else {
		    reply $chan 150 "Opening [type $chan] mode data connection for $arg ($sb(size) bytes)."

		    fcopy $fd $cb(data) -command [list ftpd::CopyDone $chan $fd]
		}
	    }
    APPE	-
    STOR	{
		if {$op == "STOR"} { set mode w } else { set mode a+ }

		if {[catch {
		    set fd [open [absolute $arg] $mode]
tclLog "OPEN file $fd"
		    fconfigure $fd -translation binary
		} err]} {
		    reply $chan 550 $err

		    close-data $chan
		} else {
		    reply $chan 150 "Opening [type $chan] mode data connection for $arg."

		    fcopy $cb(data) $fd -command [list ftpd::CopyDone $chan $fd]
		}
	    }
    XPWD	-
    PWD		{
		reply $chan 257 "\"$cb(cwd)\" is current directory."
	    }
    QUIT	{
		reply $chan 221 "Goodbye."
		finish $chan QUIT
	    }
    HELP	{
		reply $chan 214 "The following commands are recognized (* =>'s unimplemented)." cont
		puts $chan { USER    PASS    ACCT*   CWD     XCWD    CDUP    XCUP    SMNT*}
		puts $chan { QUIT    REIN*   PORT    PASV    TYPE    STRU*   MODE*   RETR}
		puts $chan { STOR    STOU*   APPE    ALLO*   REST    RNFR    RNTO    ABOR}
		puts $chan { DELE    MDTM    RMD     XRMD    MKD     XMKD    PWD     XPWD}
		puts $chan { SIZE    LIST    NLST    SITE*   SYST    STAT    HELP    NOOP}
		reply $chan 214 "Direct comments to $cb(email)."
	    }
    default	{#reply $chan 421 "Service not available."
		reply $chan 500 "$op not supported."
	    }
    }
    set cb(last) $op
}
proc ftpd::server {args} {
    if {[llength $args] == 1} {set args [lindex $args 0]}

    package require opt

    ::tcl::OptProc _ProcessOptions [list \
	[list -debug	-int	${::ftpd::debug}	{Enable Debug Tracing}] \
	[list -email	-any	${::ftpd::email}	{FTP Support Email}] \
	[list -port	-int	${::ftpd::port}		{TCP/IP Port}] \
	[list -root	-any	${::ftpd::root}		{FTP Root Directory}] \
	[list -timeout	-int	${::ftpd::timeout}	{FTP Idle TImeout}] \
    ] {
	foreach var {debug email port root timeout} {
	    set ::ftpd::$var [set $var]
	}
    }
    eval _ProcessOptions $args

    # generates error if non-existent
    file stat ${::ftpd::root} sb

    socket -server ftpd::accept ${::ftpd::port}

    tclLog "Accepting connections on ftp://[info hostname]:${ftpd::port}/"
    tclLog "FTP Root = ${::ftpd::root}"
}

set fd [open ftpd.log w]
proc tclLog msg "puts $fd \$msg;flush $fd;puts stderr \$msg"

ftpd::server $argv

vwait foreever
exit
