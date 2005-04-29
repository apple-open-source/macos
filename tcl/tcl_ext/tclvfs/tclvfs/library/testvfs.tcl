
package provide vfs::test 1.0

package require vfs 1.0

namespace eval vfs::test {}

proc vfs::test::Mount {what local} {
    vfs::filesystem mount $local [list ::vfs::test::handler $what]
    vfs::RegisterMount $local [list ::vfs::test::Unmount]
}

proc vfs::test::Unmount {local} {
    vfs::filesystem unmount $local
}

proc vfs::test::handler {what cmd root relative actualpath args} {
    eval [list $cmd $what $relative] $args
}

# If we implement the commands below, we will have a perfect
# virtual file system.

proc vfs::test::stat {what name} {
    puts "stat $name"
}

proc vfs::test::access {what name mode} {
    puts "access $name $mode"
}

proc vfs::test::open {what name mode permissions} {
    puts "open $name $mode $permissions"
    # return a list of two elements:
    # 1. first element is the Tcl channel name which has been opened
    # 2. second element (optional) is a command to evaluate when
    #    the channel is closed.
    return [list]
}

proc vfs::test::matchindirectory {what path pattern type} {
    puts "matchindirectory $path $pattern $type"
    set res [list]

    if {[::vfs::matchDirectories $type]} {
	# add matching directories to $res
    }
    
    if {[::vfs::matchFiles $type]} {
	# add matching files to $res
    }
    return $res
}

proc vfs::test::createdirectory {what name} {
    puts "createdirectory $name"
}

proc vfs::test::removedirectory {what name recursive} {
    puts "removedirectory $name"
}

proc vfs::test::deletefile {what name} {
    puts "deletefile $name"
}

proc vfs::test::fileattributes {what args} {
    puts "fileattributes $args"
    switch -- [llength $args] {
	0 {
	    # list strings
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

proc vfs::test::utime {what name actime mtime} {
    puts "utime $name"
}
