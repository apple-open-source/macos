
package provide vfs::http 0.6

package require vfs 1.0
package require http

# This works for basic operations, using http GET and HEAD requests
# to serve http data in a read-only file system.

namespace eval vfs::http {
    # Allow for options when mounting an http URL
    variable options
    # -urlencode means automatically parse "foo/my file (2).txt" as
    # "foo/my%20file%20%282%29.txt", per RFC 3986, for the user.
    set options(-urlencode) 1
    # -urlparse would further parse URLs for ? (query string) and # (anchor)
    # components, leaving those unencoded. Only works when -urlencode is true.
    set options(-urlparse) 0
}

proc vfs::http::Mount {dirurl local args} {
    ::vfs::log "http-vfs: attempt to mount $dirurl at $local (args: $args)"
    variable options
    foreach {key val} $args {
	# only do exact option name matching for now
	# We could consider allowing general http options here,
	# but those would be per-mount
	if {[info exists options($key)]} {
	    # currently only boolean values
	    if {![string is boolean -strict $val]} {
		return -code error "invalid boolean value \"$val\" for $key"
	    }
	    set options($key) $val
	}
    }

    # Break the url into parts, verifying url
    array set parts [urlparse $dirurl]

    if {[info exists parts(query)] || [info exists parts(anchor)]} {
	return -code error "invalid url \"$dirurl\":\
		no query string or anchor fragments allowed"
    }

    if {[info exists parts(user)]} {
	# At this point we need base64 for HTTP Basic AUTH
	package require base64
	foreach {user passwd} [split $parts(user) :] { break }
	set auth "Basic [base64::encode $user:$passwd]"
	set headers [list Authorization $auth]
    } else {
	set headers ""
    }

    set token [::http::geturl $parts(url) -validate 1 -headers $headers]
    http::wait $token
    set status [http::status $token]
    http::cleanup $token
    if {$status ne "ok"} {
	# we'll take whatever http agrees is "ok"
	return -code error "received status \"$status\" for \"$parts(url)\""
    }

    # Add a / to make sure the url and names are clearly separated later
    if {[string index $parts(url) end] ne "/"} {
	append parts(url) "/"
    }

    if {![catch {vfs::filesystem info $parts(url)}]} {
	# unmount old mount
	::vfs::log "ftp-vfs: unmounted old mount point at $parts(url)"
	vfs::unmount $parts(url)
    }
    ::vfs::log "http $dirurl ($parts(url)) mounted at $local"
    # Pass headers along as they may include authentication
    vfs::filesystem mount $local \
	[list vfs::http::handler $parts(url) $headers $parts(file)]
    # Register command to unmount - headers not needed
    vfs::RegisterMount $local [list ::vfs::http::Unmount $parts(url)]
    return $parts(url)
}

proc vfs::http::Unmount {url local} {
    vfs::filesystem unmount $local
}

proc vfs::http::handler {url headers path cmd root relative actualpath args} {
    if {$cmd eq "matchindirectory"} {
	eval [linsert $args 0 $cmd $url $headers $relative $actualpath]
    } else {
	eval [linsert $args 0 $cmd $url $headers $relative]
    }
}

proc vfs::http::urlparse {url} {
    # Taken from http 2.5.3

    # Validate URL by parts.  We suck out user:pass if it exists as the
    # core http package does not automate HTTP Basic Auth yet.

    # Returns data in [array get] format.  The url, host and file keys are
    # guaranteed to exist.  proto, port, query, anchor, and user should be
    # checked with [info exists]. (user may contain password)

    # URLs have basically four parts.
    # First, before the colon, is the protocol scheme (e.g. http)
    # Second, for HTTP-like protocols, is the authority
    #	The authority is preceded by // and lasts up to (but not including)
    #	the following / and it identifies up to four parts, of which only one,
    #	the host, is required (if an authority is present at all). All other
    #	parts of the authority (user name, password, port number) are optional.
    # Third is the resource name, which is split into two parts at a ?
    #	The first part (from the single "/" up to "?") is the path, and the
    #	second part (from that "?" up to "#") is the query. *HOWEVER*, we do
    #	not need to separate them; we send the whole lot to the server.
    # Fourth is the fragment identifier, which is everything after the first
    #	"#" in the URL. The fragment identifier MUST NOT be sent to the server
    #	and indeed, we don't bother to validate it (it could be an error to
    #	pass it in here, but it's cheap to strip).
    #
    # An example of a URL that has all the parts:
    #   http://jschmoe:xyzzy@www.bogus.net:8000/foo/bar.tml?q=foo#changes
    # The "http" is the protocol, the user is "jschmoe", the password is
    # "xyzzy", the host is "www.bogus.net", the port is "8000", the path is
    # "/foo/bar.tml", the query is "q=foo", and the fragment is "changes".
    #
    # Note that the RE actually combines the user and password parts, as
    # recommended in RFC 3986. Indeed, that RFC states that putting passwords
    # in URLs is a Really Bad Idea, something with which I would agree utterly.
    # Also note that we do not currently support IPv6 addresses.
    #
    # From a validation perspective, we need to ensure that the parts of the
    # URL that are going to the server are correctly encoded.

    set URLmatcher {(?x)		# this is _expanded_ syntax
	^
	(?: (\w+) : ) ?			# <protocol scheme>
	(?: //
	    (?:
		(
		    [^@/\#?]+		# <userinfo part of authority>
		) @
	    )?
	    ( [^/:\#?]+ )		# <host part of authority>
	    (?: : (\d+) )?		# <port part of authority>
	)?
	( / [^\#?]* (?: \? [^\#?]* )?)?	# <path> (including query)
	(?: \# (.*) )?			# <fragment> (aka anchor)
	$
    }

    # Phase one: parse
    if {![regexp -- $URLmatcher $url -> proto user host port srvurl anchor]} {
	unset $token
	return -code error "Unsupported URL: $url"
    }
    # Phase two: validate
    if {$host eq ""} {
	# Caller has to provide a host name; we do not have a "default host"
	# that would enable us to handle relative URLs.
	unset $token
	return -code error "Missing host part: $url"
	# Note that we don't check the hostname for validity here; if it's
	# invalid, we'll simply fail to resolve it later on.
    }
    if {$port ne "" && $port>65535} {
	unset $token
	return -code error "Invalid port number: $port"
    }
    # The user identification and resource identification parts of the URL can
    # have encoded characters in them; take care!
    if {$user ne ""} {
	# Check for validity according to RFC 3986, Appendix A
	set validityRE {(?xi)
	    ^
	    (?: [-\w.~!$&'()*+,;=:] | %[0-9a-f][0-9a-f] )+
	    $
	}
	if {![regexp -- $validityRE $user]} {
	    unset $token
	    # Provide a better error message in this error case
	    if {[regexp {(?i)%(?![0-9a-f][0-9a-f]).?.?} $user bad]} {
		return -code error \
			"Illegal encoding character usage \"$bad\" in URL user"
	    }
	    return -code error "Illegal characters in URL user"
	}
    }
    if {$srvurl ne ""} {
	# Check for validity according to RFC 3986, Appendix A
	set validityRE {(?xi)
	    ^
	    # Path part (already must start with / character)
	    (?:	      [-\w.~!$&'()*+,;=:@/]  | %[0-9a-f][0-9a-f] )*
	    # Query part (optional, permits ? characters)
	    (?: \? (?: [-\w.~!$&'()*+,;=:@/?] | %[0-9a-f][0-9a-f] )* )?
	    $
	}
	if {![regexp -- $validityRE $srvurl]} {
	    unset $token
	    # Provide a better error message in this error case
	    if {[regexp {(?i)%(?![0-9a-f][0-9a-f])..} $srvurl bad]} {
		return -code error \
			"Illegal encoding character usage \"$bad\" in URL path"
	    }
	    return -code error "Illegal characters in URL path"
	}
    } else {
	set srvurl /
    }
    if {$proto eq ""} {
	set proto http
    } else {
	set result(proto) $proto
    }

    # Here we vary from core http

    # vfs::http - we only support http at this time.  Perhaps https later?
    if {$proto ne "http"} {
	return -code error "Unsupported URL type \"$proto\""
    }

    # OK, now reassemble into a full URL, with result containing the
    # parts that exist and will be returned to the user
    array set result {}
    set url ${proto}://
    if {$user ne ""} {
	set result(user) $user
	# vfs::http will do HTTP basic auth on their existence,
	# but we pass these through as they are innocuous
	append url $user
	append url @
    }
    append url $host
    set result(host) $host
    if {$port ne ""} {
	# don't bother with adding default port
	append url : $port
	set result(port) $port
    }
    append url $srvurl
    if {$anchor ne ""} {
	# XXX: Don't append see the anchor, as it is generally a client-side
	# XXX: item.  The user can add it back if they want.
	#append url \# $anchor
	set result(anchor) $anchor
    }

    set idx [string first ? $srvurl]
    if {$idx >= 0} {
	set query [string range [expr {$idx+1}] end]
	set file  [string range 0 [expr {$idx-1}]]
	set result(file) $file
	set result(query) $query
    } else {
	set result(file) $srvurl
    }

    set result(url) $url

    # return array format list of items
    return [array get result]
}

proc vfs::http::urlname {name} {
    # Parse the passed in name into a suitable URL name based on mount opts
    variable options
    if {$options(-urlencode)} {
	set querystr ""
	if {$options(-urlparse)} {
	    # check for ? and split if necessary so that the query_string
	    # part doesn't get encoded.  Anchors come after this as well.
	    set idx [string first ? $name]
	    if {$idx >= 0} {
		set querystr [string range $name $idx end] ; # includes ?
		set name [string range $name 0 [expr {$idx-1}]]
	    }
	}
	set urlparts [list]
	foreach part [file split $name] {
	    lappend urlparts [http::mapReply $part]
	}
	set urlname "[join $urlparts /]$querystr"
    } else {
	set urlname $name
    }
    return $urlname
}

proc vfs::http::geturl {url args} {
    # a wrapper around http::geturl that handles 404 or !ok status check
    # returns error on no success, or a fully ready http token otherwise
    set token [eval [linsert $args 0 ::http::geturl $url]]
    http::wait $token

    if {[http::ncode $token] == 404 || [http::status $token] ne "ok"} {
	# 404 Not Found
	set code [http::code $token]
	http::cleanup $token
	vfs::filesystem posixerror $::vfs::posix(ENOENT)
	return -code error \
	    "could not read \"$url\": no such file or directory ($code)"
    }

    # treat returned token like a regular http token
    # call http::cleanup on it when done
    return $token
}

# If we implement the commands below, we will have a perfect
# virtual file system for remote http sites.

proc vfs::http::stat {dirurl headers name} {
    set urlname [urlname $name]
    ::vfs::log "stat $name ($urlname)"

    # get information on the type of this file.  We describe everything
    # as a file (not a directory) since with http, even directories
    # really behave as the index.html they contain.

    # this will through an error if the file doesn't exist
    set token [geturl "$dirurl$urlname" -validate 1 -headers $headers]
    http::cleanup $token
    set mtime 0
    lappend res type file
    lappend res dev -1 uid -1 gid -1 nlink 1 depth 0 \
      atime $mtime ctime $mtime mtime $mtime mode 0777
    return $res
}

proc vfs::http::access {dirurl headers name mode} {
    set urlname [urlname $name]
    ::vfs::log "access $name $mode ($urlname)"
    if {$mode & 2} {
	vfs::filesystem posixerror $::vfs::posix(EROFS)
	return -code error "read-only"
    }
    if {$name == ""} { return 1 }
    # this will through an error if the file doesn't exist
    set token [geturl "$dirurl$urlname" -validate 1 -headers $headers]
    http::cleanup $token
    return 1
}

# We've chosen to implement these channels by using a memchan.
# The alternative would be to use temporary files.
proc vfs::http::open {dirurl headers name mode permissions} {
    set urlname [urlname $name]
    ::vfs::log "open $name $mode $permissions ($urlname)"
    # return a list of two elements:
    # 1. first element is the Tcl channel name which has been opened
    # 2. second element (optional) is a command to evaluate when
    #    the channel is closed.
    switch -glob -- $mode {
	"" -
	"r" {
	    set token [geturl "$dirurl$urlname" -headers $headers]
	    set filed [vfs::memchan]
	    fconfigure $filed -translation binary
	    puts -nonewline $filed [::http::data $token]
	    http::cleanup $token

	    fconfigure $filed -translation auto
	    seek $filed 0
	    # XXX: the close command should free vfs::memchan somehow??
	    return [list $filed]
	}
	"a" -
	"w*" {
	    vfs::filesystem posixerror $::vfs::posix(EROFS)
	}
	default {
	    return -code error "illegal access mode \"$mode\""
	}
    }
}

proc vfs::http::matchindirectory {dirurl headers path actualpath pattern type} {
    ::vfs::log "matchindirectory $path $pattern $type"
    set res [list]

    if {[string length $pattern]} {
	# need to match all files in a given remote http site.
    } else {
	# single file
	if {![catch {access $dirurl $path 0}]} {
	    lappend res $path
	}
    }

    return $res
}

proc vfs::http::createdirectory {dirurl headers name} {
    ::vfs::log "createdirectory $name"
    vfs::filesystem posixerror $::vfs::posix(EROFS)
}

proc vfs::http::removedirectory {dirurl headers name recursive} {
    ::vfs::log "removedirectory $name"
    vfs::filesystem posixerror $::vfs::posix(EROFS)
}

proc vfs::http::deletefile {dirurl headers name} {
    ::vfs::log "deletefile $name"
    vfs::filesystem posixerror $::vfs::posix(EROFS)
}

proc vfs::http::fileattributes {dirurl headers path args} {
    ::vfs::log "fileattributes $args"
    switch -- [llength $args] {
	0 {
	    # list strings
	    return [list]
	}
	1 {
	    # get value
	    set index [lindex $args 0]
	}
	2 {
	    # set value
	    set index [lindex $args 0]
	    set val [lindex $args 1]
	    vfs::filesystem posixerror $::vfs::posix(EROFS)
	}
    }
}

proc vfs::http::utime {dirurl headers path actime mtime} {
    vfs::filesystem posixerror $::vfs::posix(EROFS)
}
