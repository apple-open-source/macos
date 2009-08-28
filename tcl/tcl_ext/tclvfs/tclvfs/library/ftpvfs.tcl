
package provide vfs::ftp 1.0

package require vfs 1.0
package require ftp

namespace eval vfs::ftp {
    # Number of milliseconds for which to cache listings
    variable cacheListingsFor 1000
}

proc vfs::ftp::Mount {dirurl local} {
    set dirurl [string trim $dirurl]
    ::vfs::log "ftp-vfs: attempt to mount $dirurl at $local"
    if {[string index $dirurl end] != "/"} {
	::vfs::log "ftp-vfs: adding missing directory delimiter to mount point"
	append dirurl "/"
    }
    
    set urlRE {(?:ftp://)?(?:([^@:]*)(?::([^@]*))?@)?([^/:]+)(?::([0-9]*))?/(.*/)?$} 
    if {![regexp $urlRE $dirurl - user pass host port path]} {
	return -code error "Sorry I didn't understand\
	  the url address \"$dirurl\""
    }
    
    if {![string length $user]} {
	set user anonymous
    }
    
    if {![string length $port]} {
	set port 21
    }
    
    set fd [::ftp::Open $host $user $pass -port $port -output ::vfs::ftp::log]
    if {$fd == -1} {
	error "Mount failed"
    }
    
    if {$path != ""} {
	if {[catch {
	    ::ftp::Cd $fd $path
	} err]} {
	    ftp::Close $fd
	    error "Opened ftp connection, but then received error: $err"
	}
    }
    
    if {![catch {vfs::filesystem info $dirurl}]} {
	# unmount old mount
	::vfs::log "ftp-vfs: unmounted old mount point at $dirurl"
	vfs::unmount $dirurl
    }
    ::vfs::log "ftp $host, $path mounted at $fd"
    vfs::filesystem mount $local [list vfs::ftp::handler $fd $path]
    # Register command to unmount
    vfs::RegisterMount $local [list ::vfs::ftp::Unmount $fd]
    return $fd
}

# Need this because vfs::log takes just one argument
proc vfs::ftp::log {args} {
    ::vfs::log $args
}

proc vfs::ftp::Unmount {fd local} {
    vfs::filesystem unmount $local
    ::ftp::Close $fd
}

proc vfs::ftp::handler {fd path cmd root relative actualpath args} {
    if {$cmd == "matchindirectory"} {
	eval [list $cmd $fd $relative $actualpath] $args
    } else {
	eval [list $cmd $fd $relative] $args
    }
}

proc vfs::ftp::attributes {fd} { return [list "state"] }
proc vfs::ftp::state {fd args} {
    vfs::attributeCantConfigure "state" "readwrite" $args
}

# If we implement the commands below, we will have a perfect
# virtual file system for remote ftp sites.

proc vfs::ftp::stat {fd name} {
    ::vfs::log "stat $name"
    if {$name == ""} {
	return [list type directory mtime 0 size 0 mode 0777 ino -1 \
	  depth 0 name "" dev -1 uid -1 gid -1 nlink 1]
    }
    # get information on the type of this file
    set ftpInfo [_findFtpInfo $fd $name]
    if {$ftpInfo == ""} { 
	vfs::filesystem posixerror $::vfs::posix(ENOENT)
    }
    ::vfs::log $ftpInfo
    set perms [lindex $ftpInfo 0]
    if {[string index $perms 0] == "d"} {
	lappend res type directory size 0
	set mtime 0
    } else {
	lappend res type file size [ftp::FileSize $fd $name]
	set mtime [ftp::ModTime $fd $name]
    }
    lappend res dev -1 uid -1 gid -1 nlink 1 depth 0 \
      atime $mtime ctime $mtime mtime $mtime mode 0777
    return $res
}

proc vfs::ftp::access {fd name mode} {
    ::vfs::log "ftp-access $name $mode"
    if {$name == ""} { return 1 }
    set info [_findFtpInfo $fd $name]
    if {[string length $info]} {
	return 1
    } else {
	vfs::filesystem posixerror $::vfs::posix(ENOENT)
    }
}

# We've chosen to implement these channels by using a memchan.
# The alternative would be to use temporary files.
proc vfs::ftp::open {fd name mode permissions} {
    ::vfs::log "open $name $mode $permissions"
    # return a list of two elements:
    # 1. first element is the Tcl channel name which has been opened
    # 2. second element (optional) is a command to evaluate when
    #    the channel is closed.
    switch -glob -- $mode {
	"" -
	"r" {
	    ftp::Get $fd $name -variable tmp

	    set filed [vfs::memchan]
	    fconfigure $filed -translation binary
	    puts -nonewline $filed $tmp

	    fconfigure $filed -translation auto
	    seek $filed 0
	    return [list $filed]
	}
	"a" {
	    # Try to append nothing to the file
	    if {[catch [list ::ftp::Append $fd -data "" $name] err] || !$err} {
		error "Can't open $name for appending"
	    }

	    set filed [vfs::memchan]
	    return [list $filed [list ::vfs::ftp::_closing $fd $name $filed Append]]
	}
	"w*" {
	    # Try to write an empty file
	    if {[catch [list ::ftp::Put $fd -data "" $name] err] || !$err} {
		error "Can't open $name for writing"
	    }

	    set filed [vfs::memchan]
	    return [list $filed [list ::vfs::ftp::_closing $fd $name $filed Put]]
	}
	default {
	    return -code error "illegal access mode \"$mode\""
	}
    }
}

proc vfs::ftp::_closing {fd name filed action} {
    seek $filed 0
    set contents [read $filed]
    set trans [fconfigure $filed -translation]
    if {$trans == "binary"} {
	set oldType [::ftp::Type $fd]
	::ftp::Type $fd binary
    }
    if {![::ftp::$action $fd -data $contents $name]} {
	# Would be better if we could be more specific here, with
	# one of ENETRESET ENETDOWN ENETUNREACH or whatever.
	vfs::filesystem posixerror $::vfs::posix(EIO)
	#error "Failed to write to $name"
    }
    if {[info exists oldType]} {
	::ftp::Type $fd $oldType
    }
}

proc vfs::ftp::_findFtpInfo {fd name} {
    ::vfs::log "findFtpInfo $fd $name"
    set ftpList [cachedList $fd [file dirname $name]]
    foreach p $ftpList {
	foreach {pname other} [_parseListLine $p] {}
	if {$pname == [file tail $name]} {
	    return $other
	}
    }
    return ""
}

proc vfs::ftp::cachedList {fd dir} {
    variable cacheList
    variable cacheListingsFor
    
    # Caches response to prevent going back to the ftp server
    # for common use cases: foreach {f} [glob *] { file stat $f s }
    if {[info exists cacheList($dir)]} {
	return $cacheList($dir)
    }
    set listing [ftp::List $fd $dir]

    set cacheList($dir) $listing
    after $cacheListingsFor [list unset -nocomplain ::vfs::ftp::cacheList($dir)]
    return $listing
}

# Currently returns a list of name and a list of other
# information.  The other information is currently a 
# list of:
# () permissions
# () size
proc vfs::ftp::_parseListLine {line} {
    # Check for filenames with spaces
    if {[regexp {([^ ]|[^0-9] )+$} $line name]} {
	# Check for links
	if {[set idx [string first " -> " $name]] != -1} {
	    incr idx -1
	    set name [string range $name 0 $idx]
	}
    }
    regsub -all "\[ \t\]+" $line " " line
    set items [split $line " "]

    if {![info exists name]} {set name [lindex $items end]}
    lappend other [lindex $items 0]
    if {[string is integer [lindex $items 4]]} {
	lappend other [lindex $items 4]
    }
    
    return [list $name $other]
}

proc vfs::ftp::matchindirectory {fd path actualpath pattern type} {
    ::vfs::log "matchindirectory $fd $path $actualpath $pattern $type"
    set res [list]
    if {![string length $pattern]} {
	# matching a single file
	set ftpInfo [_findFtpInfo $fd $path]
	if {$ftpInfo != ""} {
	    # Now check if types match
	    set perms [lindex $ftpInfo 0]
	    if {[string index $perms 0] == "d"} {
		if {[::vfs::matchDirectories $type]} {
		    lappend res $actualpath
		}
	    } else {
		if {[::vfs::matchFiles $type]} {
		    lappend res $actualpath
		}
	    }
	}
    } else {
	# matching all files in the given directory
	set ftpList [cachedList $fd $path]
	::vfs::log "ftpList: $ftpList"

	foreach p $ftpList {
	    foreach {name perms} [_parseListLine $p] {}
	    if {![string match $pattern $name]} {
		continue 
	    } 
	    if {[::vfs::matchDirectories $type]} {
		if {[string index $perms 0] == "d"} {
		    lappend res [file join $actualpath $name]
		}
	    }
	    if {[::vfs::matchFiles $type]} {
		if {[string index $perms 0] != "d"} {
		    lappend res [file join $actualpath $name]
		}
	    }
	    
	}
    }
 
    return $res
}

proc vfs::ftp::createdirectory {fd name} {
    ::vfs::log "createdirectory $name"
    if {![ftp::MkDir $fd $name]} {
	# Can we be more specific here?
	vfs::filesystem posixerror $::vfs::posix(EACCES)
    }
}

proc vfs::ftp::removedirectory {fd name recursive} {
    ::vfs::log "removedirectory $name $recursive"
    if {![ftp::RmDir $fd $name]} {
	# Can we be more specific here?
	if {$recursive} {
	    vfs::filesystem posixerror $::vfs::posix(EACCES)
	} else {
	    vfs::filesystem posixerror $::vfs::posix(EACCES)
	}
    }
}

proc vfs::ftp::deletefile {fd name} {
    ::vfs::log "deletefile $name"
    if {![ftp::Delete $fd $name]} {
	# Can we be more specific here?
	vfs::filesystem posixerror $::vfs::posix(EACCES)
    }
}

proc vfs::ftp::fileattributes {fd path args} {
    ::vfs::log "fileattributes $args"
    switch -- [llength $args] {
	0 {
	    # list strings
	    return [list]
	}
	1 {
	    # get value
	    set index [lindex $args 0]
	    vfs::filesystem posixerror $::vfs::posix(ENODEV)
	}
	2 {
	    # set value
	    set index [lindex $args 0]
	    set val [lindex $args 1]
	    vfs::filesystem posixerror $::vfs::posix(ENODEV)
	}
    }
}

proc vfs::ftp::utime {fd path actime mtime} {
    # Will throw an error if ftp package is old and only
    # handles 2 arguments.  But that is ok -- Tcl will give the
    # user an appropriate error message.
    ftp::ModTime $fd $path $mtime
}

