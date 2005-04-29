# urn-scheme.tcl - Copyright (C) 2001 Pat Thoyts <Pat.Thoyts@bigfoot.com>
#
# extend the uri package to deal with URN (RFC 2141)
# see http://www.normos.org/ietf/rfc/rfc2141.txt
#
# Released under the tcllib license.
#
# $Id: urn-scheme.tcl,v 1.7 2004/01/15 06:36:14 andreas_kupries Exp $
# -------------------------------------------------------------------------

package provide uri::urn 1.0.1
package require uri      1.1.2

namespace eval ::uri {}
namespace eval ::uri::urn {}

::uri::register {urn URN} {
	variable NIDpart {[a-zA-Z0-9][a-zA-Z0-9-]{0,31}}
        variable esc {%[0-9a-fA-F]{2}}
        variable trans {a-zA-Z0-9$_.+!*'(,):=@;-}
        variable NSSpart "($esc|\[$trans\])+"
        variable URNpart "($NIDpart):($NSSpart)"
        variable schemepart $URNpart
	variable url "urn:$NIDpart:$NSSpart"
}

# -------------------------------------------------------------------------

# Description:
#   Called by uri::split with a url to split into its parts.
#
proc ::uri::SplitUrn {uri} {
    #@c Split the given uri into then URN component parts
    #@a uri: the URI to split without it's scheme part.
    #@r List of the component parts suitable for 'array set'

    upvar \#0 [namespace current]::urn::URNpart pattern
    array set parts {nid {} nss {}}
    if {[regexp -- ^$pattern $uri -> parts(nid) parts(nss)]} {
        return [array get parts]
    } else {
        error "invalid urn syntax: \"$uri\" could not be parsed"
    }
}


# -------------------------------------------------------------------------

proc ::uri::JoinUrn args {
    #@c Join the parts of a URN scheme URI
    #@a list of nid value nss value
    #@r a valid string representation for your URI
    variable urn::NIDpart

    array set parts [list nid {} nss {}]
    array set parts $args
    if {! [regexp -- ^$NIDpart$ $parts(nid)]} {
        error "invalid urn: nid is invalid"
    }
    set url "urn:$parts(nid):[urn::quote $parts(nss)]"
    return $url
}

# -------------------------------------------------------------------------

# Quote the disallowed characters according to the RFC for URN scheme.
# ref: RFC2141 sec2.2
proc ::uri::urn::quote {url} {
    variable trans
    
    set ndx 0
    set result ""
    while {[regexp -indices -- "\[^$trans\]" $url r]} {
        set ndx [lindex $r 0]
        scan [string index $url $ndx] %c chr
        set rep %[format %.2X $chr]
        if {[string match $rep %00]} {
            error "invalid character: character $chr is not allowed"
        }
        
        incr ndx -1
        append result [string range $url 0 $ndx] $rep
        incr ndx 2
        set url [string range $url $ndx end]
    }
    append result $url
    return $result
}

# -------------------------------------------------------------------------

# Perform the reverse of urn::quote.
proc ::uri::urn::unquote {url} {
    set ndx 0
    while {[regexp -start $ndx -indices {%([0-9a-zA-Z]{2})} $url r]} {
        set first [lindex $r 0]
        set last [lindex $r 1]
        set str [string replace [string range $url $first $last] 0 0 0x]
        set c [format %c $str]
        set url [string replace $url $first $last $c]
        set ndx [expr {$last + 1}]
    }
    return $url
}

# -------------------------------------------------------------------------
# Local Variables:
#   indent-tabs-mode: nil
# End:
