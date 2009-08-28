# IMAP4 protocol pure Tcl implementation.
#
# COPYRIGHT AND PERMISSION NOTICE
# 
# Copyright (C) 2004 Salvatore Sanfilippo <antirez@invece.org>.
# 
# All rights reserved.
# 
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, and/or sell copies of the Software, and to permit persons
# to whom the Software is furnished to do so, provided that the above
# copyright notice(s) and this permission notice appear in all copies of
# the Software and that both the above copyright notice(s) and this
# permission notice appear in supporting documentation.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
# OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# HOLDERS INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL
# INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
# FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
# WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
# 
# Except as contained in this notice, the name of a copyright holder
# shall not be used in advertising or otherwise to promote the sale, use
# or other dealings in this Software without prior written authorization
# of the copyright holder.
#
# $Id: imap4.tcl,v 1.2 2004/06/22 17:47:46 mic42 Exp $

# TODO
# - option -inline for ::imap4::fetch, in order to return data as a Tcl list.
# - Idle mode
# - Async mode
# - Authentications
# - Literals on file mode
# - isableto without arguments should return the capability list.
# - fix OR in search, and implement time-related searches
# All the rest... se the RFC

package require Tcl 8.4
package require struct::list 1.4

namespace eval ::imap4 {
    
    # This is where we take state of all the IMAP connections.  
    # The following arrays are indexed with the connection channel
    # to access the per-channel information.

    # general connection state info    
    variable info
    array set info {}

    # selected mailbox info    
    variable mboxinfo
    array set mboxinfo {}

    # messages info    
    variable msginfo
    array set msginfo {}

    # inside debug mode? default is off
    variable debugmode 0

    # Debug mode? Don't use it for production! It will print debugging
    # information to standard output and run a special IMAP debug mode shell
    # on protocol error.
    
    variable debug 1
    
    # Version
    variable version "2004-03-07"

}

# imap4::open --
#
#   Open a new IMAP connection and initialize the protocol handler.
#
#   Arguments:
#       hostname    Hostname of the IMAP server to use
#       port        Port to use (defaults to 143)
#
#   Results:
#       chan        Identifier for IMAP channel
#
proc ::imap4::open {hostname {port 143}} {
    set chan [socket $hostname $port]
    fconfigure $chan -encoding binary -translation binary
    # Intialize the connection state array
    ::imap4::initinfo $chan
    # Get the banner
    ::imap4::processline $chan
    # Save the banner
    set ::imap4::info($chan,banner) [::imap4::lastline $chan]
    return $chan
}

# imap4::cleanup --
#
#   Cleanup the internal state and close the IMAP channel. 
#
#   Arguments:
#       chan    Identifier for IMAP channel
#
#   Results:
#       chan    Identifier for close IMAP channel
#
proc ::imap4::cleanup chan {
    variable info
    variable mboxinfo
    variable msginfo
    
    close $chan
    
    array unset info $chan,*
    array unset mboxinfo $chan,*
    array unset msginfo $chan,*
    
    return $chan
}

# imap4::lastcode --
#
#   Return the last error code for the IMAP channel.
#
#   Arguments:
#       chan    Identifier for IMAP channel
#
#   Results:
#       code    Last error code for the given channel
#
proc ::imap4::lastcode chan {
    variable info
    return $info($chan,lastcode)
}

# imap4::lastline --
#
#   Return the last line received from the server.
#
#   Arguments:
#       chan    Identifier for IMAP channel
#
#   Results:
#       line    Last line received from the server
#
proc ::imap4::lastline chan {
    variable info
    return $info($chan,lastline)
}

# imap4::state --
#
#   Get the current state
#
#   Arguments:
#       chan    Identifier for IMAP channel
#   
#   Results:
#       state   Current state of the channel
#
proc ::imap4::state chan {
    variable info
    return $info($chan,state)
}

# imap4::isableto --
#
#   Test for capability. Use the capability command
#   to ask the server if not already done by the user.
#
#   Arguments:
#       chan    Identifier for IMAP channel
#       capa    Capability to check
#
#   Results:
#       1/0     1 if supported, 0 otherwise
#
proc ::imap4::isableto {chan capa} {
    variable info
    if {![llength $info($chan,capability)]} {
    if {[::imap4::capability $chan]} {
        # mic42 FIXME: This looks strange, 
        # should probably be an error, as this signals the
        # capabilities request failed
        #
        return 1
    }
    }
    set capa [string toupper $capa]
    expr {[lsearch -exact $info($chan,capability) $capa] != -1}
}

# imap4::msginfo --
#
#   Get information (previously collected using fetch) from a given message.
#   If the 'info' argument is omitted or a null string, the full list
#   of information available for the given message is returned.
#
#   If the required information name is suffixed with a ? character,
#   the command requires true if the information is available, or
#   false if it is not.
#
#   Arguments:
#       chan    Identifier for IMAP channel
#       msgid   Message identifier
#       info    Type of info (optional)
#       default default value for info
#
#   Results:
#       msginfo The message info requested or 
#               list of all valid info values if info and default
#               are missing.
#
proc ::imap4::msginfo {chan msgid args} {
    variable msginfo
    
    switch -- [llength $args] {
    0 {
        set info {}
    }
    1 {
        set info [lindex $args 0]
        set use_defval 0
    }
    2 {
        set info [lindex $args 0]
        set defval [lindex $args 1]
        set use_defval 1
    }
    default {
        error "::imap4::msginfo called with bad number of arguments! Try ::imap4::msginfo channel messageid ?info? ?defaultvalue?"
    }
    }
    set info [string tolower $info]
    # Handle the missing info case
    if {![string length $info]} {
    set list [array names msginfo $chan,$msgid,*]
    set availinfo {}
    foreach l $list {
        lappend availinfo [string range $l \
        [string length $chan,$msgid,] end]
    }
    return $availinfo
    }
    if {[string index $info end] eq {?}} {
    set info [string range $info 0 end-1]
    return [info exists msginfo($chan,$msgid,$info)]
    } else {
    if {![info exists msginfo($chan,$msgid,$info)]} {
        if {$use_defval} {
        return $defval
        } else {
        error "No such information '$info' available for message id '$msgid'"
        }
    }
    return $msginfo($chan,$msgid,$info)
    }
}

# imap4::mboxinfo --
#
#   Get information on the currently selected mailbox.
#   If the 'info' argument is omitted or a null string, the full list
#   of information available for the mailbox is returned.
#
#   If the required information name is suffixed with a ? character,
#   the command requires true if the information is available, or
#   false if it is not.
#
#   Arguments:
#       chan    Identifier for IMAP channel
#       info    type of info requested, defaults to {}
#
#   Results:
#       mboxinfo    Information about the selected mailbox    
#   
proc ::imap4::mboxinfo {chan {info {}}} {
    set info [string tolower $info]
    # Handle the missing info case
    if {![string length $info]} {
    set list [array names ::imap4::mboxinfo $chan,*]
    set availinfo {}
    foreach l $list {
        lappend availinfo [string range $l \
        [string length $chan,] end]
    }
    return $availinfo
    }
    if {[string index $info end] eq {?}} {
    set info [string range $info 0 end-1]
    return [info exists ::imap4::mboxinfo($chan,$info)]
    } else {
    if {![info exists ::imap4::mboxinfo($chan,$info)]} {
        error "No such information '$info' available for the current mailbox"
    }
    return $::imap4::mboxinfo($chan,$info)
    }
}

###############################################################################
#
#   Helper procs 
#
###############################################################################

# imap4::initinfo --
#
#   Initialize the per connection info array for a new
#   IMAP connection.
#
#   Arguments:
#       chan        Identifier for IMAP channel
#
#   Results:
#       none  
proc ::imap4::initinfo chan {
    variable info
    set info($chan,curtag) 0
    set info($chan,state) NOAUTH
    set info($chan,capability) {}
    set info($chan,raise_on_NO) 1
    set info($chan,raise_on_BAD) 1
    set info($chan,idle) {}
    set info($chan,lastcode) {}
    set info($chan,lastline) {}
    set info($chan,lastrequest) {}
    return
}

###############################################################################
#
# Implementations of IMAP protocol commands.
#
#
###############################################################################

# imap4::capability --
#
#   Get capabilties, issues a
#   CAPABILITY command to the server.
#
#   Arguments:
#       chan    Identifer for IMAP channel
#    
#   Results:
#       0/1     0 if successful, 1 otherwise
#
proc ::imap4::capability chan {
    ::imap4::request $chan "CAPABILITY"
    if {[::imap4::getresponse $chan]} {
    return 1
    }
    return 0
}

# imap4::check --
#
#   CHECK. Flush to disk.
#
#   Arguments:
#       chan    Identifier for IMAP channel
#   
#   Results:
#       0/1
#   
proc ::imap4::check chan {
    ::imap4::simplecmd $chan CHECK SELECT {}
}

# imap4::close --
#
#   Close the mailbox. Permanently removes \Deleted messages and return to
#   the AUTH state.
#
#   Arguments:
#       chan    Identifier for IMAP channel
#   
#   Results:
#       0/1     
#   
#   Side Effects:
#       sets the state info
#
proc ::imap4::close chan {
    if {[::imap4::simplecmd $chan CLOSE SELECT {}]} {
    return 1
    }
    set ::imap4::info($chan,state) AUTH
    return 0
}

# imap4::create --
#
#   Create a new mailbox.
#
#   Arguments:
#       chan    Identifier for IMAP channel
#       mailbox Mailbox name
#   
#   Results:
#       0/1
#
proc ::imap4::create {chan mailbox} {
    ::imap4::simplecmd $chan CREATE {AUTH SELECT} $mailbox
}

# imap4::delete --
# 
#   Delete a mailbox
#
#   Arguments:
#       chan    Identifier for IMAP channel
#       mailbox Mailbox name
#
#   Results:
#       0/1
#
proc ::imap4::delete {chan mailbox} {
    ::imap4::simplecmd $chan DELETE {AUTH SELECT} $mailbox
}

# imap4::examine --
#
#   Read-only equivalent of SELECT, uses the
#   EXAMINE command.
#
#   Arguments:
#       chan    Identifier for IMAP channel
#       mailbox Name of the mailbox to examine,
#               defaults to INBOX.
#
#   Results:
#       0/1     0 if successful, 1 otherwise
#      
proc ::imap4::examine {chan {mailbox INBOX}} {
    ::imap4::selectmbox $chan EXAMINE $mailbox
}


# imap4::fetch --
#
#   Fetch a number of attributes from messages
#
#   Arguments:
#       chan    Identifier for IMAP channel
#       range   IMAP range to fetch
#       args    list of attributes to fetch
#   
#   Results:
#       0/1     0 if successful, 1 otherwise
#
#   Side Effects:
#       
proc ::imap4::fetch {chan range args} {
    ::imap4::requirestate $chan SELECT
    ::imap4::parserange $chan $range start end
    set items {}
    set hdrfields {}
    foreach w $args {
    switch -glob -- [string toupper $w] {
        ALL {lappend items ALL}
        BODYSTRUCTURE {lappend items BODYSTRUCTURE}
        ENVELOPE {lappend items ENVELOPE}
        FLAGS {lappend items FLAGS}
        SIZE {lappend items RFC822.SIZE}
        TEXT {lappend items RFC822.TEXT}
        HEADER {lappend items RFC822.HEADER}
        UID {lappend items UID}
        *: {
        lappend hdrfields $w
        }
        default {
        # Fixme: better to raise an error here?
        lappend hdrfields $w:
        }
    }
    }
    if {[llength $hdrfields]} {
    set item {BODY[HEADER.FIELDS (}
    foreach field $hdrfields {
        append item [string toupper [string range $field 0 end-1]] { }
    }
    set item [string range $item 0 end-1]
    append item {)]}
    lappend items $item
    }
    # Send the request
    ::imap4::request $chan "FETCH $start:$end ([join $items])"
    if {[::imap4::getresponse $chan]} {
    return 1
    }
    return 0
}

# imap4::login --
#
#   Login using the IMAP LOGIN command.
#
#   Arguments:
#       chan    Identifier for IMAP channel
#       user    Username to use for LOGIN
#       pass    Password to use for LOGIN
#
#   Results:
#       0/1     0 if successful, 1 otherwise
#
#   Side Effects:
#       move channel to AUTH state if successful
#
proc ::imap4::login {chan user pass} {
    ::imap4::requirestate $chan NOAUTH
    ::imap4::request $chan "LOGIN $user $pass"
    if {[::imap4::getresponse $chan]} {
    return 1
    }
    set ::imap4::info($chan,state) AUTH
    return 0
}

# imap4::noop --
#
#   NOOP command. May get information as untagged data.
#   Useful for keeping an IMAP connection alive.
#
#   Arguments:
#       chan    Identifier for IMAP channel
#
#   Results:
#       0/1
#   
#   Side Effects:
#       may get information as untagged data
#
proc ::imap4::noop chan {
    ::imap4::simplecmd $chan NOOP {NOAUTH AUTH SELECT} {}
}

# imap4::rename --
#
#   Rename a mailbox
#
#   Arguments:
#       chan    Identifier for IMAP channel
#       oldname Name of mailbox to rename
#       newname New name of mailbox
#
#   Results:
#       0/1
#
proc ::imap4::rename {chan oldname newname} {
    ::imap4::simplecmd $chan RENAME {AUTH SELECT} $oldname $newname
}

# imap4::search --
#
#   SEARCH command.
#
#   Arguments:
#       chan    Identifier for IMAP channel
#       args    search arguments
#
#   Results:
#       0/1
#
proc ::imap4::search {chan args} {
    if {![llength $args]} {
    error "missing arguments. Usage: ::imap4::search chan arg ?arg ...?"
    }
    ::imap4::requirestate $chan SELECT
    set imapexpr [::imap4::convert_search_expr $args]
    ::imap4::multiline_prefix_command imapexpr "SEARCH"
    ::imap4::multiline_request $chan $imapexpr
    if {[::imap4::getresponse $chan]} {
    return 1
    }
    return 0
}

# imap4::select --
#
#   Mailbox selection. Performs a SELECT command.
#   
#   Arguments:
#       chan    Identifier for IMAP channel
#       mailbox Name of the mailbox to select,
#               defaults to INBOX.
#
#   Results:
#       0/1     0 if successful, 1 otherwise
#
proc ::imap4::select {chan {mailbox INBOX}} {
    ::imap4::selectmbox $chan SELECT $mailbox
}

# imap4::subscribe --
#
#   Subscribe to a mailbox
#
#   Arguments:
#       chan    Identifier for IMAP channel
#       mailbox Name of mailbox
#   
#   Results:
#       0/1
#
proc ::imap4::subscribe {chan mailbox} {
    ::imap4::simplecmd $chan SUBSCRIBE {AUTH SELECT} $mailbox
}

# imap4::unsubscribe --
#
#   Unsubscribe to a mailbox
#
#   Arguments:
#       chan    Identifier for IMAP channel
#       mailbox Name of a mailbox
#   
#   Results:
#       0/1
#
proc ::imap4::unsubscribe {chan mailbox} {
    ::imap4::simplecmd $chan UNSUBSCRIBE {AUTH SELECT} $mailbox
}

###############################################################################
#
#   Protocol support functions
#
###############################################################################

# imap4::literalcount --
#
#   Creates an IMAP octect-count.
#   Used to send literals.
#
#   Arguments:
#       string      string to check
#   
#   Results:
#       litcount    IMAP literal octet count
#
proc ::imap4::literalcount string {
    return "{[string length $string]}"
}

# imap4::convert_search_expr --
#
#   Helper for the search command. Convert a programmer friendly expression
#   (actually a tcl list) to the IMAP syntax. Returns a list composed of
#   request, literal, request, literal, ... (to be sent with
#   ::imap4::multiline_request).
#
#   Arguments:
#       expr        Expression to use for search expression
#       
#   Results:
#       imapexpr    IMAP search expression
#
proc ::imap4::convert_search_expr expr {
    set result {}
    while {[llength $expr]} {
    switch -glob -- [string toupper [set token [::struct::list shift expr]]] {
        *: {
        set wanted [::struct::list shift expr]
        ::imap4::multiline_append_command result "HEADER [string range $token 0 end-1]"
        ::imap4::multiline_append_literal result $wanted
        }

        ANSWERED - DELETED - DRAFT - FLAGGED - RECENT -
        SEEN - NEW - OLD - UNANSWERED - UNDELETED -
        UNDRAFT - UNFLAGGED - UNSEEN -
        ALL {::imap4::multiline_append_command result [string toupper $token]}

        BODY - CC - FROM - SUBJECT - TEXT - KEYWORD -
        BCC {
        set wanted [::struct::list shift expr]
        ::imap4::multiline_append_command result "$token"
        ::imap4::multiline_append_literal result $wanted
        }

        OR {
        set first [::imap4::convert_search_expr [::struct::list shift expr]]
        set second [::imap4::convert_search_expr [::struct::list shift expr]]
        ::imap4::multiline_append_command result "OR"
        ::imap4::multiline_concat_expr result $first
        ::imap4::multiline_concat_expr result $second
        }

        NOT {
        set e [::imap4::convert_search_expr [::struct::list shift expr]]
        ::imap4::multiline_append_command result "NOT"
        ::imap4::multiline_concat_expr result $e
        }

        SMALLER -
        LARGER {
        set len [::struct::list shift expr]
        if {![string is integer $len]} {
            error "Invalid integer follows '$token' in IMAP search"
        }
        ::imap4::multiline_append_command result "$token $len"
        }

        ON - SENTBEFORE - SENTON - SENTSINCE - SINCE -
        BEFORE {error "TODO"}

        UID {error "TODO"}
        default {
        error "Syntax error in search expression: '... $token $expr'"
        }
    }
    }
    return $result
}

# imap4::multiline_append_command --
#
#   Append a command part to a multiline request
#
#   Arguments:
#       reqvar  Variable storing the request
#       cmd     command to append
#
#   Results:
#       none
#
proc ::imap4::multiline_append_command {reqvar cmd} {
    upvar 1 $reqvar req
    if {[llength $req] == 0} {
    lappend req {}
    }
    lset req end "[lindex $req end] $cmd"
}

# imap4::multiline_append_literal --
#
#   Append a literal to a multiline request. Uses a quoted
#   string in simple cases.
#
#   Arguments:
#       reqvar  Variable storing the request
#       lit     literal to append
#   
#   Results:
#       none
#
proc ::imap4::multiline_append_literal {reqvar lit} {
    upvar 1 $reqvar req
    if {![string is alnum $lit]} {
    lset req end "[lindex $req end] [::imap4::literalcount $lit]"
    lappend req $lit {}
    } else {
    ::imap4::multiline_append_command req "\"$lit\""
    }
}

# imap4::multiline_prefix_command --
#
#   Prefix a multiline request with a command.
#
#   Arguments:
#       reqvar  Variable storing the request
#       cmd     Command to prepend
#
#   Results:
#       none
#
proc ::imap4::multiline_prefix_command {reqvar cmd} {
    upvar 1 $reqvar req
    if {![llength $req]} {
    lappend req {}
    }
    lset req 0 " $cmd[lindex $req 0]"
}

# imap4::multiline_concat_expr --
#   
#   Concat an already created search expression to a multiline request.
#
#   Arguments:
#       reqvar  Variable storing the request
#       expr    expression to append
#
#   Results:
#       ?
#
proc ::imap4::multiline_concat_expr {reqvar expr} {
    upvar 1 $reqvar req
    lset req end "[lindex $req end] ([string range [lindex $expr 0] 1 end]"
    set req [concat $req [lrange $expr 1 end]]
    lset req end "[lindex $req end])"
}

# imap4::simplecmd --
#
#   This a general implementation for a simple implementation
#   of an IMAP command that just requires to call ::imap4::request
#   and ::imap4::getresponse.
#
#   Arguments:
#       chan        Identifier for IMAP channel
#       command     IMAP command
#       validstates The valid states for this command
#       args        Arguments to the IMAP command
#       
proc ::imap4::simplecmd {chan command validstates args} {
    ::imap4::requirestate $chan $validstates
    set req "$command"
    foreach arg $args {
    append req " $arg"
    }
    ::imap4::request $chan $req
    if {[::imap4::getresponse $chan]} {
    return 1
    }
    return 0
}

# imap4::selectmbox --
#
#   General function for mailbox selection.
#
#   Arguments:
#       chan    Identifier for IMAP channel
#       cmd     IMAP command to use
#       mailbox Name of mailbox to use
#
#   Results:
#       0/1     0 if successful, 1 otherwise
#
#   Side Effects:
#       move to SELECT state if successful
#
proc ::imap4::selectmbox {chan cmd mailbox} {
    ::imap4::requirestate $chan AUTH
    
    # Clean info about the previous mailbox if any,
    # but save a copy to restore this info on error.
    set savedmboxinfo [array get ::imap4::mboxinfo $chan,*]
    array unset ::imap4::mboxinfo $chan,*
    ::imap4::request $chan "$cmd $mailbox"
    if {[::imap4::getresponse $chan]} {
    array set ::imap4::mboxinfo $savedmboxinfo
    return 1
    }
    set ::imap4::info($chan,state) SELECT
    # Set the new name as mbox->current.
    set ::imap4::mboxinfo($chan,current) $mailbox
    return 0
}

# imap4::tag --
#
#   Return the next tag to use in IMAP requests.
#
#   Arguments:
#       chan    Identifier for IMAP channel
#
#   Results:
#       tag     A valid tag for an IMAP request
#
proc ::imap4::tag chan {
    incr ::imap4::info($chan,curtag)
}

