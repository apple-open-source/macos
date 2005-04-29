# The idea here is that we can mount 'ftp' or 'http' or 'file' types
# of urls and that (provided we have separate vfs types for them) we
# can then treat 'ftp://' as a mount point for ftp services.  For
# example, we can do:
#
# % vfs::urltype::Mount ftp
# Mounted at "ftp://"
# % cd ftp://
# % cd ftp.ucsd.edu   (or 'cd user:pass@ftp.foo.com')
# (This now creates an ordinary ftp-vfs for the remote site)
# ...
#
# Or all in one go:
# 
# % file copy ftp://ftp.ucsd.edu/pub/alpha/Readme .

package provide vfs::urltype 1.0
package require vfs

namespace eval ::vfs::urltype {}

proc vfs::urltype::Mount {type} {
    set mountPoint [_typeToMount $type]
    ::vfs::filesystem mount -volume $mountPoint [list vfs::urltype::handler $type]
    return "Mounted at \"${mountPoint}\""
}

proc vfs::urltype::Unmount {type} {
    set mountPoint [_typeToMount $type]
    ::vfs::filesystem unmount $mountPoint
}

proc vfs::urltype::_typeToMount {type} {
    set mountPoint "${type}://"
    if {$type == "file"} {
	append mountPoint "/"
    }
    return $mountPoint
}

proc vfs::urltype::handler {type cmd root relative actualpath args} {
    ::vfs::log [list urltype $type $cmd $root $relative $actualpath $args]
    if {[string length $relative]} {
	# Find the highest level path so we can mount it:
	set pathSplit [file split [file join $root $relative]]
	set newRoot [eval [list file join] [lrange $pathSplit 0 1]]
	::vfs::log [list $newRoot $pathSplit]
	# Get the package we will need
	::package require vfs::${type}
	# Mount it.
	::vfs::${type}::Mount $newRoot $newRoot
	# Now we want to find out the right handler
	set typeHandler [::vfs::filesystem info $newRoot]
	# Now we have to rearrange the root/relative for this path
	set wholepath [eval [list file join] $pathSplit]
	set newRelative [string range $wholepath [string length $newRoot] end]
	if {[string index $newRelative 0] == "/"} {
	    set newRelative [string range $newRelative 1 end]
	}
	::vfs::log [list $typeHandler $newRoot $newRelative]
	eval $typeHandler [list $cmd $newRoot $newRelative $actualpath] $args
    } else {
	if {$cmd == "matchindirectory"} {
	    eval [list $cmd $type $root $relative $actualpath] $args
	} else {
	    eval [list $cmd $type $root $relative] $args
	}
    }
}

# Stuff below not very well implemented, but works more or less.

proc vfs::urltype::stat {type root name} {
    ::vfs::log "stat $name"
    if {![string length $name]} {
	return [list type directory size 0 mode 0777 \
	  ino -1 depth 0 name $name atime 0 ctime 0 mtime 0 dev -1 \
	  uid -1 gid -1 nlink 1]
	#return -code error "could not read \"$name\": no such file or directory"
    } else {
	error "Shouldn't get here"
    }
}

proc vfs::urltype::open {type root name mode permissions} {
    ::vfs::log "open $name $mode $permissions"
    # There are no 'files' and everything is read-only
    return -code error "illegal access mode \"$mode\""
}

proc vfs::urltype::access {type root name mode} {
    ::vfs::log "access $name $mode"
    if {![string length $name]} {
	return 1
    } else {
	error "Shouldn't get here!"
    }
}

proc vfs::urltype::matchindirectory {type root path actualpath pattern types} {
    ::vfs::log [list matchindirectory $root $path $actualpath $pattern $types]

    if {![vfs::matchDirectories $types]} { return [list] }

    if {![string length $pattern]} {
	return foo
    }
    
    set res [list]
    set len [string length $root]
    
    foreach m [::vfs::filesystem info] {
	if {[string equal [string range $m 0 [expr {$len -1}]] $root]} {
	    set rest [string range $m $len end]
	    if {[string length $rest]} {
		if {[string match $pattern $rest]} {
		    lappend res "$m"
		}
	    }
	}
    }
    return $res
}

proc vfs::urltype::createdirectory {type root name} {
    ::vfs::log "createdirectory $name"
    # For ftp/http/file types we don't want to allow anything here.
    error ""
}

proc vfs::urltype::removedirectory {type root name recursive} {
    ::vfs::log "removedirectory $name"
    # For ftp/http/file types we don't want to allow anything here.
    error ""
}

proc vfs::urltype::deletefile {type root name} {
    ::vfs::log "deletefile $name"
    # For ftp/http/file types we don't want to allow anything here.
    error ""
}

proc vfs::urltype::fileattributes {type root path args} {
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
	}
    }
}

proc vfs::urltype::utime {type root name actime mtime} {
    ::vfs::log "utime $name"
    # For ftp/http/file types we don't want to allow anything here.
    error ""
}
