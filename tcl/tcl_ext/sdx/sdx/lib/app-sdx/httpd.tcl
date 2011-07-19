# Simple Sample httpd/1.0 server in 250 lines of Tcl
# Stephen Uhler / Brent Welch (c) 1996 Sun Microsystems
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

# This is a working sample httpd server written entirely in TCL with the
# CGI and imagemap capability removed.  It has been tested on the Mac, PC
# and Unix.  It is intended as sample of how to write internet servers in
# Tcl. This sample server was derived from a full-featured httpd server,
# also written entirely in Tcl.
# Comments or questions welcome (stephen.uhler@sun.com)

# Httpd is a global array containing the global server state
#  root:	the root of the document directory
#  port:	The port this server is serving
#  listen:	the main listening socket id
#  accepts:	a count of accepted connections so far

array set Httpd {
    -version	"Tcl Httpd-Lite 1.0"
    -launch	0
    -port	8080
    -ipaddr	""
    -default	index.html
    -root	/wwwroot
    -bufsize	32768
    -sockblock	0
    -config	""
}
set Httpd(-host)	[info hostname]

# HTTP/1.0 error codes (the ones we use)

array set HttpdErrors {
    204 {No Content}
    400 {Bad Request}
    404 {Not Found}
    503 {Service Unavailable}
    504 {Service Temporarily Unavailable}
    }


# Start the server by listening for connections on the desired port.

proc Httpd_Server {args} {
    global Httpd

    if {[llength $args] == 1} {
	set args [lindex $args 0]
    }
    array set Httpd $args

    if {![file isdirectory $Httpd(-root)]} {
	return -code error "Bad root directory \"$Httpd(-root)\""
    }
    if {![file exists [file join $Httpd(-root) $Httpd(-default)]]} {
	# Try and find a good default
	foreach idx {index.htm index.html default.htm contents.htm} {
	    if {[file exists [file join $Httpd(-root) $idx]]} {
		set Httpd(-default) $idx
		break
	    }
	}
    }
    if {![file exists [file join $Httpd(-root) $Httpd(-default)]]} {
	return -code error "Bad index page \"$Httpd(-default)\""
    }
    if {$Httpd(-ipaddr) != ""} {
	set Httpd(listen) [socket -server HttpdAccept \
				-myaddr $Httpd(-ipaddr) $Httpd(-port)]
    } else {
	set Httpd(listen) [socket -server HttpdAccept $Httpd(-port)]
    }
    set Httpd(accepts) 0
    if {$Httpd(-port) == 0} {
	set Httpd(-port) [lindex [fconfigure $Httpd(listen) -sockname] 2]
    }
    return $Httpd(-port)
}

# Accept a new connection from the server and set up a handler
# to read the request from the client.

proc HttpdAccept {newsock ipaddr port} {
    global Httpd
    upvar #0 Httpd$newsock data

    incr Httpd(accepts)
    fconfigure $newsock -blocking $Httpd(-sockblock) \
	-buffersize $Httpd(-bufsize) \
	-translation {auto crlf}
    Httpd_Log $newsock Connect $ipaddr $port
    set data(ipaddr) $ipaddr
    fileevent $newsock readable [list HttpdRead $newsock]
}

# read data from a client request

proc HttpdRead { sock } {
    upvar #0 Httpd$sock data

    set readCount [gets $sock line]
    if {![info exists data(state)]} {
	if [regexp {(POST|GET) ([^?]+)\??([^ ]*) HTTP/1.[01]} \
		$line x data(proto) data(url) data(query)] {
	    set data(state) mime
	    Httpd_Log $sock Query $line
	} else {
	    HttpdError $sock 400
	    Httpd_Log $sock Error "bad first line:$line"
	    HttpdSockDone $sock
	}
	return
    }

    # string compare $readCount 0 maps -1 to -1, 0 to 0, and > 0 to 1

    set state [string compare $readCount 0],$data(state),$data(proto)
    switch -- $state {
	0,mime,GET	-
	0,query,POST	{ HttpdRespond $sock }
	0,mime,POST	{ set data(state) query }
	1,mime,POST	-
	1,mime,GET	{
	    if [regexp {([^:]+):[ 	]*(.*)}  $line dummy key value] {
		set data(mime,[string tolower $key]) $value
	    }
	}
	1,query,POST	{
	    set data(query) $line
	    HttpdRespond $sock
	}
	default {
	    if [eof $sock] {
		Httpd_Log $sock Error "unexpected eof on <$data(url)> request"
	    } else {
		Httpd_Log $sock Error "unhandled state <$state> fetching <$data(url)>"
	    }
	    HttpdError $sock 404
	    HttpdSockDone $sock
	}
    }
}

proc HttpdCopyDone { in sock bytes {error ""}} {
#tclLog "CopyDone $sock $bytes $error"
    catch {close $in}
    HttpdSockDone $sock
}
# Close a socket.
# We'll use this to implement keep-alives some day.

proc HttpdSockDone { sock } {
    upvar #0 Httpd$sock data
    unset data
    close $sock
}

# Respond to the query.

proc HttpdRespond { sock } {
    global Httpd
    upvar #0 Httpd$sock data

    set mypath [HttpdUrl2File $Httpd(-root) $data(url)]
    if {[string length $mypath] == 0} {
	HttpdError $sock 400
	Httpd_Log $sock Error "$data(url) invalid path"
	HttpdSockDone $sock
	return
    }

    if {![catch {open $mypath} in]} {
	puts $sock "HTTP/1.0 200 Data follows"
	puts $sock "Date: [HttpdDate [clock seconds]]"
	puts $sock "Server: $Httpd(-version)"
	puts $sock "Last-Modified: [HttpdDate [file mtime $mypath]]"
	puts $sock "Content-Type: [HttpdContentType $mypath]"
	puts $sock "Content-Length: [file size $mypath]"
	puts $sock ""
	fconfigure $sock -translation binary -blocking $Httpd(-sockblock)
	fconfigure $in -translation binary -blocking 0
	flush $sock
	fileevent $sock readable {}
	fcopy $in $sock -command [list HttpdCopyDone $in $sock]
	#HttpdSockDone $sock
    } else {
	HttpdError $sock 404
	Httpd_Log $sock Error "$data(url) $in"
	HttpdSockDone $sock
    }
}
# convert the file suffix into a mime type
# add your own types as needed

array set HttpdMimeType {
    {}		text/plain
    .txt	text/plain
    .htm	text/html
    .html	text/html
    .gif	image/gif
    .jpg	image/jpeg
    .xbm	image/x-xbitmap
}

proc HttpdContentType {path} {
    global HttpdMimeType

    set type text/plain
    catch {set type $HttpdMimeType([string tolower [file extension $path]])}
    return $type
}

# Generic error response.

set HttpdErrorFormat {
    <title>Error: %1$s</title>
    Got the error: <b>%2$s</b><br>
    while trying to obtain <b>%3$s</b>
}

proc HttpdError {sock code} {
    upvar #0 Httpd$sock data
    global HttpdErrors HttpdErrorFormat Httpd

    append data(url) ""
    set message [format $HttpdErrorFormat $code $HttpdErrors($code)  $data(url)]
    puts $sock "HTTP/1.0 $code $HttpdErrors($code)"
    puts $sock "Date: [HttpdDate [clock seconds]]"
    puts $sock "Server: $Httpd(-version)"
    puts $sock "Content-Length: [string length $message]"
    puts $sock ""
    puts -nonewline $sock $message
}

# Generate a date string in HTTP format.

proc HttpdDate {clicks} {
    return [clock format $clicks -format {%a, %d %b %Y %T %Z}]
}

# Log an Httpd transaction.
# This should be replaced as needed.

proc Httpd_Log {sock reason args} {
    global httpdLog httpClicks
    if {[info exists httpdLog]} {
	if ![info exists httpClicks] {
	    set last 0
	} else {
	    set last $httpClicks
	}
	set httpClicks [clock seconds]
	set ts [clock format [clock seconds] -format {%Y%m%d %T}]
	puts $httpdLog "$ts ([expr $httpClicks - $last])\t$sock\t$reason\t[join $args { }]"
    }
}

# Convert a url into a pathname.
# This is probably not right.

proc HttpdUrl2File {root url} {
    global HttpdUrlCache Httpd

    if {![info exists HttpdUrlCache($url)]} {
    	lappend pathlist $root
    	set level 0
	foreach part  [split $url /] {
	    set part [HttpdCgiMap $part]
	    if [regexp {[:/]} $part] {
		return [set HttpdUrlCache($url) ""]
	    }
	    switch -- $part {
		.  { }
		.. {incr level -1}
		default {incr level}
	    }
	    if {$level <= 0} {
		return [set HttpdUrlCache($url) ""]
	    } 
	    lappend pathlist $part
	}
    	set file [eval file join $pathlist]
	if {[file isdirectory $file]} {
	    set file [file join $file $Httpd(-default)]
	}
    	set HttpdUrlCache($url) $file
    }
    return $HttpdUrlCache($url)
}

# Decode url-encoded strings.

proc HttpdCgiMap {data} {
    regsub -all {([][$\\])} $data {\\\1} data
    regsub -all {%([0-9a-fA-F][0-9a-fA-F])} $data  {[format %c 0x\1]} data
    return [subst $data]
}

proc bgerror {msg} {
    global errorInfo
    puts stderr "bgerror: $errorInfo"
}
proc openurl url {
   global tcl_platform
   if {[lindex $tcl_platform(os) 1] == "NT"} {
       exec cmd /c start $url &
   } else {
       exec start $url &
   }
}

set httpdLog stderr

upvar #0 Httpd opts

while {[llength $argv] > 0} {
    set option [lindex $argv 0]
    if {![info exists opts($option)] || [llength $argv] == 1} {
	puts stderr "usage: httpd ?options?"
	puts stderr "\nwhere options are any of the following:\n"
	foreach opt [lsort [array names opts -*]] {
	    puts stderr [format "\t%-15s default: %s" $opt $opts($opt)]
	}
	exit 1
    }
    set opts($option) [lindex $argv 1]
    set argv [lrange $argv 2 end]
}
catch {
    package require vfs
    vfs::auto $opts(-root) -readonly
}

if {$opts(-config) != ""} {
    source $opts(-config)
}

Httpd_Server [array get opts]


puts stderr "Accepting connections on http://$Httpd(-host):$Httpd(-port)/"

if {$Httpd(-launch)} {
    openurl "http://$Httpd(-host):$Httpd(-port)/"
}

if {![info exists tcl_service]} {
    vwait forever		;# start the Tcl event loop
}
