#!/bin/sh -
#\
exec tclsh `cygpath -w "$0"` "$@"

package require FTP
package require http

# support automatic proxying
package require autoproxy
autoproxy::init

auto_load http::geturl

namespace eval ftp {
    proc Progress {bytes} {
	puts -nonewline stderr .
	flush stderr
    }
    proc copy { url {file .} {chunk 32768}} {

	set ::FTP::VERBOSE 0
	set ::FTP::DEBUG 0

	set re {^ftp://(([^:/@]+):?([^/@]*)?@)?([^/]+)/(.*)}
	if {![regexp $re $url - x user pw site src]} {
	    error "bad url"
	}
	if {$user == ""} {
	    global tcl_platform
	    set user anonymous
	    set pw $tcl_platform(user)@[info hostname]
	}

	if {$pw == ""} {
	    puts -nonewline stderr "Password: "
	    set pw [gets stdin]
	}

	if {![FTP::Open $site $user $pw \
		-mode active -blocksize $chunk \
		-progress ::ftp::Progress]} {
	    error "can't connect to $url"
	}
	FTP::Type I

	if {[file isdirectory $file]} {
	    set file [file join $file [file tail $url]]
	    puts stderr "Saving to $file"
	}

	if {![FTP::Get $src $file]} {
	    error "unable to retreive $url"
	}
	puts stdout ""

	FTP::Close
    }
}
# Copy a URL to a file and print meta-data
proc ::http::copy { url {file .} {chunk 32768} } {
    if {[file isdirectory $file]} {
	set file [file join $file [file tail $url]]
	#puts stderr "Saving to $file"
    }
    set out [open $file w]
    fconfigure $out -translation binary
    set token [geturl $url -channel $out -progress ::http::Progress \
	-blocksize $chunk]
    close $out
    # This ends the line started by http::Progress
    puts stderr ""
    upvar #0 $token state
    set max 0
    foreach {name value} $state(meta) {
	if {[string length $name] > $max} {
	    set max [string length $name]
	}
	if {[regexp -nocase ^location$ $name]} {
	    # Handle URL redirects
	    puts stderr "Location:$value"
	    return [copy [string trim $value] $file $chunk]
	}
    }
    incr max
    foreach {name value} $state(meta) {
	puts [format "%-*s %s" $max $name: $value]
    }
    return $token
}
proc ::http::Progress {args} {
    puts -nonewline stderr . ; flush stderr
}

if {[llength $argv] < 1} {
    puts stderr "Usage: $argv0 url file ?chunksize?"
    exit 1
}
set url [lindex $argv 0]
switch -glob -- $url {
http://*	{ eval http::copy $argv }
ftp://*		{ eval ftp::copy $argv }
default		{ puts stderr "unsupported url \"$url\""; exit 1 }
}