# imap4::checkstate --
#
#   Check that the channel is in one of the specified states.
#
#   Arguments:
#       chan    Identifier for IMAP channel
#       states  List of states
#
#   Results:
#       bool    Either 1 or 0.
#   
proc ::imap4::checkstate {chan states} {
    expr {[lsearch -exact $states $::imap4::info($chan,state)] == -1}
}

# imap4::requirestate --
#
#   Asserts that the channel is in one of the specified states.
#
#   Arguments:
#       chan    Identifier for IMAP channel
#       states  List of states
#
#   Results:
#       none
#   
#   Side Effects:
#       An error is raised if the channel is not in one
#       of the states in the states list.
#
proc ::imap4::requirestate {chan states} {
    if {[checkstate $chan $states]} {
    error "IMAP channel not in one of the following states: '$state' (current state is '$::imap4::info($chan,state)')"
    }
}

##############################################################################
#
# procs for decoding IMAP responses and general processing
#
##############################################################################

# imap4::processline --
#
#   Process an IMAP response line.
#   This function trades simplicity in IMAP commands
#   implementation with monolitic handling of responses.
#   However note that the IMAP server can reply to a command
#   with many different untagged info, so to have the reply
#   processing centralized makes this simple to handle.
#
#   Arguments:
#       chan    Identifier for IMAP channel
#
#   Results:
#       tag     Tag found on the line
#
proc ::imap4::processline chan {
    set literals {}
    while 1 {
    # Read a line
    if {[gets $chan buf] == -1} {
        error "IMAP unexpected EOF from server."
    }
    append line $buf
    # Remove the trailing CR at the end of the line, if any.
    if {[string index $line end] eq "\r"} {
        set line [string range $line 0 end-1]
    }
    # Check if there is a literal to read, and read it if any.
    if {[regexp {{([0-9]+)}\s+$} $buf => length]} {
        # puts "Reading $length bytes of literal..."
        lappend literals [read $chan $length]
    } else {
        break
    }
    }
    set ::imap4::info($chan,lastline) $line

    if {$::imap4::debug} {
    puts "S: $line"
    }

    # Extract the tag.
    set idx [string first { } $line]
    if {$idx == -1 || $idx == 0} {
    ::imap4::protoerror $chan "IMAP: malformed response '$line'"
    }
    set tag [string range $line 0 [expr {$idx-1}]]
    set line [string range $line [expr {$idx+1}] end]
    # If it's just a command continuation response, return.
    if {$tag eq {+}} {return +}
    # Extract the error code, if it's a tagged line
    if {$tag ne {*}} {
    set idx [string first { } $line]
    if {$idx == -1 || $idx == 0} {
        ::imap4::protoerror $chan "IMAP: malformed response '$line'"
    }
    set code [string range $line 0 [expr {$idx-1}]]
    set line [string trim [string range $line [expr {$idx+1}] end]]
    set ::imap4::info($chan,lastcode) $code
    }
    # Extract information from the line
    set dirty 0
    switch -glob -- $line {
    {*\[READ-ONLY\]*} {set ::imap4::mboxinfo($chan,perm) READ-ONLY; incr dirty}
    {*\[READ-WRITE\]*} {set ::imap4::mboxinfo($chan,perm) READ-WRITE; incr dirty}
    {*\[TRYCREATE\]*} {set ::imap4::mboxinfo($chan,perm) TRYCREATE; incr dirty}
    {FLAGS *(*)*} {
        regexp {.*\((.*)\).*} $line => flags
        set ::imap4::mboxinfo($chan,flags) $flags
        incr dirty
    }
    {*\[PERMANENTFLAGS *(*)*\]*} {
        regexp {.*\[PERMANENTFLAGS \((.*)\).*\].*} $line => flags
        set ::imap4::mboxinfo($chan,permflags) $flags
        incr dirty
    }
    }
    if {!$dirty && $tag eq {*}} {
    # FIXME: match should be case insensitive.
    switch -regexp -- $line {
        {^[0-9]+\s+EXISTS} {
        regexp {^([0-9]+)\s+EXISTS} $line => ::imap4::mboxinfo($chan,exists)
        incr dirty
        }
        {^[0-9]+\s+RECENT} {
        regexp {^([0-9]+)\s+RECENT} $line => ::imap4::mboxinfo($chan,recent)
        incr dirty
        }
        {.*?\[UIDVALIDITY\s+[0-9]+?\]} {
        regexp {.*?\[UIDVALIDITY\s+([0-9]+?)\]} $line => \
            ::imap4::mboxinfo($chan,uidval)
        incr dirty
        }
        {.*?\[UNSEEN\s+[0-9]+?\]} {
        regexp {.*?\[UNSEEN\s+([0-9]+?)\]} $line => \
            ::imap4::mboxinfo($chan,unseen)
        incr dirty
        }
        {.*?\[UIDNEXT\s+[0-9]+?\]} {
        regexp {.*?\[UIDNEXT\s+([0-9]+?)\]} $line => \
            ::imap4::mboxinfo($chan,uidnext)
        incr dirty
        }
        {^[0-9]+\s+FETCH} {
        processfetchline $chan $line $literals
        incr dirty
        }
        {^CAPABILITY\s+.*} {
        regexp {^CAPABILITY\s+(.*)\s*$} $line => capstring
        set ::imap4::info($chan,capability) [split [string toupper $capstring]]
        incr dirty
        }
        {^SEARCH\s*$} {
        # Search tag without list of messages. Nothing found
        # so we set an empty list.
        set ::imap4::mboxinfo($chan,found) {}
        }
        {^SEARCH\s+.*} {
        regexp {^SEARCH\s+(.*)\s*$} $line => foundlist
        set ::imap4::mboxinfo($chan,found) $foundlist
        incr dirty
        }
        default {
        if {$::imap4::debug} {
            puts "*** WARNING: unprocessed server reply '$line'"
        }
        }
    }
    }
    if {[string length [set ::imap4::info($chan,idle)]] && $dirty} {
    # ... Notify.
    }
    # if debug and no dirty and untagged line... warning: unprocessed IMAP line
    return $tag
}

# imap4::processfetchline --
#
#   Process untagged FETCH lines.
#
#   Arguments:
#       chan        Identifier for IMAP channel
#       line        Line to process
#       literals    IMAP literals (?)
#
#   Results:
#       ?
#
#   Side Effects:
#       Fills in the msginfo
#
proc ::imap4::processfetchline {chan line literals} {
    regexp -nocase {([0-9]+)\s+FETCH\s+(\(.*\))} $line => msgnum items
    foreach {name val} [imaptotcl items literals] {
    set attribname [switch -glob -- [string toupper $name] {
        INTERNALDATE {format internaldate}
        BODYSTRUCTURE {format bodystructure}
        {BODY\[HEADER.FIELDS*\]} {format fields}
        {BODY.PEEK\[HEADER.FIELDS*\]} {format fields}
        {BODY\[*\]} {format body}
        {BODY.PEEK\[*\]} {format body}
        HEADER {format header}
        RFC822.HEADER {format header}
        RFC822.SIZE {format size}
        RFC822.TEXT {format text}
        ENVELOPE {format envelope}
        FLAGS {format flags}
        UID {format uid}
        default {
        ::imap4::protoerror $chan "IMAP: Unknown FETCH item '$name'. Upgrade the software"
        }
    }]
    switch -- $attribname {
        fields {
        set last_fieldname __garbage__
        foreach f [split $val "\n\r"] {
            # Handle multi-line headers. Append to the last header
            # if this line starts with a tab character.
            if {[string is space [string index $f 0]]} {
            append ::imap4::msginfo($chan,$msgnum,$last_fieldname) " [string range $f 1 end]"
            continue
            }
            # Process the line searching for a new field.
            if {![string length $f]} continue
            if {[set fnameidx [string first ":" $f]] == -1} {
            ::imap4::protoerror $chan "IMAP: Not a valid RFC822 field '$f'"
            }
            set fieldname [string tolower [string range $f 0 $fnameidx]]
            set last_fieldname $fieldname
            set fieldval [string trim \
            [string range $f [expr {$fnameidx+1}] end]]
            set ::imap4::msginfo($chan,$msgnum,$fieldname) $fieldval
        }
        }
        default {
            set ::imap4::msginfo($chan,$msgnum,$attribname) $val
        }
    }
    #puts "$attribname -> [string range $val 0 20]"
    }
}

# imap4::parserange --
#
#   Parse an IMAP range, store 'start' and 'end' in the
#   named vars. If the first number of the range is omitted,
#   1 is assumed. If the second number of the range is omitted,
#   the value of "exists" of the current mailbox is assumed.
#
#   So : means all the messages.
#
#   Arguments:
#       chan        Identifier for IMAP channel
#       range       range to parse
#       startvar    variable to store the start in
#       endvar      variable to store the end in
#
#   Results:
#       none        
#
#   Side Effects:
#       results are stored in startvar and endvar
#
proc ::imap4::parserange {chan range startvar endvar} {
    upvar $startvar start $endvar end
    set rangelist [split $range :]
    switch -- [llength $rangelist] {
    1 {
        if {![string is integer $range]} {
        error "Invalid range"
        }
        set start $range
        set end $range
    }
    2 {
        foreach {start end} $rangelist break
        if {![string length $start]} {
        set start 1
        }
        if {![string length $end]} {
        set end [::imap4::mboxinfo $chan exists]
        }
        if {![string is integer $start] || ![string is integer $end]} {
        error "Invalid range"
        }
    }
    default {
        error "Invalid range"
    }
    }
}

# imap4::imaptotcl --
#
#   Convert IMAP data into Tcl data. Consumes the part of the
#   string converted.
#   'literals' is a list with all the literals extracted
#   from the original line, in the same order they appeared.
#
#   Arguments:
#       datavar     variable holding the data to parse
#       literalsvar variable holding the literals
#
#   Results:
#       ?
#  
#   Side Effects:
#       consumes data from datavar
#       
proc ::imap4::imaptotcl {datavar literalsvar} {
    upvar 1 $datavar data $literalsvar literals
    set data [string trim $data]
    switch -- [string index $data 0] {
    \{ {imaptotcl_literal data literals}
    "(" {imaptotcl_list data literals}
    "\"" {imaptotcl_quoted data}
    0 - 1 - 2 - 3 - 4 - 5 - 6 - 7 - 8 - 9 {imaptotcl_number data}
    \) {imaptotcl_endlist data;# that's a trick to parse lists}
    default {imaptotcl_symbol data}
    }
}

# imap4::imaptotcl_literal --
#
#   Extract a literal
#
#   Arguments:
#       datavar     variable holding the data to parse
#       literalsvar variable holding the literals
#
#   Results:
#       ?
#
#   Side Effects:
#       consumes data from datavar
#
proc ::imap4::imaptotcl_literal {datavar literalsvar} {
    upvar 1 $datavar data $literalsvar literals
    if {![regexp {{.*?}} $data match]} {
    ::imap4::protoerror $chan "IMAP data format error: '$data'"
    }
    set data [string range $data [string length $match] end]
    set retval [lindex $literals 0]
    set literals [lrange $literals 1 end]
    return $retval
}

# imap4::imaptotcl_quoted --
#
#   Extract a quoted string
#
#   Arguments:
#       datavar     variable holding the data to parse
#   
#   Results:
#       string      The extracted string
#
#   Side Effects:
#       consumes data from datavar
#
proc ::imap4::imaptotcl_quoted datavar {
    upvar 1 $datavar data
    if {![regexp "\\s*?(\".*?\[^\\\\\]\"|\"\")\\s*?" $data => match]} {
    ::imap4::protoerror $chan "IMAP data format error: '$data'"
    }
    set data [string range $data [string length $match] end]
    return [string range $match 1 end-1]
}

# imap4::imaptotcl_number --
#
#   Extract a number
#
#   Arguments:
#       datavar     variable holding the data to parse
#   
#   Results:
#       number      An integer number
#
#   Side Effects:
#       consumes data from datavar
#
proc imaptotcl_number datavar {
    upvar 1 $datavar data
    if {![regexp {^[0-9]+} $data match]} {
    ::imap4::protoerror $chan "IMAP data format error: '$data'"
    }
    set data [string range $data [string length $match] end]
    return $match
}

# imap4::imaptotcl_symbol --
#
#   Extract a "symbol". Not really exists in IMAP, but there
#   are named items, and this names have a strange unquoted
#   syntax like BODY[HEAEDER.FIELD (From To)] and other stuff
#   like that.
#
#   Arguments:
#       datavar     variable holding the data to parse
#   
#   Results:
#       match       The symbol found
#
#   Side Effects:
#       consume data from datavar
#
proc ::imap4::imaptotcl_symbol datavar {
    upvar 1 $datavar data
    if {![regexp {([\w\.]+\[[^\[]+\]|[\w\.]+)} $data => match]} {
    ::imap4::protoerror $chan "IMAP data format error: '$data'"
    }
    set data [string range $data [string length $match] end]
    return $match
}

# imap4::imaptotcl_list --
#
#   Extract an IMAP list.
#
#   Arguments:
#       datavar     variable holding the data to parse
#       literalsvar variable holding the literals
#
#   Results:
#       ?
#
#   Side Effects:
#       consumes data from datavar
#
proc ::imap4::imaptotcl_list {datavar literalsvar} {
    upvar 1 $datavar data $literalsvar literals
    set list {}
    # Remove the first '(' char
    set data [string range $data 1 end]
    # Get all the elements of the list. May indirectly recurse called
    # by [imaptotcl].
    while {[string length $data]} {
    set ele [imaptotcl data literals]
    if {$ele eq {)}} {
        break
    }
    lappend list $ele
    }
    return $list
}

# imap4::imaptotcl_endlist --
#
#   Just extracts the ")" character alone.
#   This is actually part of the list extraction work.
#
#   Arguments:
#       datavar     variable holding the data to parse
#       
#   Results:
#       char        The character "("
#
#   Side Effects:
#       consumes data from datavar
#
proc ::imap4::imaptotcl_endlist datavar {
    upvar 1 $datavar data
    set data [string range $data 1 end]
    return ")"
}

######################################################################
#
#   procs for communication with server
#
######################################################################

# imap4::request --
#
#   Write a request to the IMAP channel.
#
#   Arguments:
#       chan    Identifier for IMAP channel
#       request Request to send
#
#   Results:
#       none
#
proc ::imap4::request {chan request} {
    set t "[::imap4::tag $chan] $request"
    if {$::imap4::debug} {
    puts "C: $t"
    }
    set ::imap4::info($chan,lastrequest) $t
    puts -nonewline $chan "$t\r\n"
    flush $chan
}

# imap4::multiline_request --
#
#   Write a multiline request. The 'request' list must contain
#   parts of command and literals interleaved. Literals are at odd
#   list positions (1, 3, ...).
#
#   Arguments:
#       chan    Identifier for IMAP channel
#       request request list
#   
#   Results:
#       none
#
proc ::imap4::multiline_request {chan request} {
    lset request 0 "[::imap4::tag $chan][lindex $request 0]"
    set items [llength $request]
    foreach {line literal} $request {
    # Send the line
    if {$::imap4::debug} {
        puts "C: $line"
    }
    puts -nonewline $chan "$line\r\n"
    flush $chan
    incr items -1
    if {!$items} break
    # Wait for the command continuation response
    if {[::imap4::processline $chan] ne {+}} {
        ::imap4::protoerror $chan "Expected a command continuation response but got '[::imap4::lastline $chan]'"
    }
    # Send the literal
    if {$::imap4::debug} {
        puts "C> $literal"
    }
    puts -nonewline $chan $literal
    flush $chan
    incr items -1
    }
    set ::imap4::info($chan,lastrequest) $request
}

# imap4::getresponse --
#
#   Process IMAP responses. If the IMAP channel is not
#   configured to raise errors on IMAP errors, returns 0
#   on OK response, otherwise 1 is returned.
#
#   Arguments:
#       chan    Identifier for IMAP channel
#   
#   Results:
#       0/1     0 for BAD/NO , 1 for OK
#
#   Side Effects:
#       may raise errors
#
proc ::imap4::getresponse chan {
    # Process lines until the tagged one.
    while {[set tag [::imap4::processline $chan]] eq {*} || $tag eq {+}} {}
    switch -- [::imap4::lastcode $chan] {
    OK {return 0}
    NO {
        if {$::imap4::info($chan,raise_on_NO)} {
        error "IMAP error: [::imap4::lastline $chan]"
        }
        return 1
    }
    BAD {
        if {$::imap4::info($chan,raise_on_BAD)} {
        ::imap4::protoerror $chan "IMAP error: [::imap4::lastline $chan]"
        }
        return 1
    }
    default {
        ::imap4::protoerror $chan "IMAP protocol error. Unknown response code '[::imap4::lastcode $chan]'"
    }
    }
}


########################################################################################
#
#   Debug and example code
#
########################################################################################



# Debug mode.
# This is a developers mode only that pass the control to the
# programmer. Every line entered is sent verbatim to the
# server (after the addition of the request identifier).
# The ::imap4::debug variable is automatically set to '1' on enter.
#
# It's possible to execute Tcl commands starting the line
# with a slash.

proc ::imap4::debugmode {chan {errormsg {None}}} {
    set ::imap4::debugmode 1
    set ::imap4::debugchan $chan
    set welcometext [list \
    "------------------------ IMAP DEBUG MODE --------------------" \
    "IMAP Debug mode usage: Every line typed will be sent" \
    "verbatim to the IMAP server prefixed with a unique IMAP tag." \
    "To execute Tcl commands prefix the line with a / character." \
    "The current debugged channel is returned by the \[me\] command." \
    "Type ! to exit" \
    "Type help for more information" \
    "Type info to see information about the connection" \
    "" \
    "Last error: '$errormsg'" \
    "IMAP library version: '$imap4::version'" \
    "" \
    ]
    foreach l $welcometext {
    puts $l
    }
    ::imap4::debugmode_info $chan
    while 1 {
    puts -nonewline "imap debug> "
    flush stdout
    gets stdin line
    if {![string length $line]} continue
    if {$line eq {!}} exit
    if {$line eq {info}} {
        ::imap4::debugmode_info $chan
        continue
    }
    if {[string index $line 0] eq {/}} {
        catch {eval [string range $line 1 end]} result
        puts $result
    } else {
        ::imap4::request $chan $line
        if {[catch {::imap4::getresponse $chan} error]} {
        puts "--- ERROR ---\n$error\n-------------\n"
        }
    }
    }
}

# Little helper for debugmode command.
proc ::imap4::debugmode_info chan {
    puts "Last sent request: '$imap4::info($chan,lastrequest)'"
    puts "Last received line: '$imap4::info($chan,lastline)'"
    puts ""
}

# Protocol error! Enter the debug mode if ::imap4::debug is true.
# Otherwise just raise the error.
proc ::imap4::protoerror {chan msg} {
    if {$::imap4::debug && !$::imap4::debugmode} {
    ::imap4::debugmode $chan $msg
    } else {
    error $msg
    }
}

proc ::imap4::me {} {
    set ::imap4::debugchan
}

# Other stuff to do in random order...
#
# proc ::imap4::idle notify-command
# proc ::imap4::auth plain ...
# proc ::imap4::securestauth user pass
# proc ::imap4::store
# proc ::imap4::logout (need to clean both msg and mailbox info arrays)
# proc ::imap4::create
# proc ::imap4::delete
# proc ::imap4::list
# ::imap4::search $chan or {flags {seen flagged}} {larger 1000}
# ::imap4::search $chan from: antirez to: ...

################################################################################
# Example
################################################################################

set ::imap4::debug 0
if {[llength $argv] < 3} {
    puts "Usage: imap4.tcl <servername> <username> <password> ?-debugmode?"
    exit
}
if {[llength $argv] > 3} {
    est ::imap4::debug 1
}
foreach {servername username password} $argv break

# Star the connection and select the INBOX mailbox
set imap [::imap4::open $servername]
::imap4::login $imap $username $password
::imap4::select $imap INBOX

# Output all the information about that mailbox
foreach info [::imap4::mboxinfo $imap] {
    puts "$info -> [::imap4::mboxinfo $imap $info]"
}

# Fetch from: to: and size for all the messages
::imap4::fetch $imap : from: to: size header bodystructure

# Show they
for {set i 1} {$i <= [::imap4::mboxinfo $imap exists]} {incr i} {
    puts "$i) To: [::imap4::msginfo $imap $i to: {No To: field}]"
    set bstruct [::imap4::msginfo $imap $i bodystructure]
    if {[string toupper [lindex $bstruct 0]] eq {TEXT}} {
    set bstruct [list $bstruct]
    }
    foreach entry $bstruct {
    puts "\t$entry"
    }
}

# Show all the information available about the message ID 1
puts "Available info about message 1: [::imap4::msginfo $imap 1]"

# Use the capability stuff
::imap4::capability $imap
puts "Is able to idle? [::imap4::isableto $imap idle]"
puts "Is able to jump? [::imap4::isableto $imap jump]"
puts "Is able to imap4rev1? [::imap4::isableto $imap imap4rev1]"

# Show the search feature.
::imap4::search $imap larger 4000 seen
puts "Found messages: [::imap4::mboxinfo $imap found]"

# Enter the debug mode for fun or development time
::imap4::debugmode $imap

# Cleanup
::imap4::cleanup $imap
