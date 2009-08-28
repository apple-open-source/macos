################################################################################
# This is the first try to provide access to tar-files via
# the vfs-mechanism.
# This file is copied and adapted from zipvfs.tcl
# (and ftpvfs.tcl). The internal structure for the tar-data is stored
# analog to zipvfs so that many functions can be the same as in zipvfs.
#
# Jan 13 2003: Stefan Vogel (stefan.vogel@avinci.de)
# (reformatted to tabsize 8 by Vince).
# 
# TODOs:
# * add writable access (should be easy with tar-files)
# * add gzip-support (?)
# * more testing :-(
################################################################################

package require vfs
package provide vfs::tar 0.91

# Using the vfs, memchan and Trf extensions, we're able
# to write a Tcl-only tar filesystem.  

namespace eval vfs::tar {}

proc vfs::tar::Mount {tarfile local} {
    set fd [vfs::tar::_open [::file normalize $tarfile]]
    vfs::filesystem mount $local [list ::vfs::tar::handler $fd]
    # Register command to unmount
    vfs::RegisterMount $local [list ::vfs::tar::Unmount $fd]
    return $fd
}

proc vfs::tar::Unmount {fd local} {
    vfs::filesystem unmount $local
    vfs::tar::_close $fd
}

proc vfs::tar::handler {tarfd cmd root relative actualpath args} {
    if {$cmd == "matchindirectory"} {
	# e.g. called from "glob *"
	eval [list $cmd $tarfd $relative $actualpath] $args
    } else {
	# called for all other commands: access, stat
	eval [list $cmd $tarfd $relative] $args
    }
}

proc vfs::tar::attributes {tarfd} { return [list "state"] }
proc vfs::tar::state {tarfd args} {
    vfs::attributeCantConfigure "state" "readonly" $args
}

# If we implement the commands below, we will have a perfect
# virtual file system for tar files.
# Completely copied from zipvfs.tcl

proc vfs::tar::matchindirectory {tarfd path actualpath pattern type} {
    # This call to vfs::tar::_getdir handles empty patterns properly as asking
    # for the existence of a single file $path only
    set res [vfs::tar::_getdir $tarfd $path $pattern]
    if {![string length $pattern]} {
	if {![vfs::tar::_exists $tarfd $path]} { return {} }
	set res [list $actualpath]
	set actualpath ""
    }

    set newres [list]
    foreach p [::vfs::matchCorrectTypes $type $res $actualpath] {
	lappend newres [file join $actualpath $p]
    }
    return $newres
}

# return the necessary "array"
proc vfs::tar::stat {tarfd name} {
    vfs::tar::_stat $tarfd $name sb
    array get sb
}

proc vfs::tar::access {tarfd name mode} {
    if {$mode & 2} {
	vfs::filesystem posixerror $::vfs::posix(EROFS)
    }
    # Readable, Exists and Executable are treated as 'exists'
    # Could we get more information from the archive?
    if {[vfs::tar::_exists $tarfd $name]} {
	return 1
    } else {
	error "No such file"
    }
}

proc vfs::tar::open {tarfd name mode permissions} {
    # return a list of two elements:
    # 1. first element is the Tcl channel name which has been opened
    # 2. second element (optional) is a command to evaluate when
    #    the channel is closed.

    switch -- $mode {
	"" -
	"r" {
	    if {![vfs::tar::_exists $tarfd $name]} {
		vfs::filesystem posixerror $::vfs::posix(ENOENT)
	    }

	    vfs::tar::_stat $tarfd $name sb

	    set nfd [vfs::memchan]
	    fconfigure $nfd -translation binary

	    # get the starting point from structure
	    seek $tarfd $sb(start) start
	    vfs::tar::_data $tarfd sb data

	    puts -nonewline $nfd $data

	    fconfigure $nfd -translation auto
	    seek $nfd 0
	    return [list $nfd]
	}
	default {
	    vfs::filesystem posixerror $::vfs::posix(EROFS)
	}
    }
}

proc vfs::tar::createdirectory {tarfd name} {
    vfs::filesystem posixerror $::vfs::posix(EROFS)
    #error "tar-archives are read-only (not implemented)"
}

proc vfs::tar::removedirectory {tarfd name recursive} {
    #::vfs::log "removedirectory $name"
    vfs::filesystem posixerror $::vfs::posix(EROFS)
    #error "tar-archives are read-only (not implemented)"
}

proc vfs::tar::deletefile {tarfd name} {
    vfs::filesystem posixerror $::vfs::posix(EROFS)
    #error "tar-archives are read-only (not implemented)"
}

# don't care about platform-specific attributes
proc vfs::tar::fileattributes {tarfd name args} {
    #::vfs::log "fileattributes $args"
    switch -- [llength $args] {
	0 {
	    # list strings
	    return [list]
	}
	1 {
	    # get value
	    set index [lindex $args 0]
	    return ""
	}
	2 {
	    # set value
	    set index [lindex $args 0]
	    set val [lindex $args 1]
	    vfs::filesystem posixerror $::vfs::posix(EROFS)
	}
    }
}

# set the 'mtime' of a file.
proc vfs::tar::utime {fd path actime mtime} {
    vfs::filesystem posixerror $::vfs::posix(EROFS)
}

#
# tar decoder:
#
# Format of tar file:
# see http://www.gnu.org/manual/tar/html_node/tar_123.html
# "comments" are put into the the arrays for readability
# the fields in aPosixHeader are stored inside a
# 512-byte-block. Not all header-fields are used here.
#
# Here are some excerpts from the above resource for information
# only:
#
# name, linkname, magic, uname, and gname are null-terminated strings.
# All other fileds are zero-filled octal numbers in ASCII.
# Each numeric field of width w contains
#   w minus 2 digits, a space, and a null,
#   except size, and mtime, which do not contain the trailing null

# mtime field is the modification time of the file at the time it was
# archived. It is the ASCII representation of the octal value of the
# last time the file was modified, represented as an integer number
# of seconds since January 1, 1970, 00:00 Coordinated Universal Time


namespace eval vfs::tar {
    set HEADER_SIZE 500
    set BLOCK_SIZE 512

    # fields of header with start/end-index in "comments": length of
    # field in bytes (just for documentation) prefix is the
    # "datatype": s == null-terminated string o == zero-filled octal
    # number (numeric but leave it octal e.g mode) n == numeric -->
    # integer change to decimal) "not used" is marked when the field
    # is not needed anywhere here
    array set aPosixHeader {
	name      {s 0    99}     # 100
	mode      {o 100 107}     # "8   - not used now"
	uid       {n 108 115}     # 8
	gid       {n 116 123}     # 8
	size      {n 124 135}     # 12
	mtime     {n 136 147}     # 12
	chksum    {o 148 155}     # "8   - not used"
	typeflag  {o 156 156}     # 1
	linkname  {s 157 256}     # "100 - not used"
	magic     {s 257 262}     # "6   - not used"
	version   {o 263 264}     # "2   - not used"
	uname     {s 265 296}     # "32  - not used"
	gname     {s 297 328}     # "32  - not used"
	devmajor  {o 329 336}     # "8   - not used"
	devminor  {o 337 344}     # "8   - not used"
	prefix    {o 345 499}     # "155 - not used"
    }

    # just for compatibility with posix-header
    # only DIRTYPE is used
    array set aTypeFlag {
	REGTYPE  0            # "regular file"
	AREGTYPE \000         # "regular file"
	LNKTYPE  1            # link
	SYMTYPE  2            # reserved
	CHRTYPE  3            # "character special"
	BLKTYPE  4            # "block special"
	DIRTYPE  5            # directory
	FIFOTYPE 6            # "FIFO special"
	CONTTYPE 7            # reserved
    }
}

proc vfs::tar::_data {fd arr {varPtr ""}} {
    upvar 1 $arr sb

    if {$varPtr eq ""} {
	seek $fd $sb(size) current
    } else {
	upvar 1 $varPtr data
	set data [read $fd $sb(size)]
    }
}

proc vfs::tar::TOC {fd arr toc} {
    variable aPosixHeader
    variable aTypeFlag
    variable HEADER_SIZE
    variable BLOCK_SIZE
    
    upvar 1 $arr sb
    upvar 1 $toc _toc
    
    set pos 0
    set sb(nitems) 0
    
    # loop through file in blocks of BLOCK_SIZE
    while {![eof $fd]} {
	seek $fd $pos
	set hdr [read $fd $BLOCK_SIZE]

	# read header-fields from block (see aPosixHeader)
	foreach key {name typeflag size mtime uid gid} {
	    set type [lindex $aPosixHeader($key) 0]
	    set positions [lrange $aPosixHeader($key) 1 2]
	    switch $type {
		s {
		    set $key [eval [list string range $hdr] $positions]
		    # cut the trailing Nulls
		    set $key [string range [set $key] 0 [expr [string first "\000" [set $key]]-1]]
		}
		o {
		    # leave it as is (octal value)
		    set $key [eval [list string range $hdr] $positions]
		}
		n {
		    set $key [eval [list string range $hdr] $positions]
		    # change to integer
		    scan [set $key] "%o" $key
		    # if not set, set default-value "0"
		    # (size == "" is not a very good value)
		    if {![string is integer [set $key]] || [set $key] == ""} { set $key 0 }
		}
		default {
		    error "tar::TOC: '$fd' wrong type for header-field: '$type'"
		}
	    }
	}

	# only the last three octals are interesting for mode
	# ignore mode now, should this be added??
	# set mode 0[string range $mode end-3 end]

	# get the increment to the next valid block
	# (ignore file-blocks in between)
	# if size == 0 the minimum incr is 512
	set incr [expr {int(ceil($size/double($BLOCK_SIZE)))*$BLOCK_SIZE+$BLOCK_SIZE}]

	set startPosition [expr {$pos+$BLOCK_SIZE}]
	# make it relative to this working-directory, remove the
	# leading "relative"-paths
	regexp -- {^(?:\.\.?/)*/?(.*)} $name -> name

	if {$name != ""} {
	    incr sb(nitems)
	    set sb($name,start) [expr {$pos+$BLOCK_SIZE}]
	    set sb($name,size) $size
	    set type "file"
	    # the mode should be 0777?? or must be changed to decimal?
	    if {$typeflag == $aTypeFlag(DIRTYPE)} {
		# directory! append this without /
		# leave mode: 0777
		# (else we might not be able to walk through archive)
		set type "directory"
		lappend _toc([string trimright $name "/"]) \
		  name [string trimright $name "/"] \
		  type $type mtime $mtime size $size mode 0777 \
		  ino -1 start $startPosition \
		  depth [llength [file split $name]] \
		  uid $uid gid $gid
	    }
	    lappend _toc($name) \
	      name $name \
	      type $type mtime $mtime size $size mode 0777 \
	      ino -1 start $startPosition depth [llength [file split $name]] \
	      uid $uid gid $gid
	}
	incr pos $incr
    }
    return
}

proc vfs::tar::_open {path} {
    set fd [::open $path]
    
    if {[catch {
	upvar #0 vfs::tar::$fd.toc toc
	fconfigure $fd -translation binary ;#-buffering none
	vfs::tar::TOC $fd sb toc
    } err]} {
	close $fd
	return -code error $err
    }
    
    return $fd
}

proc vfs::tar::_exists {fd path} {
    #::vfs::log "$fd $path"
    if {$path == ""} {
	return 1
    } else {
	upvar #0 vfs::tar::$fd.toc toc
	return [expr {[info exists toc($path)] || [info exists toc([string trimright $path "/"]/)]}]
    }
}

proc vfs::tar::_stat {fd path arr} {
    upvar #0 vfs::tar::$fd.toc toc
    upvar 1 $arr sb

    if { $path == "" || $path == "." } {
	array set sb {
	    type directory mtime 0 size 0 mode 0777 
	    ino -1 depth 0 name ""
	}
    } elseif {![info exists toc($path)] } {
	return -code error "could not read \"$path\": no such file or directory"
    } else {
	array set sb $toc($path)
    }

    # set missing attributes
    set sb(dev) -1
    set sb(nlink) 1
    set sb(atime) $sb(mtime)
    set sb(ctime) $sb(mtime)

    return ""
}

# Treats empty pattern as asking for a particular file only.
# Directly copied from zipvfs.
proc vfs::tar::_getdir {fd path {pat *}} {
    upvar #0 vfs::tar::$fd.toc toc
    
    if { $path == "." || $path == "" } {
	set path $pat
    } else {
	set path [string tolower $path]
	if {$pat != ""} {
	    append path /$pat
	}
    }
    set depth [llength [file split $path]]
    
    if {$depth} {
	set ret {}
	foreach key [array names toc $path] {
	    if {[string index $key end] eq "/"} {
		# Directories are listed twice: both with and without
		# the trailing '/', so we ignore the one with
		continue
	    }
	    array set sb $toc($key)

	    if { $sb(depth) == $depth } {
		if {[info exists toc(${key}/)]} {
		    array set sb $toc(${key}/)
		}
		# remove sb(name) (because == $key)
		lappend ret [file tail $key]
	    }
	    unset sb
	}
	return $ret
    } else {
	# just the 'root' of the zip archive.  This obviously exists and
	# is a directory.
	return [list {}]
    }
}

proc vfs::tar::_close {fd} {
    variable $fd.toc
    unset -nocomplain $fd.toc
    ::close $fd
}
