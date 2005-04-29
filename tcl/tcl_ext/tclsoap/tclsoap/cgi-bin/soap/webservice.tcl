# webservice.tcl - Copyright (C) 2001 Pat Thoyts <Pat.Thoyts@bigfoot.com>
#
#
#
# -------------------------------------------------------------------------
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the accompanying file `LICENSE'
# for more details.
# -------------------------------------------------------------------------
#
# @(#)$Id: webservice.tcl,v 1.1 2001/08/03 21:36:42 patthoyts Exp $

package require SOAP
package require rpcvar
namespace import -force rpcvar::*

set scriptdir  webservice.scripts
set admindir   webservice.admin
set lockfile   [file join $admindir webservice.lock]
set userfile   [file join $admindir webservice.users]
set actionfile [file join $admindir webservice.actions]
set actionmap  [file join $admindir webservice.map]

namespace eval urn:tclsoap:webservices {

    SOAP::export register unregister save read

    proc register {email passwd action} {
	global usertbl
	global useract
	global actiontbl

	auth::map_lock
	set failed [catch {
	    auth::map_parse
	    if {! [info exists usertbl($email)]} {
		# new user
		if {[info exists actiontbl($action)]} {
		    error "registration failed: \
			    SOAPAction \"$action\" has already been registered" \
			    {} Client
		}
		set usertbl($email) $passwd
		set actiontbl($action) [auth::safeaction $action]
		set useract($email) $action
		
	    } else {
		# registered user - new action
		auth::authorise $email $passwd new
		if {$action == {}} {
		    error "registration failed: \
			    SOAPAction \"\" is not permitted." () Client
		}
		if {[info exists actiontbl($action)]} {
		    error "registration failed: \
			    SOAPAction \"$action\" has already been registered" \
			    {} Client
		}
		set actiontbl($action) [auth::safeaction $action]
		lappend useract($email) $action
	    }
	    auth::map_update
	} err]
	auth::map_unlock
	if {$failed} {
	    return -code error $err
	}
	return [rpcvar boolean true]
    }

    proc save {email passwd action filedata} {
	auth::map_lock
	auth::map_parse
	auth::map_unlock
	auth::authorise $email $passwd $action
	set filename [auth::actionfile $action]
	set f [open [file join $::scriptdir $filename] w]
	puts $f $filedata
	close $f
	return [rpcvar boolean true]
    }

    proc read {email passwd action} {
	auth::map_lock
	auth::map_parse
	auth::map_unlock
	auth::authorise $email $passwd $action
	set filename [auth::actionfile $action]
	set f [open [file join $::scriptdir $filename] r]
	set data [::read $f]
	close $f
	return $data
    }

    proc unregister {email passwd action} {
	error "not implemented" {} Server
    }
    
    namespace eval auth {

	proc authorise {email passwd action} {
	    global usertbl
	    global useract
	    if {! [info exists usertbl($email)]} {
		error "authorisation failed: \
			invalid username or password" {} Client
	    }
	    if {! [string match $usertbl($email) $passwd]} {
		error "authorisation failed: \
			invalid username or password" {} Client
	    }
	    if {$action != "new"} {
		if {[lsearch -exact $useract($email) $action] == -1} {
		    error "authorisation failed: \
			    action not registered to your account." {} Client
		}
	    }
	    return
	}

	proc actionfile {action} {
	    global actiontbl
	    if {! [info exists actiontbl($action)]} {
		error "invalid SOAPAction specified:\
			\"$action\" is not a registered namespace." {} Client
	    }
	    return $actiontbl($action)
	}

	proc safeaction {action} {
	    set name {}
	    set action [string map {: _ / ^} $action]
	    foreach c [split $action {}] {
		if {[string is wordchar $c] || [string match {[-_^.]} $c]} {
		    append name $c
		}
	    }
	    return $name
	}

	proc map_lock {} {
	    global lockfile
	    for {set try 0} {$try < 10} {incr try} {
		set failed [catch {open $lockfile {WRONLY EXCL CREAT}} lock]
		if {! $failed} {
		    puts $lock [pid]
		    close $lock
		    return
		}
		after 100
	    }
	    error "failed to obtain lock: please try again." {} Client
	}

	proc map_unlock {} {
	    global lockfile
	    catch {file delete $lockfile}
	}

	proc map_parse {} {
	    map_parse_userfile
	    map_parse_actionfile
	}
	
	proc map_parse_userfile {} {
	    global userfile
	    global usertbl
	    if {! [file exists $userfile]} {
		set f [open $userfile {RDONLY CREAT}]
	    } else {
		set f [open $userfile r]
	    }

	    while {! [eof $f]} {
		if {[gets $f line] > 0} {
		    set line [split $line "\t"]
		    set usertbl([lindex $line 0]) [lindex $line 1]
		}
	    }
	    close $f
	}

	proc map_parse_actionfile {} {
	    global actionfile
	    global useract
	    global actiontbl
	    if {! [file exists $actionfile]} {
		set f [open $actionfile {RDONLY CREAT}]
	    } else {
		set f [open $actionfile r]
	    }

	    while {! [eof $f]} {
		if {[gets $f line] > 0} {
		    set line [split $line "\t"]
		    set useract([lindex $line 0]) [lindex $line 1]
		    foreach action [lindex $line 1] {
			set actiontbl($action) [safeaction $action]
		    }
		}
	    }
	    close $f
	}

	proc map_update {} {
	    global userfile
	    global actionfile
	    global actionmap
	    global usertbl
	    global useract

	    set f [open $userfile w]
	    foreach email [array names usertbl] {
		puts $f "$email\t$usertbl($email)"
	    }
	    close $f

	    set f [open $actionfile w]
	    set g [open $actionmap w]
	    foreach email [array names useract] {
		puts $f "$email\t$useract($email)"
		foreach action $useract($email) {
		    puts $g "$action\t[safeaction $action]\t$email"
		}
	    }
	    close $g
	    close $f

	    return
	}
    }
}    

#
# Local variables:
# mode: tcl
# End:
