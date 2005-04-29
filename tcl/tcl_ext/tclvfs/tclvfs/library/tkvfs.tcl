
package provide vfs::tk 0.5

package require vfs 1.0

# Thanks to jcw for the idea here.  This is a 'file system' which
# is actually a representation of the Tcl command namespace hierarchy.
# Namespaces are directories, and procedures are files.  Tcl allows
# procedures with the same name as a namespace, which are hidden in
# a filesystem representation.

namespace eval vfs::tk {}

proc vfs::tk::Mount {tree local} {
    if {![winfo exists $tree]} {
	return -code error "No such window $tree"
    }
    ::vfs::log "tk widget hierarchy $tree mounted at $local"
    if {$tree == "."} { set tree "" }
    vfs::filesystem mount $local [list vfs::tk::handler $tree]
    vfs::RegisterMount $local [list vfs::tk::Unmount]
    return $local
}

proc vfs::tk::Unmount {local} {
    vfs::filesystem unmount $local
}

proc vfs::tk::handler {widg cmd root relative actualpath args} {
    regsub -all / $relative . relative
    if {$cmd == "matchindirectory"} {
	eval [list $cmd $widg $relative $actualpath] $args
    } else {
	eval [list $cmd $widg $relative] $args
    }
}

# If we implement the commands below, we will have a perfect
# virtual file system for namespaces.

proc vfs::tk::stat {widg name} {
    ::vfs::log "stat $name"
    if {![winfo exists ${widg}.${name}]} {
	return -code error "could not read \"$name\": no such file or directory"
    }
    set len [llength [winfo children ${widg}.${name}]]
    if {$len || ([winfo class $widg.$name] == "Frame")} {
	return [list type directory size $len mode 0777 \
	  ino -1 depth 0 name $name atime 0 ctime 0 mtime 0 dev -1 \
	  uid -1 gid -1 nlink 1]
    } else {
	return [list type file size 0 atime 0 ctime 0 mtime 0]
    }
}

proc vfs::tk::access {widg name mode} {
    ::vfs::log "access $name $mode"
    if {[winfo exists ${widg}.${name}]} {
	return 1
    } else {
	error "No such file"
    }
}

proc vfs::tk::open {widg name mode permissions} {
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
	    puts -nonewline $nfd [_generate ${widg}.${name}]
	    fconfigure $nfd -translation auto
	    seek $nfd 0
	    return [list $nfd]
	}
	default {
	    return -code error "illegal access mode \"$mode\""
	}
    }
}

proc vfs::tk::_generate {p} {
    lappend a [string tolower [winfo class $p]]
    lappend a $p
    foreach arg [$p configure] {
	set item [lindex $arg 0]
	lappend a $item [$p cget $item]
    }
    return $a
}

proc vfs::tk::matchindirectory {widg path actualpath pattern type} {
    ::vfs::log [list matchindirectory $widg $path $actualpath $pattern $type]
    set res [list]

    if {$widg == "" && $path == ""} {
	set wp ""
    } else {
	set wp $widg.$path
    }
    if {$wp == ""} { set wpp "." } else { set wpp $wp }
    set l [string length $wp]
    
    if {$type == 0} {
	foreach ch [winfo children $wpp] {
	    if {[string match $pattern [string range $ch $l end]]} {
		lappend res $ch
	    }
	}
    } else {
	if {[::vfs::matchDirectories $type]} {
	    # add matching directories to $res
	    if {[string length $pattern]} {
		foreach ch [winfo children $wpp] {
		    if {[string match $pattern [string range $ch $l end]]} {
			if {[llength [winfo children $ch]]} {
			    lappend res $ch
			}
		    }
		}
	    } else {
		if {[string match $pattern $wpp]} {
		    if {[llength [winfo children $wpp]]} {
			lappend res $wpp
		    }
		}
	    }
	}
	if {[::vfs::matchFiles $type]} {
	    # add matching files to $res
	    if {[string length $pattern]} {
		foreach ch [winfo children $wpp] {
		    if {[string match $pattern [string range $ch $l end]]} {
			if {![llength [winfo children $ch]]} {
			    lappend res $ch
			}
		    }
		}
	    } else {
		if {[string match $pattern $wpp]} {
		    if {![llength [winfo children $wpp]]} {
			lappend res $wpp
		    }
		}
	    }
	}
    }
    
    set realres [list]
    set l [expr {1 + [string length $wp]}]
    foreach r $res {
	lappend realres [file join ${actualpath} [string range $r $l end]]
    }
    #::vfs::log $realres
    
    return $realres
}

proc vfs::tk::createdirectory {widg name} {
    ::vfs::log "createdirectory $name"
    frame ${widg}.${name}
}

proc vfs::tk::removedirectory {widg name recursive} {
    ::vfs::log "removedirectory $name"
    if {!$recursive} {
	if {[llength [winfo children $widg.$name]]} {
	    vfs::filesystem posixerror $::vfs::posix(ENOTEMPTY)
	}
    }
    destroy $widg.$name
}

proc vfs::tk::deletefile {widg name} {
    ::vfs::log "deletefile $name"
    destroy $widg.$name
}

proc vfs::tk::fileattributes {widg name args} {
    ::vfs::log "fileattributes $args"
    switch -- [llength $args] {
	0 {
	    # list strings
	    set res {}
	    foreach c [$widg.$name configure] {
		lappend res [lindex $c 0]
	    }
	    return $res
	}
	1 {
	    # get value
	    set index [lindex $args 0]
	    set arg [lindex [lindex [$widg.$name configure] $index] 0]
	    return [$widg.$name cget $arg]
	}
	2 {
	    # set value
	    set index [lindex $args 0]
	    set val [lindex $args 1]
	    set arg [lindex [lindex [$widg.$name configure] $index] 0]
	    return [$widg.$name configure $arg $val]
	}
    }
}

proc vfs::tk::utime {what name actime mtime} {
    ::vfs::log "utime $name"
    error ""
}
