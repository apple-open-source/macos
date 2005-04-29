
package provide vfs::http 0.5

package require vfs 1.0
package require http

# This works for basic operations, but has not been very debugged.

namespace eval vfs::http {}

proc vfs::http::Mount {dirurl local} {
    ::vfs::log "http-vfs: attempt to mount $dirurl at $local"
    if {[string index $dirurl end] != "/"} {
	append dirurl "/"
    }
    if {[string range $dirurl 0 6] == "http://"} {
	set rest [string range $dirurl 7 end]
    } else {
	set rest $dirurl
	set dirurl "http://${dirurl}"
    }
    
    if {![regexp {(([^:]*)(:([^@]*))?@)?([^/]*)(/(.*/)?([^/]*))?$} $rest \
      junk junk user junk pass host junk path file]} {
	return -code error "Sorry I didn't understand\
	  the url address \"$dirurl\""
    }
    
    if {[string length $file]} {
	return -code error "Can only mount directories, not\
	  files (perhaps you need a trailing '/' - I understood\
	  a path '$path' and file '$file')"
    }
    
    if {![string length $user]} {
	set user anonymous
    }
    
    set token [::http::geturl $dirurl -validate 1]

    if {![catch {vfs::filesystem info $dirurl}]} {
	# unmount old mount
	::vfs::log "ftp-vfs: unmounted old mount point at $dirurl"
	vfs::unmount $dirurl
    }
    ::vfs::log "http $host, $path mounted at $local"
    vfs::filesystem mount $local [list vfs::http::handler $dirurl $path]
    # Register command to unmount
    vfs::RegisterMount $local [list ::vfs::http::Unmount $dirurl]
    return $dirurl
}

proc vfs::http::Unmount {dirurl local} {
    vfs::filesystem unmount $local
}

proc vfs::http::handler {dirurl path cmd root relative actualpath args} {
    if {$cmd == "matchindirectory"} {
	eval [list $cmd $dirurl $relative $actualpath] $args
    } else {
	eval [list $cmd $dirurl $relative] $args
    }
}

# If we implement the commands below, we will have a perfect
# virtual file system for remote http sites.

proc vfs::http::stat {dirurl name} {
    ::vfs::log "stat $name"
    
    # get information on the type of this file.  We describe everything
    # as a file (not a directory) since with http, even directories
    # really behave as the index.html they contain.
    set state [::http::geturl "$dirurl$name" -validate 1]
    set mtime 0
    lappend res type file
    lappend res dev -1 uid -1 gid -1 nlink 1 depth 0 \
      atime $mtime ctime $mtime mtime $mtime mode 0777
    return $res
}

proc vfs::http::access {dirurl name mode} {
    ::vfs::log "access $name $mode"
    if {$mode & 2} {
	vfs::filesystem posixerror $::vfs::posix(EROFS)
    }
    if {$name == ""} { return 1 }
    set state [::http::geturl "$dirurl$name"]
    set info ""
    if {[string length $info]} {
	return 1
    } else {
	error "No such file"
    }
}

# We've chosen to implement these channels by using a memchan.
# The alternative would be to use temporary files.
proc vfs::http::open {dirurl name mode permissions} {
    ::vfs::log "open $name $mode $permissions"
    # return a list of two elements:
    # 1. first element is the Tcl channel name which has been opened
    # 2. second element (optional) is a command to evaluate when
    #    the channel is closed.
    switch -glob -- $mode {
	"" -
	"r" {
	    set state [::http::geturl "$dirurl$name"]

	    set filed [vfs::memchan]
	    fconfigure $filed -translation binary
	    puts -nonewline $filed [::http::data $state]

	    fconfigure $filed -translation auto
	    seek $filed 0
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

proc vfs::http::matchindirectory {dirurl path actualpath pattern type} {
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

proc vfs::http::createdirectory {dirurl name} {
    ::vfs::log "createdirectory $name"
    vfs::filesystem posixerror $::vfs::posix(EROFS)
}

proc vfs::http::removedirectory {dirurl name recursive} {
    ::vfs::log "removedirectory $name"
    vfs::filesystem posixerror $::vfs::posix(EROFS)
}

proc vfs::http::deletefile {dirurl name} {
    ::vfs::log "deletefile $name"
    vfs::filesystem posixerror $::vfs::posix(EROFS)
}

proc vfs::http::fileattributes {dirurl path args} {
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

proc vfs::http::utime {dirurl path actime mtime} {
    vfs::filesystem posixerror $::vfs::posix(EROFS)
}

