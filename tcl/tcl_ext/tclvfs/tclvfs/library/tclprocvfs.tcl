
package provide vfs::ns 0.5.1

package require vfs 1.0

# Thanks to jcw for the idea here.  This is a 'file system' which
# is actually a representation of the Tcl command namespace hierarchy.
# Namespaces are directories, and procedures are files.  Tcl allows
# procedures with the same name as a namespace, which are hidden in
# a filesystem representation.

namespace eval vfs::ns {}

proc vfs::ns::Mount {ns local} {
    if {![namespace exists ::$ns]} {
	error "No such namespace"
    }
    ::vfs::log "ns $ns mounted at $local"
    vfs::filesystem mount $local [list vfs::ns::handler $ns]
    vfs::RegisterMount $local [list vfs::ns::Unmount]
    return $local
}

proc vfs::ns::Unmount {local} {
    vfs::filesystem unmount $local
}

proc vfs::ns::handler {ns cmd root relative actualpath args} {
    regsub -all / $relative :: relative
    if {$cmd == "matchindirectory"} {
	eval [list $cmd $ns $relative $actualpath] $args
    } else {
	eval [list $cmd $ns $relative] $args
    }
}

# If we implement the commands below, we will have a perfect
# virtual file system for namespaces.

proc vfs::ns::stat {ns name} {
    ::vfs::log "stat $name"
    if {[namespace exists ::${ns}::${name}]} {
	return [list type directory size 0 mode 0777 \
	  ino -1 depth 0 name $name atime 0 ctime 0 mtime 0 dev -1 \
	  uid -1 gid -1 nlink 1]
    } elseif {[llength [info procs ::${ns}::${name}]]} {
	return [list type file]
    } else {
	return -code error "could not read \"$name\": no such file or directory"
    }
}

proc vfs::ns::access {ns name mode} {
    ::vfs::log "access $name $mode"
    if {[namespace exists ::${ns}::${name}]} {
	return 1
    } elseif {[llength [info procs ::${ns}::${name}]]} {
	if {$mode & 2} {
	    error "read-only"
	}
	return 1
    } else {
	error "No such file"
    }
}

proc vfs::ns::exists {ns name} {
    if {[namespace exists ::${ns}::${name}]} {
	return 1
    } elseif {[llength [info procs ::${ns}::${name}]]} {
	return 1
    } else {
	return 0
    }
}

proc vfs::ns::open {ns name mode permissions} {
    ::vfs::log "open $name $mode $permissions"
    # return a list of two elements:
    # 1. first element is the Tcl channel name which has been opened
    # 2. second element (optional) is a command to evaluate when
    #    the channel is closed.
    switch -- $mode {
	"" -
	"r" {
	    set nfd [vfs::memchan]
	    fconfigure $nfd -translation binary
	    puts -nonewline $nfd [_generate ::${ns}::${name}]
	    fconfigure $nfd -translation auto
	    seek $nfd 0
	    return [list $nfd]
	}
	default {
	    return -code error "illegal access mode \"$mode\""
	}
    }
}

proc vfs::ns::_generate {p} {
    lappend a proc $p
    set argslist [list]
    foreach arg [info args $p] {
	if {[info default $p $arg v]} {
	    lappend argslist [list $arg $v]
	} else {
	    lappend argslist $arg
	}
    }
    lappend a $argslist [info body $p]
}

proc vfs::ns::matchindirectory {ns path actualpath pattern type} {
    ::vfs::log "matchindirectory $path $actualpath $pattern $type"
    set res [list]

    set ns ::[string trim $ns :]
    set nspath ${ns}::${path}
    set slash 1
    if {[::vfs::matchDirectories $type]} {
	# add matching directories to $res
	if {[string length $pattern]} {
	    eval [linsert [namespace children $nspath $pattern] 0 lappend res]
	} elseif {[namespace exists $nspath]} {
	    lappend res $nspath
	}
    }

    if {[::vfs::matchFiles $type]} {
	# add matching files to $res
	if {[string length $pattern]} {
	    eval [linsert [info procs ${nspath}::$pattern] 0 lappend res]
	} elseif {[llength [info procs $nspath]]} {
 	    lappend res $nspath
	    set slash 0
	}
    }

    # There is a disconnect between 8.4 and 8.5 with the / handling
    # Make sure actualpath gets just one trailing /
    if {$slash && ![string match */ $actualpath]} { append actualpath / }

    set realres [list]
    foreach r $res {
	regsub "^(::)?${ns}(::)?${path}(::)?" $r $actualpath rr
	lappend realres $rr
    }
    #::vfs::log $realres

    return $realres
}

proc vfs::ns::createdirectory {ns name} {
    ::vfs::log "createdirectory $name"
    namespace eval ::${ns}::${name} {}
}

proc vfs::ns::removedirectory {ns name recursive} {
    ::vfs::log "removedirectory $name"
    namespace delete ::${ns}::${name}
}

proc vfs::ns::deletefile {ns name} {
    ::vfs::log "deletefile $name"
    rename ::${ns}::${name} {}
}

proc vfs::ns::fileattributes {ns name args} {
    ::vfs::log "fileattributes $args"
    switch -- [llength $args] {
	0 {
	    # list strings
	    return [list -args -body]
	}
	1 {
	    # get value
	    set index [lindex $args 0]
	    switch -- $index {
		0 {
		    ::info args ::${ns}::${name}
		}
		1 {
		    ::info body ::${ns}::${name}
		}
	    }
	}
	2 {
	    # set value
	    set index [lindex $args 0]
	    set val [lindex $args 1]
	    switch -- $index {
		0 {
		    error "read-only"
		}
		1 {
		    error "unimplemented"
		}
	    }
	}
    }
}

proc vfs::ns::utime {what name actime mtime} {
    ::vfs::log "utime $name"
    error ""
}
