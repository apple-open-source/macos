# mk4vfs.tcl -- Mk4tcl Virtual File System driver
# Copyright (C) 1997-2003 Sensus Consulting Ltd. All Rights Reserved.
# Matt Newman <matt@sensus.org> and Jean-Claude Wippler <jcw@equi4.com>
#
# $Id: mk4vfs.tcl,v 1.42 2008/12/03 02:16:19 hobbs Exp $
#
# 05apr02 jcw	1.3	fixed append mode & close,
#			privatized memchan_handler
#			added zip, crc back in
# 28apr02 jcw	1.4	reorged memchan and pkg dependencies
# 22jun02 jcw	1.5	fixed recursive dir deletion
# 16oct02 jcw	1.6	fixed periodic commit once a change is made
# 20jan03 jcw	1.7	streamed zlib decompress mode, reduces memory usage
# 01feb03 jcw	1.8	fix mounting a symlink, cleanup mount/unmount procs
# 04feb03 jcw	1.8	whoops, restored vfs::mk4::Unmount logic
# 17mar03 jcw	1.9	start with mode translucent or readwrite
# 18oct05 jcw	1.10	add fallback to MK Compatible Lite driver (vfs::mkcl)

# Removed provision of the backward compatible name. Moved to separate
# file/package.
package provide vfs::mk4 1.10.1
package require vfs

# need this so init failure in interactive mode does not mess up errorInfo
if {[info exists env(VFS_DEBUG)] && [info commands history] == ""} {
    proc history {args} {}
}

namespace eval vfs::mk4 {
    proc Mount {mkfile local args} {
        # 2005-10-19 switch to MK Compatible Lite driver if there is no Mk4tcl 
	if {[catch { package require Mk4tcl }]} {
	  package require vfs::mkcl
	  return [eval [linsert $args 0 vfs::mkcl::Mount $mkfile $local]]
	}

	if {$mkfile != ""} {
	  # dereference a symlink, otherwise mounting on it fails (why?)
	  catch {
	    set mkfile [file join [file dirname $mkfile] \
	    			  [file readlink $mkfile]]
	  }
	  set mkfile [file normalize $mkfile]
	}
	set db [eval [list ::mk4vfs::_mount $mkfile] $args]
	::vfs::filesystem mount $local [list ::vfs::mk4::handler $db]
	::vfs::RegisterMount $local [list ::vfs::mk4::Unmount $db]
	return $db
    }

    proc Unmount {db local} {
	vfs::filesystem unmount $local
	::mk4vfs::_umount $db
    }

    proc attributes {db} { return [list "state" "commit"] }
    
    # Can use this to control commit/nocommit or whatever.
    # I'm not sure yet of what functionality jcw needs.
    proc commit {db args} {
	switch -- [llength $args] {
	    0 {
		if {$::mk4vfs::v::mode($db) == "readonly"} {
		    return 0
		} else {
		    # To Do: read the commit state
		    return 1
		}
	    }
	    1 {
		set val [lindex $args 0]
		if {$val != 0 && $val != 1} {
		    return -code error \
		      "invalid commit value $val, must be 0,1"
		}
		# To Do: set the commit state.
	    }
	    default {
		return -code error "Wrong num args"
	    }
	}
    }
    
    proc state {db args} {
	switch -- [llength $args] {
	    0 {
		return $::mk4vfs::v::mode($db)
	    }
	    1 {
		set val [lindex $args 0]
		if {[lsearch -exact [::vfs::states] $val] == -1} {
		    return -code error \
		      "invalid state $val, must be one of: [vfs::states]"
		}
		set ::mk4vfs::v::mode($db) $val
		::mk4vfs::setupCommits $db
	    }
	    default {
		return -code error "Wrong num args"
	    }
	}
    }
    
    proc handler {db cmd root relative actualpath args} {
	#puts stderr "handler: $db - $cmd - $root - $relative - $actualpath - $args"
	if {$cmd == "matchindirectory"} {
	    eval [list $cmd $db $relative $actualpath] $args
	} elseif {$cmd == "fileattributes"} {
	    eval [list $cmd $db $root $relative] $args
	} else {
	    eval [list $cmd $db $relative] $args
	}
    }

    proc utime {db path actime modtime} {
	::mk4vfs::stat $db $path sb
	
	if { $sb(type) == "file" } {
	    mk::set $sb(ino) date $modtime
	}
    }

    proc matchindirectory {db path actualpath pattern type} {
	set newres [list]
	if {![string length $pattern]} {
	    # check single file
	    if {[catch {access $db $path 0}]} {
		return {}
	    }
	    set res [list $actualpath]
	    set actualpath ""
	} else {
	    set res [::mk4vfs::getdir $db $path $pattern]
	}
	foreach p [::vfs::matchCorrectTypes $type $res $actualpath] {
	    lappend newres [file join $actualpath $p]
	}
	return $newres
    }

    proc stat {db name} {
	::mk4vfs::stat $db $name sb

	set sb(ino) 0
	array get sb
    }

    proc access {db name mode} {
	if {$mode & 2} {
	    if {$::mk4vfs::v::mode($db) == "readonly"} {
		vfs::filesystem posixerror $::vfs::posix(EROFS)
	    }
	}
	# We can probably do this more efficiently, can't we?
	::mk4vfs::stat $db $name sb
    }

    proc open {db file mode permissions} {
	# return a list of two elements:
	# 1. first element is the Tcl channel name which has been opened
	# 2. second element (optional) is a command to evaluate when
	#  the channel is closed.
	switch -glob -- $mode {
	    {}  -
	    r {
		::mk4vfs::stat $db $file sb

		if { $sb(csize) != $sb(size) } {
		    if {$::mk4vfs::zstreamed} {
		      set fd [mk::channel $sb(ino) contents r]
		      fconfigure $fd -translation binary
		      set fd [vfs::zstream decompress $fd $sb(csize) $sb(size)]
		    } else {
		      set fd [vfs::memchan]
		      fconfigure $fd -translation binary
		      set s [mk::get $sb(ino) contents]
		      puts -nonewline $fd [vfs::zip -mode decompress $s]

		      fconfigure $fd -translation auto
		      seek $fd 0
		    }
		} elseif { $::mk4vfs::direct } {
		    set fd [vfs::memchan]
		    fconfigure $fd -translation binary
		    puts -nonewline $fd [mk::get $sb(ino) contents]

		    fconfigure $fd -translation auto
		    seek $fd 0
		} else {
		    set fd [mk::channel $sb(ino) contents r]
		}
		return [list $fd]
	    }
	    a {
		if {$::mk4vfs::v::mode($db) == "readonly"} {
		    vfs::filesystem posixerror $::vfs::posix(EROFS)
		}
		if { [catch {::mk4vfs::stat $db $file sb }] } {
		    # Create file
		    ::mk4vfs::stat $db [file dirname $file] sb
		    set tail [file tail $file]
		    set fview $sb(ino).files
		    if {[info exists mk4vfs::v::fcache($fview)]} {
			lappend mk4vfs::v::fcache($fview) $tail
		    }
		    set now [clock seconds]
		    set sb(ino) [mk::row append $fview \
			    name $tail size 0 date $now ]

		    if { [string match *z* $mode] || $mk4vfs::compress } {
			set sb(csize) -1  ;# HACK - force compression
		    } else {
			set sb(csize) 0
		    }
		}

		set fd [vfs::memchan]
		fconfigure $fd -translation binary
		set s [mk::get $sb(ino) contents]

		if { $sb(csize) != $sb(size) && $sb(csize) > 0 } {
		    append mode z
		    puts -nonewline $fd [vfs::zip -mode decompress $s]
		} else {
		    if { $mk4vfs::compress } { append mode z }
		    puts -nonewline $fd $s
		    #set fd [mk::channel $sb(ino) contents a]
		}
		fconfigure $fd -translation auto
		seek $fd 0 end
		return [list $fd [list mk4vfs::do_close $db $fd $mode $sb(ino)]]
	    }
	    w*  {
		if {$::mk4vfs::v::mode($db) == "readonly"} {
		    vfs::filesystem posixerror $::vfs::posix(EROFS)
		}
		if { [catch {::mk4vfs::stat $db $file sb }] } {
		    # Create file
		    ::mk4vfs::stat $db [file dirname $file] sb
		    set tail [file tail $file]
		    set fview $sb(ino).files
		    if {[info exists mk4vfs::v::fcache($fview)]} {
			lappend mk4vfs::v::fcache($fview) $tail
		    }
		    set now [clock seconds]
		    set sb(ino) [mk::row append $fview \
			    name $tail size 0 date $now ]
		}

		if { [string match *z* $mode] || $mk4vfs::compress } {
		    append mode z
		    set fd [vfs::memchan]
		} else {
		    set fd [mk::channel $sb(ino) contents w]
		}
		return [list $fd [list mk4vfs::do_close $db $fd $mode $sb(ino)]]
	    }
	    default   {
		error "illegal access mode \"$mode\""
	    }
	}
    }

    proc createdirectory {db name} {
	mk4vfs::mkdir $db $name
    }

    proc removedirectory {db name recursive} {
	mk4vfs::delete $db $name $recursive
    }

    proc deletefile {db name} {
	mk4vfs::delete $db $name
    }

    proc fileattributes {db root relative args} {
	switch -- [llength $args] {
	    0 {
		# list strings
		return [::vfs::listAttributes]
	    }
	    1 {
		# get value
		set index [lindex $args 0]
		return [::vfs::attributesGet $root $relative $index]

	    }
	    2 {
		# set value
		if {$::mk4vfs::v::mode($db) == "readonly"} {
		    vfs::filesystem posixerror $::vfs::posix(EROFS)
		}
		set index [lindex $args 0]
		set val [lindex $args 1]
		return [::vfs::attributesSet $root $relative $index $val]
	    }
	}
    }
}

namespace eval mk4vfs {
    variable compress 1     ;# HACK - needs to be part of "Super-Block"
    variable flush    5000  ;# Auto-Commit frequency
    variable direct   0	    ;# read through a memchan, or from Mk4tcl if zero
    variable zstreamed 0    ;# decompress on the fly (needs zlib 1.1)

    namespace eval v {
	variable seq      0
	variable mode	    ;# array key is db, value is mode 
	             	     # (readwrite/translucent/readonly)
	variable timer	    ;# array key is db, set to afterid, periodicCommit

	array set cache {}
	array set fcache {}

	array set mode {exe translucent}
    }

    proc init {db} {
	mk::view layout $db.dirs \
		{name:S parent:I {files {name:S size:I date:I contents:M}}}

	if { [mk::view size $db.dirs] == 0 } {
	    mk::row append $db.dirs name <root> parent -1
	}
    }

    proc _mount {{file ""} args} {
	set db mk4vfs[incr v::seq]

	if {$file == ""} {
	    mk::file open $db
	    init $db
	    set v::mode($db) "translucent"
	} else {
	    eval [list mk::file open $db $file] $args
	    
	    init $db
	    
	    set mode 0
	    foreach arg $args {
		switch -- $arg {
		    -readonly   { set mode 1 }
		    -nocommit   { set mode 2 }
		}
	    }
	    if {$mode == 0} {
		periodicCommit $db
	    }
	    set v::mode($db) [lindex {translucent readwrite readwrite} $mode]
	}
	return $db
    }

    proc periodicCommit {db} {
	variable flush
	set v::timer($db) [after $flush [list ::mk4vfs::periodicCommit $db]]
	mk::file commit $db
	return ;# 2005-01-20 avoid returning a value
    }

    proc _umount {db args} {
	catch {after cancel $v::timer($db)}
	array unset v::mode $db
	array unset v::timer $db
	array unset v::cache $db,*
	array unset v::fcache $db.*
	mk::file close $db
    }

    proc stat {db path {arr ""}} {
	set sp [::file split $path]
	set tail [lindex $sp end]

	set parent 0
	set view $db.dirs
	set type directory

	foreach ele [lrange $sp 0 end-1] {
	    if {[info exists v::cache($db,$parent,$ele)]} {
		set parent $v::cache($db,$parent,$ele)
	    } else {
		set row [mk::select $view -count 1 parent $parent name $ele]
		if { $row == "" } {
		    vfs::filesystem posixerror $::vfs::posix(ENOENT)
		}
		set v::cache($db,$parent,$ele) $row
		set parent $row
	    }
	}
	
	# Now check if final comp is a directory or a file
	# CACHING is required - it can deliver a x15 speed-up!
	
	if { [string equal $tail "."] || [string equal $tail ":"] \
	  || [string equal $tail ""] } {
	    set row $parent

	} elseif { [info exists v::cache($db,$parent,$tail)] } {
	    set row $v::cache($db,$parent,$tail)
	} else {
	    # File?
	    set fview $view!$parent.files
	    # create a name cache of files in this directory
	    if {![info exists v::fcache($fview)]} {
		# cache only a limited number of directories
		if {[array size v::fcache] >= 10} {
		    array unset v::fcache *
		}
		set v::fcache($fview) {}
		mk::loop c $fview {
		    lappend v::fcache($fview) [mk::get $c name]
		}
	    }
	    set row [lsearch -exact $v::fcache($fview) $tail]
	    #set row [mk::select $fview -count 1 name $tail]
	    #if {$row == ""} { set row -1 }
	    if { $row != -1 } {
		set type file
		set view $view!$parent.files
	    } else {
		# Directory?
		set row [mk::select $view -count 1 parent $parent name $tail]
		if { $row != "" } {
		    set v::cache($db,$parent,$tail) $row
		} else { 
		    vfs::filesystem posixerror $::vfs::posix(ENOENT)
		}
	    }
	}
 
        if {![string length $arr]} {
            # The caller doesn't need more detailed information.
            return 1
        }
 
	set cur $view!$row

	upvar 1 $arr sb

	set sb(type)    $type
	set sb(view)    $view
	set sb(ino)     $cur

	if { [string equal $type "directory"] } {
	    set sb(atime) 0
	    set sb(ctime) 0
	    set sb(gid)   0
	    set sb(mode)  0777
	    set sb(mtime) 0
	    set sb(nlink) [expr { [mk::get $cur files] + 1 }]
	    set sb(size)  0
	    set sb(csize) 0
	    set sb(uid)   0
	} else {
	    set mtime   [mk::get $cur date]
	    set sb(atime) $mtime
	    set sb(ctime) $mtime
	    set sb(gid)   0
	    set sb(mode)  0777
	    set sb(mtime) $mtime
	    set sb(nlink) 1
	    set sb(size)  [mk::get $cur size]
	    set sb(csize) [mk::get $cur -size contents]
	    set sb(uid)   0
	}
    }

    proc do_close {db fd mode cur} {
	if {![regexp {[aw]} $mode]} {
	    error "mk4vfs::do_close called with bad mode: $mode"
	}

	mk::set $cur size -1 date [clock seconds]
	flush $fd
	if { [string match *z* $mode] } {
	    fconfigure $fd -translation binary
	    seek $fd 0
	    set data [read $fd]
	    set cdata [vfs::zip -mode compress $data]
	    set len [string length $data]
	    set clen [string length $cdata]
	    if { $clen < $len } {
		mk::set $cur size $len contents $cdata
	    } else {
		mk::set $cur size $len contents $data
	    }
	} else {
	    mk::set $cur size [mk::get $cur -size contents]
	}
	# 16oct02 new logic to start a periodic commit timer if not yet running
	setupCommits $db
	return ""
    }

    proc setupCommits {db} {
	if {$v::mode($db) eq "readwrite" && ![info exists v::timer($db)]} {
	    periodicCommit $db
	    mk::file autocommit $db
	}
    }

    proc mkdir {db path} {
	if {$v::mode($db) == "readonly"} {
	    vfs::filesystem posixerror $::vfs::posix(EROFS)
	}
	set sp [::file split $path]
	set parent 0
	set view $db.dirs

	set npath {}
	# This actually does more work than is needed. Tcl's
	# vfs only requires us to create the last piece, and
	# Tcl already knows it is not a file.
	foreach ele $sp {
	    set npath [file join $npath $ele]

	    if {![catch {stat $db $npath sb}] } {
		if { $sb(type) != "directory" } {
		    vfs::filesystem posixerror $::vfs::posix(EROFS)
		}
		set parent [mk::cursor position sb(ino)]
		continue
	    }
	    #set parent [mk::cursor position sb(ino)]
	    set cur [mk::row append $view name $ele parent $parent]
	    set parent [mk::cursor position cur]
	}
	setupCommits $db
	return ""
    }

    proc getdir {db path {pat *}} {
	if {[catch { stat $db $path sb }] || $sb(type) != "directory" } {
	    return
	}

	# Match directories
	set parent [mk::cursor position sb(ino)] 
	foreach row [mk::select $sb(view) parent $parent -glob name $pat] {
	    set hits([mk::get $sb(view)!$row name]) 1
	}
	# Match files
	set view $sb(view)!$parent.files
	foreach row [mk::select $view -glob name $pat] {
	    set hits([mk::get $view!$row name]) 1
	}
	return [lsort [array names hits]]
    }

    proc mtime {db path time} {
	if {$v::mode($db) == "readonly"} {
	    vfs::filesystem posixerror $::vfs::posix(EROFS)
	}
	stat $db $path sb
	if { $sb(type) == "file" } {
	    mk::set $sb(ino) date $time
	}
	return $time
    }

    proc delete {db path {recursive 0}} {
	#puts stderr "mk4delete db $db path $path recursive $recursive"
	if {$v::mode($db) == "readonly"} {
	    vfs::filesystem posixerror $::vfs::posix(EROFS)
	}
	stat $db $path sb
	if {$sb(type) == "file" } {
	    mk::row delete $sb(ino)
	    if {[regexp {(.*)!(\d+)} $sb(ino) - v r] \
		    && [info exists v::fcache($v)]} {
		set v::fcache($v) [lreplace $v::fcache($v) $r $r]
	    }
	} else {
	    # just mark dirs as deleted
	    set contents [getdir $db $path *]
	    if {$recursive} {
		# We have to delete these manually, else
		# they (or their cache) may conflict with
		# something later
		foreach f $contents {
		    delete $db [file join $path $f] $recursive
		}
	    } else {
		if {[llength $contents]} {
		    vfs::filesystem posixerror $::vfs::posix(ENOTEMPTY)
		}
	    }
	    array unset v::cache \
		    "$db,[mk::get $sb(ino) parent],[file tail $path]"
	    
	    # flag with -99, because parent -1 is not reserved for the root dir
	    # deleted entries never get re-used, should be cleaned up one day
	    mk::set $sb(ino) parent -99 name ""
	    # get rid of file entries to release the space in the datafile
	    mk::view size $sb(ino).files 0
	}
	setupCommits $db
	return ""
    }
}

# DEPRECATED - please don't use.

namespace eval mk4vfs {

    namespace export mount umount

    # deprecated, use vfs::mk4::Mount (first two args are reversed!)
    proc mount {local mkfile args} {
	uplevel [list ::vfs::mk4::Mount $mkfile $local] $args
    }

    # deprecated: unmounts, but only if vfs was mounted on itself
    proc umount {local} {
	foreach {db path} [mk::file open] {
	    if {[string equal $local $path]} {
		vfs::filesystem unmount $local
		_umount $db
		return
	    }
	}
	tclLog "umount $local? [mk::file open]"
    }
}
